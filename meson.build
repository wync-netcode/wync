project('wync', 'c')

source_files = [
  './src/wync_flow.c',
  './src/wync_init.c',
  './src/wync_join.c',
  './src/wync_lerp.c',
  './src/wync_prop.c',
  './src/wync_stat.c',
  './src/wync_clock.c',
  './src/wync_debug.c',
  './src/wync_input.c',
  './src/wync_spawn.c',
  './src/wync_track.c',
  './src/wync_xtrap.c',
  './src/wync_throttle.c',
  './src/wync_state_set.c',
  './src/wync_state_send.c',
  './src/wync_event_utils.c',
  './src/wync_packet_util.c',
  './src/wync_state_store.c',
  './src/wync_wrapper_util.c',
  './src/wync_tick_collection.c',
  './src/wync_delta_sync.c',
  './src/wync_actions_and_events_consumed.c',
]

cc = meson.get_compiler('c')
math_dep = cc.find_library('m', required : false)

inc = include_directories('.')
wync = library('wync',
  source_files,
  dependencies : [
    math_dep,
  ],
  include_directories : inc,
  install : true)

wync_dep = declare_dependency(
  include_directories : inc,
  link_with : wync)

# tests

text_exe = executable('tests',
  './test/tests.c',
  dependencies: [ wync_dep ]
)
test('Main test', text_exe)

