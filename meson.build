cargs = ['-DG_LOG_DOMAIN="FuPluginFirehose"']

install_data(['firehose.quirk'],
  install_dir: join_paths(datadir, 'fwupd', 'quirks.d')
)

shared_module('fu_plugin_firehose',
  fu_hash,
  sources : [
    'fu-plugin-firehose.c',
    'fu-firehose-device.c',
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
