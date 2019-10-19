with_x11 = get_option('with-x11')

dep_xcb = dependency('xcb', required: with_x11)
dep_xcb_ewmh = dependency('xcb-ewmh', required: with_x11)
dep_xcb_icccm = dependency('xcb-icccm', required: with_x11)

dui_deps += [
	dep_xcb,
	dep_xcb_ewmh,
	dep_xcb_icccm,
]

found_x11 = true
foreach dep : dui_deps
	if not dep.found()
		found_x11 = false
	endif
endforeach

conf_data.set10('WITH_X11', found_x11)
if found_x11
	dui_src += files('display.c')
endif
