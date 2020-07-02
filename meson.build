cargs = ['-DG_LOG_DOMAIN="FuPluginQuectel"']

install_quectel = false
if get_option('plugin_quectel')
  install_quectel = true
endif

install_data(['quectel.quirk'],
  install_dir: join_paths(datadir, 'fwupd', 'quirks.d')
)

shared_module('fu_plugin_quectel',
  sources : [
    'fu-plugin-quectel.c',
    'fu-quectel-device.c',
    'fu-quectel-firmware.c',
    'protocol/sahara_proto.c',
    'protocol/firehose_proto.c',
  ],
  include_directories : [
    root_incdir,
    fwupd_incdir,
    fwupdplugin_incdir,
  ],
  install : true,
  install_dir: plugin_dir,
  link_with : [
    fwupd,
    fwupdplugin,
  ],
  c_args : cargs,
  dependencies : [
    plugin_deps,
  ],
)