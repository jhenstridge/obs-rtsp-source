#include "mdns-publish.h"

#include <gio/gio.h>
#include <avahi-glib/glib-malloc.h>
#include <avahi-glib/glib-watch.h>
#include <avahi-client/client.h>
#include <avahi-client/publish.h>
#include <avahi-common/alternative.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/error.h>

struct _MdnsPublisher {
    AvahiGLibPoll *glib_poll;
    AvahiClient *client;

    int port;
    GHashTable *services;
};

typedef struct _Service Service;

static void service_free(Service *service);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(Service, service_free);

struct _Service {
    MdnsPublisher *publisher;
    char *path;
    char *name;
    AvahiEntryGroup *group;
};

static Service *
service_new(MdnsPublisher *publisher, const char *path, const char *name) {
    g_autoptr(Service) service = g_new0(Service, 1);

    service->publisher = publisher;
    service->path = g_strdup(path);
    service->name = g_strdup(name);

    return g_steal_pointer(&service);
}

static void
service_free(Service *service) {
    if (service == NULL) return;

    g_clear_pointer(&service->path, g_free);
    g_clear_pointer(&service->name, g_free);
    g_clear_pointer(&service->group, avahi_entry_group_free);
}

static void entry_group_callback(AvahiEntryGroup *g,
                                 AvahiEntryGroupState state,
                                 void *user_data);

static void
service_register(Service *service) {
    int ret;
    char *alt_name;

    if (!service->group) {
        service->group = avahi_entry_group_new(
            service->publisher->client, entry_group_callback, service);
        if (!service->group) {
            g_warning(
                "could not create entry group: %s",
                avahi_strerror(avahi_client_errno(service->publisher->client)));
            return;
        }
    }


register_services:
    if (avahi_entry_group_is_empty(service->group)) {
        g_autofree char *txt = NULL;

        txt = g_strdup_printf("path=%s", service->path);
        if ((ret = avahi_entry_group_add_service(
                 service->group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0,
                 service->name, "_rtsp._tcp", NULL, NULL,
                 service->publisher->port, txt, NULL)) < 0) {
            if (ret == AVAHI_ERR_COLLISION)
                goto collision;
            g_warning("could not add service: %s", avahi_strerror(ret));
            return;
        }

        if ((ret = avahi_entry_group_add_service_subtype(
                 service->group, AVAHI_IF_UNSPEC, AVAHI_PROTO_UNSPEC, 0,
                 service->name, "_rtsp._tcp", NULL,
                 "_obs-source._sub._rtsp._tcp")) < 0) {
            g_warning("could not add service subtype: %s", avahi_strerror(ret));
            return;
        }

        if ((ret = avahi_entry_group_commit(service->group)) < 0) {
            g_warning(
                "could not commit entry group: %s",
                avahi_strerror(avahi_client_errno(service->publisher->client)));
            return;
        }
    }

    return;

collision:
    alt_name = avahi_alternative_service_name(service->name);
    g_free(service->name);
    service->name = alt_name;
    avahi_entry_group_reset(service->group);
    goto register_services;
}

static void
service_reset(Service *service) {
    if (service->group)
        avahi_entry_group_reset(service->group);
}

static void
entry_group_callback(AvahiEntryGroup *g, AvahiEntryGroupState state,
                     void *user_data) {
    Service *service = user_data;
    char *alt_name;

    switch (state) {
    case AVAHI_ENTRY_GROUP_ESTABLISHED:
    case AVAHI_ENTRY_GROUP_UNCOMMITED:
    case AVAHI_ENTRY_GROUP_REGISTERING:
        // nothing to do
        break;
    case AVAHI_ENTRY_GROUP_COLLISION:
        // collision with remote name: pick a new name and re-register
        alt_name = avahi_alternative_service_name(service->name);
        g_free(service->name);
        service->name = alt_name;
        service_register(service);
        break;
    case AVAHI_ENTRY_GROUP_FAILURE:
        g_warning("entry group failure: %s", avahi_strerror(avahi_client_errno(service->publisher->client)));
        break;
    }
}

static void
publisher_register_services(MdnsPublisher *publisher) {
    GHashTableIter iter;
    Service *service;

    g_hash_table_iter_init(&iter, publisher->services);
    while (g_hash_table_iter_next(&iter, NULL, (void **)&service)) {
        service_register(service);
    }
}

static void
publisher_reset_services(MdnsPublisher *publisher) {
    GHashTableIter iter;
    Service *service;

    g_hash_table_iter_init(&iter, publisher->services);
    while (g_hash_table_iter_next(&iter, NULL, (void **)&service)) {
        service_reset(service);
    }
}

static void
client_callback(AvahiClient *client, AvahiClientState state, void *user_data) {
    MdnsPublisher *publisher = user_data;

    switch (state) {
    case AVAHI_CLIENT_S_RUNNING:
        // Server started: register all services
        publisher_register_services(publisher);
        break;

    case AVAHI_CLIENT_S_COLLISION:
    case AVAHI_CLIENT_S_REGISTERING:
        // Network issues: reset services and wait until we're running again
        publisher_reset_services(publisher);
        break;

    case AVAHI_CLIENT_CONNECTING:
        // nothing
        break;

    case AVAHI_CLIENT_FAILURE:
        // Something went wrong
        g_warning("Avahi client failure: %s",
                  avahi_strerror(avahi_client_errno(client)));
        break;
    }
}

MdnsPublisher *
mdns_publisher_new(int port, GError **error) {
    g_autoptr(MdnsPublisher) publisher = g_new0(MdnsPublisher, 1);
    int avahi_error = 0;

    avahi_set_allocator(avahi_glib_allocator());

    publisher->port = port;
    publisher->services = g_hash_table_new_full(
        g_str_hash, g_str_equal,
        NULL, (GDestroyNotify)service_free);

    publisher->glib_poll = avahi_glib_poll_new (NULL, G_PRIORITY_DEFAULT);
    publisher->client = avahi_client_new(
        avahi_glib_poll_get(publisher->glib_poll),
        AVAHI_CLIENT_NO_FAIL,
        client_callback, publisher,
        &avahi_error);
    if (publisher->client == NULL) {
        g_set_error(
            error, G_IO_ERROR, G_IO_ERROR_FAILED,
            "could not create MDNS client: %s", avahi_strerror(avahi_error));
        return NULL;
    }

    return g_steal_pointer(&publisher);
}

void
mdns_publisher_free(MdnsPublisher *publisher) {
    if (publisher == NULL) return;

    g_clear_pointer(&publisher->services, g_hash_table_destroy);
    g_clear_pointer(&publisher->client, avahi_client_free);
    g_clear_pointer(&publisher->glib_poll, avahi_glib_poll_free);
    g_free(publisher);
}

void
mdns_publisher_add_stream(MdnsPublisher *publisher, const char *path, const char *name) {
    Service *service = NULL;

    if (g_hash_table_lookup(publisher->services, path)) {
        g_warning("Cannot publish the same path multiple times");
        return;
    }

    service = service_new(publisher, path, name);
    g_hash_table_insert(publisher->services, service->path, service);
    if (avahi_client_get_state(publisher->client) == AVAHI_CLIENT_S_RUNNING)
        service_register(service);
}
