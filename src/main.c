#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#define DEFAULT_RTSP_PORT "8554"
#define DEFAULT_CONFIG_FILE "rtsp-server.conf"

static gboolean
parse_options(int *argc, char ***argv, char **port, GKeyFile **config,
              GError **error) {
    g_autoptr(GOptionContext) ctx = NULL;
    char *config_file = DEFAULT_CONFIG_FILE;
    GOptionEntry options[] = {
        {"port", 'p', 0, G_OPTION_ARG_STRING, port,
         "Port to listen on (default: " DEFAULT_RTSP_PORT ")", "PORT"},
        {"config", 'c', 0, G_OPTION_ARG_FILENAME, &config_file,
         "Configuration file (default: " DEFAULT_CONFIG_FILE ")", "CONFIG"},
        {NULL}
    };

    *port = DEFAULT_RTSP_PORT;
    ctx = g_option_context_new(NULL);
    g_option_context_add_main_entries(ctx, options, NULL);
    g_option_context_add_group(ctx, gst_init_get_option_group());

    if (!g_option_context_parse (ctx, argc, argv, error)) {
        return FALSE;
    }

    *config = g_key_file_new();
    return g_key_file_load_from_file(*config, config_file, G_KEY_FILE_NONE, error);
}

static gboolean
setup_streams(GstRTSPServer *server, GKeyFile *config, GError **error) {
    g_autoptr(GstRTSPMountPoints) mounts = NULL;
    g_auto(GStrv) groups = NULL;
    gsize n_groups, i;

    mounts = gst_rtsp_server_get_mount_points(server);

    groups = g_key_file_get_groups(config, &n_groups);
    for (i = 0; i < n_groups; i++) {
        g_autoptr(GstRTSPMediaFactory) factory = NULL;
        g_autofree char *pipeline = NULL;
        g_autofree char *launch = NULL;

        pipeline = g_key_file_get_string(config, groups[i], "pipeline", error);
        if (!pipeline)
            return FALSE;

        factory = gst_rtsp_media_factory_new();
        launch = g_strjoin("( ", pipeline, " )", NULL);
        gst_rtsp_media_factory_set_launch(factory, launch);

        gst_rtsp_mount_points_add_factory(
            mounts, groups[i], g_steal_pointer(&factory));
    }

    return TRUE;
}

static void
client_closed(GstRTSPClient *client) {
    GstRTSPConnection *conn = gst_rtsp_client_get_connection(client);

    g_message("Closed connection from %s", gst_rtsp_connection_get_ip(conn));
}

static void
client_connected(GstRTSPServer *server, GstRTSPClient *client) {
    GstRTSPConnection *conn = gst_rtsp_client_get_connection(client);

    g_signal_connect(client, "closed", G_CALLBACK(client_closed), NULL);
    g_message("Received connection from %s", gst_rtsp_connection_get_ip(conn));
}

int
main(int argc, char **argv) {
    g_autoptr(GError) error = NULL;
    g_autoptr(GMainLoop) main_loop = NULL;
    g_autoptr(GstRTSPServer) server = NULL;
    g_autoptr(GKeyFile) config = NULL;
    char *port = NULL;

    if (!parse_options(&argc, &argv, &port, &config, &error)) {
        g_printerr("Error parsing options: %s\n", error->message);
        return 1;
    }

    main_loop = g_main_loop_new(NULL, FALSE);
    server = gst_rtsp_server_new();
    g_object_set(server, "service", port, NULL);
    g_signal_connect(server, "client-connected", G_CALLBACK(client_connected), NULL);

    if (!setup_streams(server, config, &error)) {
        g_printerr("Error setting up streams: %s\n", error->message);
        return 1;
    }

    if (!gst_rtsp_server_attach(server, NULL)) {
        g_printerr("Could not attach server: %s\n", error->message);
        return 1;
    }

    g_main_loop_run(main_loop);

    return 0;
}
