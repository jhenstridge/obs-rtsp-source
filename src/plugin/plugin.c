#include <obs/obs-module.h>

#include "mdns-browse.h"
#include "active-notify.h"
#include "source.h"

OBS_DECLARE_MODULE();

MdnsBrowser *mdns_browser = NULL;
ActiveNotify *active_notify = NULL;

bool
obs_module_load(void) {
    g_autoptr(GError) error = NULL;

    mdns_browser = mdns_browser_new(&error);
    if (!mdns_browser) {
        g_warning("Could not create mDNS browser: %s", error->message);
    }

    active_notify = active_notify_new();

    obs_register_source(&remote_source);
    return true;
}

void
obs_module_unload(void) {
    g_clear_pointer(&mdns_browser, mdns_browser_free);
    g_clear_pointer(&active_notify, active_notify_free);
}
