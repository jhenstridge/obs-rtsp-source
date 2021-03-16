#pragma once

#include <glib.h>

typedef struct _MdnsPublisher MdnsPublisher;

MdnsPublisher *mdns_publisher_new(int port, GError **error);

void mdns_publisher_free(MdnsPublisher *publisher);

void mdns_publisher_add_stream(MdnsPublisher *publisher,
                               const char *path,
                               const char *name);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(MdnsPublisher, mdns_publisher_free);
