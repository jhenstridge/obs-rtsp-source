#pragma once

#include <glib.h>

typedef struct _Publisher Publisher;

Publisher *publisher_new(int port, GError **error);

void publisher_free(Publisher *publisher);

void publisher_add_stream(Publisher *publisher,
                          const char *path,
                          const char *name);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(Publisher, publisher_free);
