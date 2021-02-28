#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

int main(int argc, char **argv) {
    g_autoptr(GMainLoop) main_loop = NULL;
    g_autoptr(GstRTSPServer) server = NULL;
    g_autoptr(GstRTSPMountPoints) mounts = NULL;
    g_autoptr(GstRTSPMediaFactory) factory = NULL;

    gst_init(&argc, &argv);

    main_loop = g_main_loop_new(NULL, FALSE);
    server = gst_rtsp_server_new();

    mounts = gst_rtsp_server_get_mount_points(server);

    factory = gst_rtsp_media_factory_new();
    gst_rtsp_media_factory_set_launch(
        factory, "( "
        "videotestsrc ! video/x-raw,format=(string)YUY2,width=1280,height=720,framerate=30/1 ! "
//        "videoconvert ! rtpvrawpay name=pay0 pt=96 "
        "jpegenc ! queue ! rtpjpegpay name=pay0 pt=26 "
        ")");

    gst_rtsp_mount_points_add_factory(mounts, "/test", factory);

    if (!gst_rtsp_server_attach(server, NULL)) {
        g_warning("Could not attach server");
        return 1;
    }

    g_main_loop_run(main_loop);

    return 0;
}
