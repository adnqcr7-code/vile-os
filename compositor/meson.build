project('vile-compositor', 'c',
  version: '0.5.6',
  license: 'MIT',
  meson_version: '>=1.3',
  default_options: ['c_std=c11', 'warning_level=2'])

wlroots = dependency('wlroots-0.19')
wayland_server = dependency('wayland-server')
libdrm = dependency('libdrm')
pixman = dependency('pixman-1')
pangocairo = dependency('pangocairo')
cairo = dependency('cairo')
xkbcommon = dependency('xkbcommon')
pam = meson.get_compiler('c').find_library('pam', required: true)
math = meson.get_compiler('c').find_library('m', required: false)

# wlroots' public headers include the generated xdg-shell protocol
# header, but a source-built wlroots does not install it. Generate it
# ourselves from wayland-protocols — the standard wlroots-consumer
# pattern, and it works on Arch too (wayland-protocols + wayland-scanner
# come with the `wayland` package there).
wayland_protos = dependency('wayland-protocols')
wayland_scanner = find_program('wayland-scanner')
protos_dir = wayland_protos.get_variable('pkgdatadir')
xdg_shell_proto = custom_target('xdg-shell-protocol.h',
  output: 'xdg-shell-protocol.h',
  input: protos_dir / 'stable/xdg-shell/xdg-shell.xml',
  command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'])

executable('vile',
  'src/main.c',
  xdg_shell_proto,
  dependencies: [wlroots, wayland_server, libdrm, pixman, pangocairo,
    cairo, xkbcommon, pam, math],
  install: true)
