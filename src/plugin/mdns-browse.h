#pragma once

#include <glib.h>

typedef struct _Browser Browser;

Browser *browser_new(GError **error);
void browser_free(Browser *browser);

// stamp is incremented whenever the browser state changes.  This
// gives a quick way to quickly check if anything has changed.
gint browser_get_stamp(Browser *browser);
char *browser_get_uri(Browser *browser, const char *name, gint *stamp);
GStrv browser_get_available(Browser *browser);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(Browser, browser_free);
