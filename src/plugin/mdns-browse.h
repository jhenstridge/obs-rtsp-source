#pragma once

#include <glib.h>

typedef struct _Browser Browser;

Browser *browser_new(GError **error);
void browser_free(Browser *browser);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(Browser, browser_free);
