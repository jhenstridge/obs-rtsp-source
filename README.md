This is a work in progress project intended to make it easy to
integrate networked cameras into an OBS Studio broadcast with minimal
configuration.

Having previously used the dvswitch set of tools to do multi-camera
live mixed video, I wanted to see if I could get a similar model
working with OBS Studio.

The intended model is something like this:

1. source machine runs an RTSP server that streams its camera via RTSP.
2. streams are advertised via mDNS (a.k.a. Bonjour).
3. streams are discovered and used as sources on mixing machine running OBS.

Currently only the first two parts are working, but we can use the
streams via OBS's default "Media Source" source.  The main settings to
change from default are:

* unset "Local File"
* unset "Reset playback when source becomes active"
* set "Network buffering" to 0 MB
* set "Input" to the rtsp URL of the stream

## Pipelines

The RTSP server is built on top of GStreamer and will use pipelines
defined in the configuration file.  The appropriate pipeline will
depend on the features of the camera/capture device.

For a camera that supports MJPG, the following pipeline should work:

    v4l2src device=/dev/video0 ! image/jpeg,width=1280,height=720,framerate=30/1 ! rtpjpegpay name=pay0

For a camera with a built-in H.264 encoder, the following can be used:

    v4l2src device=/dev/video0 ! video/x-h264,width=1920,height=1080,framerate=30/1 ! h264parse ! rtph264pay name=pay0

For a camera supporting YUYV, encoding to MJPG:

    v4l2src device=/dev/video0 ! video/x-raw,format=(string)YUY2,width=1280,height=720,framerate=10/1 ! jpegenc ! rtpjpegpay name=pay0

The capabilities of the camera can be determined with the following
command:

    v4l2-ctl -d /dev/video0 --list-formats-ext
