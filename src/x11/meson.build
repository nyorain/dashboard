with_x11 = get_option('with-x11')

dep_xcb = dependency('xcb', required: with_x11)
dep_xcb_ewmh = dependency('xcb-ewmh', required: with_x11)
dep_xcb_icccm = dependency('xcb-icccm', required: with_x11)

dui_deps += [
	dep_xcb,
	dep_xcb_ewmh,
	dep_xcb_icccm,
]

dui_src += files('display.c')
