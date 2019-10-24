#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <poll.h>
#include <limits.h>
#include <playerctl/playerctl.h>
#include <mainloop.h>
#include "shared.h"
#include "music.h"
#include "display.h"
#include "banner.h"

#define MAX_PLAYER_COUNT 16

struct mod_music {
	struct display* dpy;
	PlayerctlPlayerManager* manager;
	enum music_state state;
	char songbuf[256]; // "artist - title"
	PlayerctlPlayer* player; // selected player
	int sid_metadata; // for signal disconnecting
	struct ml_custom* glib_source;
};

static gboolean status_callback(PlayerctlPlayer* player,
	PlayerctlPlaybackStatus status, struct mod_music* pc);
static gboolean metadata_callback(PlayerctlPlayer* player,
	GVariant* metadata, struct mod_music* pc);

enum music_state convert_state(PlayerctlPlaybackStatus status) {
	switch(status) {
		case PLAYERCTL_PLAYBACK_STATUS_PLAYING:
			return music_state_playing;
		case PLAYERCTL_PLAYBACK_STATUS_PAUSED:
			return music_state_paused;
		case PLAYERCTL_PLAYBACK_STATUS_STOPPED:
			return music_state_stopped;
		default:
			return music_state_none;
	}
}

static void reload_state(struct mod_music* pc) {
	if(!pc->player) {
		snprintf(pc->songbuf, sizeof(pc->songbuf), "-");
		pc->state = 1;
		return;
	}

	GError* error = NULL;

	// state
	PlayerctlPlaybackStatus status = 0;
    g_object_get(pc->player, "playback-status", &status, NULL);
	pc->state = convert_state(status);

	// songbuf
	gchar* gartist = playerctl_player_get_artist(pc->player, &error);
	if(error != NULL) {
		printf("Can't get artist: %s\n", error->message);
	}

	gchar* gtitle = playerctl_player_get_title(pc->player, &error);
	if(error != NULL) {
		printf("Can't get title: %s\n", error->message);
	}

	const char* artist = gartist;
	const char* title = gtitle;
	if(!artist) artist = "<unknown>";
	if(!title) title = "<unknown>";

	snprintf(pc->songbuf, sizeof(pc->songbuf), "%s - %s", artist, title);
	g_free(gartist);
	g_free(gtitle);

	// TODO: only call this if state really changed.
	// shouldn't be too hard to compare.
	// Needed since e.g. mpd has something like seektime as metadata,
	// which changes *really* often.
	display_redraw(pc->dpy, banner_music);
	// printf("state: %d, song: %s\n", (int) pc->state, pc->songbuf);
}

static void change_player(struct mod_music* pc, PlayerctlPlayer* player) {
	if(pc->player) {
		g_signal_handler_disconnect(pc->player, pc->sid_metadata);
	}

	pc->player = player;
	if(pc->player) {
		pc->sid_metadata = g_signal_connect(G_OBJECT(player),
			"metadata", G_CALLBACK(metadata_callback), pc);
	}
	reload_state(pc);
}

// Selects just the first playing player.
// Otherwise will choose mpd if available.
// Otherwise will just choose the first player.
static void select_player(struct mod_music* pc) {
	PlayerctlPlayer* found = NULL;

	GList* players = NULL;
    g_object_get(pc->manager, "players", &players, NULL);
    for(GList* l = players; l != NULL; l = l->next) {
		PlayerctlPlayer* player = l->data;

		PlayerctlPlaybackStatus status;
		g_object_get(player, "playback-status", &status, NULL);
		if(status == PLAYERCTL_PLAYBACK_STATUS_PLAYING) {
			found = player;
			break;
		}

		gchar* name;
		g_object_get(player, "player-name", &name, NULL);
		if(strcmp(name, "mpd") == 0) {
			found = player;
		} else if(!found) {
			found = player;
		}

		free(name);
	}

	change_player(pc, found);
}

static gboolean status_callback(PlayerctlPlayer* player,
		PlayerctlPlaybackStatus status, struct mod_music* pc) {
	// TODO: we could additionally always switch to the currently
	// playing player if there is one.
	// Only relevant when multiple players are playing which shouldn't
	// be the case, ever, anyways.
	if(player == pc->player) {
		pc->state = convert_state(status);
		// printf("state: %d, song: %s\n", (int) pc->state, pc->songbuf);
		if(status == PLAYERCTL_PLAYBACK_STATUS_STOPPED) {
			select_player(pc);
		} else {
			display_redraw(pc->dpy, banner_music);
		}
	} else if(status == PLAYERCTL_PLAYBACK_STATUS_PLAYING && (pc->state != music_state_playing)) {
		change_player(pc, player);
	}

	return true;
}

static gboolean metadata_callback(PlayerctlPlayer* player, GVariant* metadata,
		struct mod_music* pc) {
	assert(player == pc->player);
	reload_state(pc);
	display_redraw(pc->dpy, banner_music);
	return true;
}

static void name_appeared_callback(PlayerctlPlayerManager* manager,
		PlayerctlPlayerName* name, struct mod_music* pc) {
	GError* error;
	PlayerctlPlayer* player = playerctl_player_new_from_name(name, &error);
	if(error != NULL) {
		printf("Can't create player: %s\n", error->message);
		return;
	}

	g_signal_connect(G_OBJECT(player), "playback-status",
		G_CALLBACK(status_callback), pc);
	playerctl_player_manager_manage_player(manager, player);
	g_object_unref(player); // will remain valid since manager refs it

	if(!pc->player) {
		change_player(pc, player);
	}
}

