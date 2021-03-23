#pragma once

#include <glib.h>

typedef struct _ActiveNotify ActiveNotify;

ActiveNotify *active_notify_new(void);
void active_notify_free(ActiveNotify *notify);

void active_notify_send(ActiveNotify *notify, const char *rtsp_url, gboolean active);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(ActiveNotify, active_notify_free);
