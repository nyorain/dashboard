with_wl = get_option('with-wayland')

# Generate client protocols
# Taken from swaybg, https://github.com/swaywm/swaybg
wayland_client = dependency('wayland-client', required: with_wl)
wayland_protos = dependency('wayland-protocols', version: '>=1.14', required: with_wl)

wayland_scanner = find_program('wayland-scanner', required: with_wl)
wl_protocol_dir = wayland_protos.get_pkgconfig_variable('pkgdatadir')

if wayland_client.version().version_compare('>=1.14.91')
	code_type = 'private-code'
else
	code_type = 'code'
endif

wayland_scanner_code = generator(
	wayland_scanner,
	output: '@BASENAME@-protocol.c',
	arguments: [code_type, '@INPUT@', '@OUTPUT@'],
)

wayland_scanner_client = generator(
	wayland_scanner,
	output: '@BASENAME@-client-protocol.h',
	arguments: ['client-header', '@INPUT@', '@OUTPUT@'],
)

client_protos_src = []
client_protos_headers = []

client_protocols = [
	[wl_protocol_dir, 'stable/xdg-shell/xdg-shell.xml'],
	[wl_protocol_dir, 'unstable/xdg-output/xdg-output-unstable-v1.xml'],
	['wlr-layer-shell-unstable-v1.xml'],
]

foreach p : client_protocols
	xml = join_paths(p)
	client_protos_src += wayland_scanner_code.process(xml)
	client_protos_headers += wayland_scanner_client.process(xml)
endforeach

lib_client_protos = static_library(
	'client_protos',
	client_protos_src + client_protos_headers,
	dependencies: [wayland_client]
) # for the include directory

client_protos = declare_dependency(
	link_with: lib_client_protos,
	sources: client_protos_headers,
)


# add protocols and wayland code to dui
dui_deps += [
	client_protos,
	wayland_client,
]

dui_src += files('display.c')
