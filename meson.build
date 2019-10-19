project(
	'dashboard',
	'c',
	version: '0.1.0',
	license: 'MIT',
	meson_version: '>=0.48.0',
	default_options: [
		'c_std=c11',
		'warning_level=3',
		'werror=true',
	],
)

add_project_arguments(
	[
		'-Wno-unused-parameter',
		'-Wundef',
		'-Wvla',
	],
	language: 'c',
)

cc = meson.get_compiler('c')

dep_cairo = dependency('cairo')
dep_threads = dependency('threads')
dep_m = cc.find_library('m', required: false)

# modules
dep_sqlite3 = dependency('sqlite3', required: get_option('with-notes'))

dui_src = files(
	'src/power.c',
	'src/brightness.c',
	'src/display.c',
	'src/ui.c',
	'src/daemon.c',
	'src/inotify.c',
	'src/utf8.c',
)

dui_deps = [
	dep_cairo,
	dep_threads,
	dep_m,
]

# mod_music
dep_playerctl = dependency('playerctl', required: get_option('impl-music') == 'playerctl')
dep_mpd = dependency('libmpdclient', required: get_option('impl-music') == 'mpd')

music_impl = 'dummy'
if dep_playerctl.found()
	music_impl = 'playerctl'
	dui_deps += [dep_playerctl]
elif dep_mpd.found()
	music_impl = 'mpd'
	dui_deps += [dep_mpd]
endif

dui_src += files('src/music_' + music_impl + '.c')

# mod_audio
dep_alsa = dependency('alsa', required: get_option('impl-audio') == 'alsa')
dep_pulse = dependency('libpulse', required: get_option('impl-audio') == 'pulse')

audio_impl = 'dummy'
if dep_alsa.found()
	audio_impl = 'alsa'
	dui_deps += [dep_alsa]
elif
	audio_impl = 'pulse'
	dui_deps += [dep_pulse]
endif

dui_src += files('src/audio_' + audio_impl + '.c')

# mod_notes
if dep_sqlite3.found()
	dui_src += files('src/notes.c')
	dui_deps += [dep_sqlite3]
endif

conf_data = configuration_data()
conf_data.set_quoted('MOD_MUSIC_IMPL', music_impl)
conf_data.set_quoted('MOD_AUDIO_IMPL', audio_impl)
conf_data.set10('WITH_NOTES', dep_sqlite3.found())

subdir('src/x11')
subdir('src/wl')

dui_inc = [
	include_directories('.'),
	include_directories('include'),
	include_directories('src'),
]

configure_file(output: 'config.h',
	configuration: conf_data
)

executable('dashboard',
	dui_src,
	include_directories: dui_inc,
	dependencies: dui_deps,
	install: true
)
