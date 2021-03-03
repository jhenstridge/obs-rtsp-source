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
