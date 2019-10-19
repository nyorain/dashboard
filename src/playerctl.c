#include "config.h"
#include <stdlib.h>

#ifdef WITH_PLAYERCTL
#include <stdio.h>
#include <assert.h>
#include <playerctl/playerctl.h>
#include "shared.h"

#define MAX_PLAYER_COUNT 16

struct playerctl {
	PlayerctlPlayerManager* manager;
	int state;
	char songbuf[256]; // "artist - title"
	PlayerctlPlayer* player; // selected player

	// for signal disconnecting
	int sid_metadata;
};

static gboolean status_callback(PlayerctlPlayer* player,
	PlayerctlPlaybackStatus status, struct playerctl* pc);
static gboolean metadata_callback(PlayerctlPlayer* player,
	GVariant* metadata, struct playerctl* pc);

int convert_state(PlayerctlPlaybackStatus status) {
	switch(status) {
		case PLAYERCTL_PLAYBACK_STATUS_PLAYING: return 2;
		case PLAYERCTL_PLAYBACK_STATUS_PAUSED: return 3;
		case PLAYERCTL_PLAYBACK_STATUS_STOPPED: return 1;
		default: return 0;
	}
}

static void reload_state(struct playerctl* pc) {
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
	display_redraw_dashboard(display_get());
}

static void change_player(struct playerctl* pc, PlayerctlPlayer* player) {
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
static void select_player(struct playerctl* pc) {
	PlayerctlPlayer* found = NULL;

	GList* players = NULL;
    g_object_get(pc->manager, "players", &players, NULL);
    for(GList* l = players; l != NULL; l = l->next) {
		PlayerctlPlayer* player = l->data;
		gchar* name;
		g_object_get(player, "player-name", &name, NULL);

		PlayerctlPlaybackStatus status;
		g_object_get(player, "playback-status", &status, NULL);
		if(status == PLAYERCTL_PLAYBACK_STATUS_PLAYING) {
			found = player;
		} else if(strcmp(name, "mpd") == 0) {
			found = player;
		} else if(!found) {
			found = player;
		}

		free(name);
	}

	change_player(pc, found);
}

static gboolean status_callback(PlayerctlPlayer* player,
		PlayerctlPlaybackStatus status, struct playerctl* pc) {
	// TODO: we could additionally always switch to the currently
	// playing player if there is one.
	// Only relevant when multiple players are playing which shouldn't
	// be the case, ever, anyways.
	if(player == pc->player) {
		pc->state = convert_state(status);
		if(status == PLAYERCTL_PLAYBACK_STATUS_STOPPED) {
			select_player(pc);
		} else {
			display_redraw_dashboard(display_get());
		}
	} else if(status == PLAYERCTL_PLAYBACK_STATUS_PLAYING && (pc->state != 2)) {
		change_player(pc, player);
	}

	return true;
}

static gboolean metadata_callback(PlayerctlPlayer* player, GVariant* metadata,
		struct playerctl* pc) {
	assert(player == pc->player);
	reload_state(pc);
	display_redraw_dashboard(display_get());
	return true;
}

static void name_appeared_callback(PlayerctlPlayerManager* manager,
		PlayerctlPlayerName* name, struct playerctl* pc) {
	printf("name appeared\n");
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
		PlayerctlPlayer* player, struct playerctl* pc) {
	printf("player vanished\n");
	if(pc->player == player) {
		pc->player = NULL;
		pc->sid_metadata = -1;
		select_player(pc);
	}
}

static void dispatch(int fd, unsigned revents, void* data) {
	// struct playerctl* pc = (struct playerctl*) data;
	GMainContext* ctx = g_main_context_default();
	GPollFD gfd;
	gfd.fd = fd;
	gfd.revents = revents;
	g_main_context_check(ctx, 100000, &gfd, 1);
	g_main_context_dispatch(ctx);

	// prepare for next iteration
	gint prio, timeout;
	g_main_context_prepare(ctx, &prio);
	int res = g_main_context_query(ctx, prio, &timeout, &gfd, 1);
	assert(res == 1);
	assert(gfd.fd == fd);
}

struct playerctl* playerctl_create(void) {
	GError* error = NULL;

	struct playerctl* pc = calloc(1, sizeof(*pc));
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
		printf("Found player: %s, %s\n", name->name, name->instance);

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

	// we don't want to use the glib main loop so we integrate
	// it with ours.
	// NOTE: really hacky atm. We assume that there is only one
	// fd and that it never changes.
	GMainContext* ctx = g_main_context_default();
	gint prio, timeout;
	g_main_context_acquire(ctx);
	g_main_context_prepare(ctx, &prio);

	GPollFD fd;
	int res = g_main_context_query(ctx, prio, &timeout, &fd, 1);
	assert(res == 1);
	add_poll_handler(fd.fd, fd.events, pc, dispatch);

	return pc;
}

void playerctl_destroy(struct playerctl* pc) {
	if(pc->manager) g_object_unref(pc->manager);
	free(pc);
}

const char* playerctl_get_song(struct playerctl* pc) {
	return pc->songbuf;
}

int playerctl_get_state(struct playerctl* pc) {
	return pc->state;
}

void playerctl_next(struct playerctl* pc) {
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

	display_show_banner(display_get(), banner_music);
}

void playerctl_prev(struct playerctl* pc) {
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

	display_show_banner(display_get(), banner_music);
}

void playerctl_toggle(struct playerctl* pc) {
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

	display_show_banner(display_get(), banner_music);
}

#else // HAVE_PLAYERCTL

struct playerctl* playerctl_create(void) { return NULL; }
void playerctl_destroy(struct playerctl* pc) {}
const char* playerctl_get_song(struct playerctl* pc) { return NULL; }
int playerctl_get_state(struct playerctl* pc) { return 0; }
void playerctl_next(struct playerctl* pc) {}
void playerctl_prev(struct playerctl* pc) {}
void playerctl_toggle(struct playerctl* pc) {}

#endif
