if get_option('obs-plugin')

  shared_module('remote-source',
    'plugin.c',
    'source.c',
    'mdns-browse.c',
    'active-notify.c',
    c_args: '-fvisibility=hidden',
    name_prefix: '',
    dependencies: plugin_deps,
  )
endif
