#include <glib.h>
#include <glib-object.h>
#include <gst/gst.h>
#include <gst/rtsp-server/rtsp-server.h>

#include "mdns-publish.h"

#define DEFAULT_RTSP_PORT 8554
#define DEFAULT_CONFIG_FILE "rtsp-server.conf"

static gboolean
parse_options(int *argc, char ***argv, int *port, GKeyFile **config,
              GError **error) {
    g_autoptr(GOptionContext) ctx = NULL;
    char *config_file = DEFAULT_CONFIG_FILE;
    GOptionEntry options[] = {
        {"port", 'p', 0, G_OPTION_ARG_INT, port,
         "Port to listen on (default: " G_STRINGIFY(DEFAULT_RTSP_PORT) ")", "PORT"},
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
setup_streams(GstRTSPServer *server, Publisher *publisher,
              GKeyFile *config, GError **error) {
    g_autoptr(GstRTSPMountPoints) mounts = NULL;
    g_auto(GStrv) groups = NULL;
    gsize n_groups, i;

    mounts = gst_rtsp_server_get_mount_points(server);

    groups = g_key_file_get_groups(config, &n_groups);
    for (i = 0; i < n_groups; i++) {
        g_autoptr(GstRTSPMediaFactory) factory = NULL;
        g_autofree char *pipeline = NULL;
        g_autofree char *launch = NULL;
        g_autofree char *publish = NULL;

        pipeline = g_key_file_get_string(config, groups[i], "pipeline", error);
        if (!pipeline)
            return FALSE;

        factory = gst_rtsp_media_factory_new();
        launch = g_strjoin("( ", pipeline, " )", NULL);
        gst_rtsp_media_factory_set_launch(factory, launch);
        gst_rtsp_media_factory_set_shared(factory, TRUE);

        gst_rtsp_mount_points_add_factory(
            mounts, groups[i], g_steal_pointer(&factory));

        publish = g_key_file_get_string(config, groups[i], "publish", NULL);
        if (publish) {
            publisher_add_stream(publisher, groups[i], publish);
        }
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
    g_autoptr(Publisher) publisher = NULL;
    g_autoptr(GKeyFile) config = NULL;
    int port;
    g_autofree char *port_str = NULL;

    if (!parse_options(&argc, &argv, &port, &config, &error)) {
        g_printerr("Error parsing options: %s\n", error->message);
        return 1;
    }

    main_loop = g_main_loop_new(NULL, FALSE);
    server = gst_rtsp_server_new();
    port_str = g_strdup_printf("%d", port);
    g_object_set(server, "service", port_str, NULL);
    g_signal_connect(server, "client-connected", G_CALLBACK(client_connected), NULL);

    publisher = publisher_new(port, &error);
    if (!publisher) {
        g_printerr("Error setting up Avahi publisher: %s\n", error->message);
        return 1;
    }

    if (!setup_streams(server, publisher, config, &error)) {
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
