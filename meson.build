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
dep_mpdclient = dependency('libmpdclient')
dep_alsa = dependency('alsa')
dep_sqlite3 = dependency('sqlite3')

dui_src = files(
	'src/battery.c',
	'src/brightness.c',
	'src/display.c',
	'src/draw.c',
	'src/daemon.c',
	'src/inotify.c',
	'src/mpd.c',
	'src/notes.c',
	'src/utf8.c',
	'src/volume.c',
)

dui_deps = [
	dep_cairo,
	dep_threads,
	dep_m,

	dep_mpdclient,
	dep_alsa,
	dep_sqlite3,
]

dui_inc = include_directories('src')

subdir('src/x11')
subdir('src/wl')

executable('dashboard',
	dui_src,
	include_directories: dui_inc,
	dependencies: dui_deps,
	install: true
)
