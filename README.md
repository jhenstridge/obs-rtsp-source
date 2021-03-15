This is a work in progress project intended to make it easy to
integrate networked cameras into an OBS Studio broadcast with minimal
configuration.

Having previously used the dvswitch set of tools to do multi-camera
live mixed video, I wanted to see if I could get a similar model
working with OBS Studio.

The project consists of two components: an RTSP server that runs on a
sender node to share camera streas, and an OBS plugin that provides a
source giving easy access to these streams.


## The RTSP Sender

The `rtsp-sender` daemon is a GStreamer based RTSP server used to
share one or more cameras.  It can be invoked with:

    rtsp-sender [ -p PORT ] -c CONFIG_FILE

The configuration file is a simple ini-style file describing the
streams to export.  Each section describes a URI path provided by the
RTSP server.  The `pipeline` key sets a GStreamer pipeline used to
produce the RTP payloads.  The `publish` key provides the mDNS service
name to advertise the stream as.

The appropriate pipeline will depend on the capabilities of the video
capture device you're using and the desired video resolution/frame
rate.  A good place to start is by listing the capabilities with the
following command:

    v4l2-ctl -d /dev/video0 --list-formats-ext

(or whatever file name your capture device uses).  From there, it will
depend on the formats supported:

1. If the device supports MGPG, a pipeline like the following will
   work (with appropriate adjustments for resolution and framerate):

       v4l2src device=/dev/video0 ! image/jpeg,width=1280,height=720,framerate=30/1 ! rtpjpegpay name=pay0

2. If the device supports H264, use the following:

       v4l2src device=/dev/video0 ! video/x-h264,width=1920,height=1080,framerate=30/1 ! h264parse ! rtph264pay name=pay0

3. For cameras supporting raw YUYV, we can encode to JPEG with a
   pipeline like the following:

       v4l2src device=/dev/video0 ! video/x-raw,format=(string)YUY2,width=1280,height=720,framerate=10/1 ! jpegenc ! rtpjpegpay name=pay0

In theory, you should be able to send raw video using the `rtpvrawpay`
element, but I couldn't get that to work reliably.

As well as serving the streams via RTSP, the daemon also advertises
the streams via mDNS (also known as Bonjour) using Avahi.  You can get
a listing of the cameras available on the local network with the
following command:

    avahi-browse -rt _obs-source._sub._rtsp._tcp


## The OBS plugin

The `remote-source.so` plugin should be copied to the OBS plugin
directory (probably `/usr/lib/x86_64-linux-gnu/obs-plugins` or
similar).

When installed, a new "Remote Source" source type will be available.
When you add a source of this type to a scene, it will provide a list
of named camera streams advertised on the network.  If you want to use
the same camera in multiple scenes, I suggest copy/pasting the source
rather than creating a new source for the same stream.

The source continues to monitor whether the camera's service name is
being advertised.  When it stops being advertised, the source
disconnects from the RTSP server.  When the name is advertised again,
the source will immediately reconnect.

This means that you can start OBS without ensuring all the camera
machines are running.  As each system on the network starts, the
streams will integrate into the scenes set up in previous sessions.


## Todo

As the OBS plugin also lets us know when the source is "active"
(i.e. part of the current scene), it could also let us communicate
this back to the sender.  In turn, that would allow for some kind of
"tally light" feature.
