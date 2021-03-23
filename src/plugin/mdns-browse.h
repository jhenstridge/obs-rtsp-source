#pragma once

#include <glib.h>

typedef struct _MdnsBrowser MdnsBrowser;

MdnsBrowser *mdns_browser_new(GError **error);
void mdns_browser_free(MdnsBrowser *browser);

// stamp is incremented whenever the browser state changes.  This
// gives a quick way to quickly check if anything has changed.
gint mdns_browser_get_stamp(MdnsBrowser *browser);
char *mdns_browser_get_uri(MdnsBrowser *browser, const char *name, gint *stamp);
GStrv mdns_browser_get_available(MdnsBrowser *browser);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(MdnsBrowser, mdns_browser_free);
