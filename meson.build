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

cc = meson.get_compiler('c')

dep_xcb = dependency('xcb')
dep_xcb_ewmh = dependency('xcb-ewmh')
dep_xcb_icccm = dependency('xcb-icccm')
dep_cairo = dependency('cairo')
dep_threads = dependency('threads')
dep_m = cc.find_library('m', required: false)

# modules
dep_mpdclient = dependency('libmpdclient')
dep_alsa = dependency('alsa')
dep_sqlite3 = dependency('sqlite3')

deps = [
	dep_xcb,
	dep_xcb_ewmh,
	dep_xcb_icccm,
	dep_cairo,
	dep_threads,
	dep_m,

	dep_mpdclient,
	dep_alsa,
	dep_sqlite3,
]

executable('dashboard',
	'src/dashboard.c',
	'src/mpd.c',
	'src/volume.c',
	'src/notes.c',
	'src/inotify.c',
	'src/brightness.c',
	'src/battery.c',
	dependencies: deps)
