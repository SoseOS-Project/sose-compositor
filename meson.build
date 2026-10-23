project(
  'sose-compositor',
  'c',
  version: '0.1.0',
  license: 'GPL-2.0-only',
  meson_version: '>=1.0',
  # optimized with debug info: fast, and crashes still show where they happened
  default_options: ['c_std=c11', 'warning_level=2', 'buildtype=debugoptimized'],
)

# wlroots' api isn't stable between versions, it makes you say you know that
add_project_arguments('-DWLR_USE_UNSTABLE', language: 'c')

# plain c11 hides posix functions (clock_gettime, setenv), this turns them back on
add_project_arguments('-D_POSIX_C_SOURCE=200809L', language: 'c')

# wayland callbacks always get arguments they don't all use, that's not a bug
add_project_arguments('-Wno-unused-parameter', language: 'c')

deps = [
  dependency('wlroots-0.20'),
  dependency('wayland-server'),
  dependency('xkbcommon'),
]

sources = files(
  'src/main.c',
  'src/toplevel.c',
  'src/popup.c',
  'src/decoration.c',
  'src/output.c',
  'src/input.c',
  'src/keybindings.c',
)

executable('sose-compositor', sources, dependencies: deps, install: true)
