#include "mdns-browse.h"

#include <glib.h>
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/thread-watch.h>
#include <avahi-common/error.h>

#define MDNS_ERROR mdns_error_quark()
G_DEFINE_QUARK(mdns-error-quark, mdns_error);

struct _Browser {
    AvahiThreadedPoll *poll;
    AvahiClient *client;
    AvahiServiceBrowser *service_browser;

    GHashTable *services;
};

static void
resolve_callback(AvahiServiceResolver *r,
                 AvahiIfIndex interface, AvahiProtocol protocol,
                 AvahiResolverEvent event, const char *name, const char *type,
                 const char *domain, const char *host_name,
                 const AvahiAddress *address, uint16_t port,
                 AvahiStringList *txt, AvahiLookupResultFlags flags,
                 void *user_data) {
    Browser *browser = user_data;

    switch (event) {
    case AVAHI_RESOLVER_FAILURE:
        g_warning("Failed to resolve service '%s' of type '%s' in domain '%s': %s", name, type, domain, avahi_strerror(avahi_client_errno(browser->client)));
        break;

    case AVAHI_RESOLVER_FOUND:
        if (!g_hash_table_lookup(browser->services, name)) {
            AvahiStringList *path_entry;
            char *path = NULL;
            char *rtsp_uri;

            // Extract path from TXT data
            path_entry = avahi_string_list_find(txt, "path");
            if (path_entry) {
                avahi_string_list_get_pair(path_entry, NULL, &path, NULL);
            }

            rtsp_uri = g_strdup_printf("rtsp://%s:%u%s", host_name, port,
                                       path ? path : "/");
            g_hash_table_insert(browser->services, g_strdup(name), rtsp_uri);
            g_message("Found service '%s': %s", name, rtsp_uri);
            // Increment counter
        }
        break;
    }
    avahi_service_resolver_free(r);
}

static void
browse_callback(AvahiServiceBrowser *b,
                AvahiIfIndex interface, AvahiProtocol protocol,
                AvahiBrowserEvent event, const char *name, const char *type,
                const char *domain, AvahiLookupResultFlags flags,
                void *user_data) {
    Browser *browser = user_data;

    switch (event) {
    case AVAHI_BROWSER_FAILURE:
        g_warning("Browser failure %s", avahi_strerror(avahi_client_errno(browser->client)));
        avahi_threaded_poll_quit(browser->poll);
        break;

    case AVAHI_BROWSER_NEW:
        if (!g_hash_table_lookup(browser->services, name)) {
            AvahiServiceResolver *resolver = avahi_service_resolver_new(
                browser->client, interface, protocol, name, type, domain,
                AVAHI_PROTO_UNSPEC, 0, resolve_callback, browser);
            if (resolver == NULL) {
                g_warning("Failed to resolve service '%s': %s\n", name, avahi_strerror(avahi_client_errno(browser->client)));
            }
        }
        break;

    case AVAHI_BROWSER_REMOVE:
        if (g_hash_table_lookup(browser->services, name)) {
            g_hash_table_remove(browser->services, name);
            g_message("Removed service '%s'", name);
            // increment counter
        }
        break;

    case AVAHI_BROWSER_CACHE_EXHAUSTED:
    case AVAHI_BROWSER_ALL_FOR_NOW:
        // nothing
        break;
    }
}

static void
client_callback(AvahiClient *c, AvahiClientState state, void *user_data) {
    Browser *browser = user_data;

    switch (state) {
    case AVAHI_CLIENT_S_RUNNING:
        g_clear_pointer(&browser->service_browser, avahi_service_browser_free);
        browser->service_browser = avahi_service_browser_new(
            c, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC,
            "_obs_source._sub._rtsp._tcp", NULL, 0, browse_callback, browser);
        if (browser->service_browser == NULL) {
            g_warning("could not create service browser: %s",
                      avahi_strerror(avahi_client_errno(c)));
            avahi_threaded_poll_quit(browser->poll);
        }
        break;

    case AVAHI_CLIENT_FAILURE:
        g_warning("Server connection failure: %s",
                  avahi_strerror(avahi_client_errno(c)));
        avahi_threaded_poll_quit(browser->poll);
        break;

    case AVAHI_CLIENT_S_REGISTERING:
    case AVAHI_CLIENT_S_COLLISION:
    case AVAHI_CLIENT_CONNECTING:
        // nothing
        break;
    }
}

Browser *
browser_new(GError **error) {
    g_autoptr(Browser) browser = g_new0(Browser, 1);
    int avahi_error = 0;

    browser->services = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, g_free);

    browser->poll = avahi_threaded_poll_new();
    browser->client = avahi_client_new(
        avahi_threaded_poll_get(browser->poll),
        AVAHI_CLIENT_NO_FAIL,
        client_callback, browser,
        &avahi_error);
    if (browser->client == NULL) {
        g_set_error(
            error, MDNS_ERROR, 0,
            "could not create Avahi client: %s", avahi_strerror(avahi_error));
        return NULL;
    }

    avahi_threaded_poll_start(browser->poll);

    return g_steal_pointer(&browser);
}

void
browser_free(Browser *browser) {
    if (browser == NULL) return;

    if (browser->poll) {
        avahi_threaded_poll_stop(browser->poll);
    }
    g_clear_pointer(&browser->services, g_hash_table_destroy);
    g_clear_pointer(&browser->service_browser, avahi_service_browser_free);
    g_clear_pointer(&browser->client, avahi_client_free);
    g_clear_pointer(&browser->poll, avahi_threaded_poll_free);
    g_free(browser);
}
