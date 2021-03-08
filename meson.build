project('rtsp', 'c', version: '0.1')

glib_dep = dependency('glib-2.0')
gst_rtsp_dep = dependency('gstreamer-rtsp-1.0')
gst_rtsp_server_dep = dependency('gstreamer-rtsp-server-1.0')
avahi_client_dep = dependency('avahi-client')
avahi_glib_dep = dependency('avahi-glib')

rtsp_deps = [
  gst_rtsp_dep, gst_rtsp_server_dep,
  avahi_client_dep, avahi_glib_dep
]

if get_option('obs-plugin')
  obs_dep = dependency('libobs')
  plugin_deps = [glib_dep, obs_dep]
endif

subdir('src')
