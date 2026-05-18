project(
  'quick-share-ubuntu',
  'c',
  version : '0.1.0',
  default_options : [
    'c_std=c11',
    'warning_level=2',
    'buildtype=debugoptimized',
  ],
  meson_version : '>= 0.61.0',
)

gtk_dep  = dependency('gtk4',          version : '>= 4.10')
adw_dep  = dependency('libadwaita-1',  version : '>= 1.4')
gio_dep  = dependency('gio-2.0')

facade_inc = include_directories('../facade')

facade_lib = static_library(
  'mock_facade',
  sources : ['../facade/mock_facade.c'],
  include_directories : facade_inc,
  dependencies : [gtk_dep, gio_dep],
)

executable(
  'quick-share-ubuntu',
  sources : [
    'src/main.c',
    'src/app_window.c',
    'src/ui_state.c',
    'src/style.c',
    'src/splash_window.c',
    'src/format_util.c',
    'src/view_home.c',
    'src/view_incoming.c',
    'src/view_transferring.c',
    'src/view_complete.c',
    'src/view_failed.c',
    'src/view_onboarding.c',
    'src/view_settings.c',
  ],
  include_directories : [facade_inc, include_directories('src')],
  link_with : facade_lib,
  dependencies : [gtk_dep, adw_dep, gio_dep],
  install : false,
)
