# Copyright 2026 The Quick Share Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

project(
  'quick-share-ubuntu',
  ['c', 'cpp'],
  version : '0.1.0',
  default_options : [
    'c_std=c11',
    'cpp_std=c++17',
    'warning_level=2',
    'buildtype=debugoptimized',
  ],
  meson_version : '>= 0.61.0',
)

gtk_dep  = dependency('gtk4',          version : '>= 4.10')
adw_dep  = dependency('libadwaita-1',  version : '>= 1.4')
gio_dep  = dependency('gio-2.0')

bridge_inc = include_directories('../facade')
bridge_lib_dir = meson.current_source_dir() + '/../../bazel-bin/app/facade'

cc = meson.get_compiler('c')
nearby_bridge_dep = cc.find_library(
  'nearby_bridge',
  dirs : [bridge_lib_dir],
  required : true,
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
  include_directories : [bridge_inc, include_directories('src')],
  dependencies : [gtk_dep, adw_dep, gio_dep, nearby_bridge_dep],
  link_args : ['-Wl,-rpath,' + bridge_lib_dir],
  install : false,
)
