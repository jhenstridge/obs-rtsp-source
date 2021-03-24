#include "active-notify.h"

#include <gio/gio.h>

struct _ActiveNotify {
    GThread *worker;
    GAsyncQueue *queue;
};

typedef struct {
    char *rtsp_url;
    gboolean active;
} QueueItem;

static void
queue_item_free(QueueItem *item) {
    g_free(item->rtsp_url);
    g_free(item);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(QueueItem, queue_item_free);

static gboolean
set_parameter(GSocketClient *client, const char *rtsp_url, gboolean active,
              GError **error) {
    const char *body;
    g_autofree char *request = NULL;
    g_autoptr(GSocketConnection) conn = NULL;
    char response[4096];
    GInputStream *istream;
    GOutputStream *ostream;

    body = active ? "obs-active: true" : "obs-active: false";
    request = g_strdup_printf(
        "SET_PARAMETER %s RTSP/2.0\r\n"
        "CSeq: 0\r\n"
        "Connection: close\r\n"
        "Content-Type: text/parameters\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s", rtsp_url, (int)strlen(body), body);

    conn = g_socket_client_connect_to_uri(client, rtsp_url, 554, NULL, error);
    if (!conn) {
        return FALSE;
    }

    istream = g_io_stream_get_input_stream(G_IO_STREAM(conn));
    ostream = g_io_stream_get_output_stream(G_IO_STREAM(conn));

    if (!g_output_stream_write_all(ostream, request, strlen(request), NULL, NULL, error)) {
        return FALSE;
    }

    // Read the response
    if (g_input_stream_read(istream, response, sizeof(response), NULL, error) < 0) {
        return FALSE;
    }

    if (!g_io_stream_close(G_IO_STREAM(conn), NULL, error)) {
        return FALSE;
    }

    return TRUE;
}


static void *
activity_notify_worker(void *user_data) {
    ActiveNotify *notify = user_data;
    g_autoptr(GSocketClient) client = NULL;

    client = g_socket_client_new();

    for (;;) {
        g_autoptr(QueueItem) item = g_async_queue_pop(notify->queue);
        g_autoptr(GError) error = NULL;

        if (item->rtsp_url == NULL) break;

        if (!set_parameter(client, item->rtsp_url, item->active, &error)) {
            g_warning("Could not send SET_PARAMETER request: %s", error->message);
        }
    }

    return NULL;
}

ActiveNotify *
active_notify_new(void) {
    g_autoptr(ActiveNotify) notify = g_new0(ActiveNotify, 1);

    notify->queue = g_async_queue_new_full((GDestroyNotify)queue_item_free);
    notify->worker = g_thread_new("rtsp-activity-notify", activity_notify_worker, notify);

    return g_steal_pointer(&notify);
}

void
active_notify_free(ActiveNotify *notify) {
    // Send empty item to signal worker to exit
    g_async_queue_push(notify->queue, g_new0(QueueItem, 1));
    // Wait for thread to exit
    g_thread_join(notify->worker);

    g_clear_pointer(&notify->worker, g_thread_unref);
    g_clear_pointer(&notify->queue, g_async_queue_unref);
    g_free(notify);
}

void
active_notify_send(ActiveNotify *notify, const char *rtsp_url, gboolean active) {
    QueueItem *item;

    g_assert(rtsp_url != NULL);

    item = g_new(QueueItem, 1);
    item->rtsp_url = g_strdup(rtsp_url);
    item->active = active;

    g_async_queue_push(notify->queue, item);
}