static void player_vanished_callback(PlayerctlPlayerManager* manager,
		PlayerctlPlayer* player, struct mod_music* pc) {
	if(pc->player == player) {
		pc->player = NULL;
		pc->sid_metadata = -1;
		select_player(pc);
	}
}

static void glib_prepare(struct ml_custom* c) {
	GMainContext* ctx = ml_custom_get_data(c);
	gint prio;
	g_main_context_prepare(ctx, &prio);
}

// TODO: we depend here on the fact that pollfd and GPollFD have
// the same memory layout. I'm not sure if this is actually guaranteed
// to be true, they have the same members though (see pollfd as described
// by posix) so a compiler that gives them randomly different layouts
// would be really weird i guess? (maybe C even guarantees that
// two structs with the same members must have the same layout?
// strict aliasing could still mess with us here, during link-time
// optimization or something?)
//
// we could otherwise make the custom userdata a cached GPollFD array
// that is realloc'd when needed. Probably cleaner/more portable version
static unsigned glib_query(struct ml_custom* c, struct pollfd* fds,
		unsigned n_fds, int* timeout) {
	const gint prio = INT_MAX;
	GMainContext* ctx = ml_custom_get_data(c);
	unsigned ret = g_main_context_query(ctx, prio, timeout, (GPollFD*) fds, n_fds);
	return ret;
}

static void glib_dispatch(struct ml_custom* c, struct pollfd* fds, unsigned n_fds) {
	GMainContext* ctx = ml_custom_get_data(c);
	g_main_context_check(ctx, INT_MAX, (GPollFD*) fds, n_fds);
	g_main_context_dispatch(ctx);
}

static const struct ml_custom_impl glib_custom_impl = {
	.prepare = glib_prepare,
	.query = glib_query,
	.dispatch = glib_dispatch
};

struct mod_music* mod_music_create(struct display* dpy) {
	GError* error = NULL;

	struct mod_music* pc = calloc(1, sizeof(*pc));
	pc->dpy = dpy;
	pc->manager = playerctl_player_manager_new(&error);
	if(error != NULL) {
		printf("Can't create playerctl manager: %s\n", error->message);
		free(pc);
		return NULL;
	}

	g_signal_connect(PLAYERCTL_PLAYER_MANAGER(pc->manager), "name-appeared",
		G_CALLBACK(name_appeared_callback), pc);
	g_signal_connect(PLAYERCTL_PLAYER_MANAGER(pc->manager), "player-vanished",
		G_CALLBACK(player_vanished_callback), pc);

	// initial player iteration
	// make sure every player has a status change signal connected
	GList* available_players = NULL;
    g_object_get(pc->manager, "player-names", &available_players, NULL);
    for(GList* l = available_players; l != NULL; l = l->next) {
		PlayerctlPlayerName* name = l->data;

		PlayerctlPlayer* player = playerctl_player_new_from_name(name, &error);
		if(error != NULL) {
			printf("Can't create player: %s\n", error->message);
			continue;
		}

		g_signal_connect(G_OBJECT(player), "playback-status",
			G_CALLBACK(status_callback), pc);
		playerctl_player_manager_manage_player(pc->manager, player);
		g_object_unref(player);
	}

	// initial selection
	select_player(pc);

	// we really don't want to use the glib main loop so we integrate
	// it with ours.
	GMainContext* gctx = g_main_context_default();
	g_main_context_acquire(gctx);

	pc->glib_source = ml_custom_new(dui_mainloop(), &glib_custom_impl);
	ml_custom_set_data(pc->glib_source, gctx);

	return pc;
}

void mod_music_destroy(struct mod_music* pc) {
	if(pc->glib_source) ml_custom_destroy(pc->glib_source);
	if(pc->manager) g_object_unref(pc->manager);
	free(pc);
}

const char* mod_music_get_song(struct mod_music* pc) {
	return pc->songbuf;
}

enum music_state mod_music_get_state(struct mod_music* pc) {
	return pc->state;
}

void mod_music_next(struct mod_music* pc) {
	if(!pc->player) {
		printf("playerctl next: no active player\n");
		return;
	}

	GError* error = NULL;
	playerctl_player_next(pc->player, &error);
	if(error != NULL) {
		printf("playerctl next: %s\n", error->message);
		return;
	}

	display_show_banner(pc->dpy, banner_music);
}

void mod_music_prev(struct mod_music* pc) {
	if(!pc->player) {
		printf("playerctl prev: no active player\n");
		return;
	}

	GError* error = NULL;
	playerctl_player_previous(pc->player, &error);
	if(error != NULL) {
		printf("playerctl prev: %s\n", error->message);
		return;
	}

	display_show_banner(pc->dpy, banner_music);
}

void mod_music_toggle(struct mod_music* pc) {
	if(!pc->player) {
		printf("playerctl toggle: no active player\n");
		return;
	}

	GError* error = NULL;
	playerctl_player_play_pause(pc->player, &error);
	if(error != NULL) {
		printf("playerctl toggle: %s\n", error->message);
		return;
	}

	display_show_banner(pc->dpy, banner_music);
}
