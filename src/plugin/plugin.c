#include <obs/obs-module.h>

#include "mdns-browse.h"
#include "source.h"

OBS_DECLARE_MODULE();

Browser *mdns_browser = NULL;

bool
obs_module_load(void) {
    g_autoptr(GError) error = NULL;

    mdns_browser = browser_new(&error);
    if (!mdns_browser) {
        g_warning("Could not create mDNS browser: %s", error->message);
    }

    obs_register_source(&remote_source);
    return true;
}

void
obs_module_unload(void) {
    g_clear_pointer(&mdns_browser, browser_free);
}
