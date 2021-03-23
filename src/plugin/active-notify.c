#include "active-notify.h"

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

static void *
activity_notify_worker(void *user_data) {
    ActiveNotify *notify = user_data;

    for (;;) {
        g_autoptr(QueueItem) item = g_async_queue_pop(notify->queue);

        if (item->rtsp_url == NULL) break;

        g_message("Notify uri=%s active=%s", item->rtsp_url, item->active ? "true" : "false");
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
