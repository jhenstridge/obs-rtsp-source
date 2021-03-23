#include "source.h"
#include <glib.h>

#include "mdns-browse.h"
#include "active-notify.h"

extern MdnsBrowser *mdns_browser;
extern ActiveNotify *active_notify;

struct remote_source {
    obs_source_t *source;

    // Settings
    char *service_name;
    char *rtsp_url;
    bool hw_decode;
    gint last_stamp;

    obs_source_t *media_source;
};

static void remote_source_update(void *user_data, obs_data_t *settings);

static const char *
remote_source_get_name(void *user_data) {
    return "Remote Source";
}

static void *
remote_source_create(obs_data_t *settings, obs_source_t *source) {
    struct remote_source *remote = g_new0(struct remote_source, 1);

    remote->source = source;
    remote->service_name = g_strdup("");
    remote->rtsp_url = NULL;
    remote->media_source = obs_source_create_private(
        "ffmpeg_source", NULL, NULL);
    remote_source_update(remote, settings);

    return remote;
}

static void
remote_source_destroy(void *user_data) {
    struct remote_source *remote = user_data;

    obs_source_remove(remote->media_source);
    g_clear_pointer(&remote->media_source, obs_source_release);
    g_clear_pointer(&remote->rtsp_url, g_free);
    g_clear_pointer(&remote->service_name, g_free);

    g_free(remote);
}

static void
remote_source_get_defaults(obs_data_t *settings) {
}

static obs_properties_t *
remote_source_get_properties(void *user_data) {
    struct remote_source *remote = user_data;
    obs_properties_t *props;
    obs_property_t *service_list;
    bool service_found = false;

    props = obs_properties_create();
    obs_properties_set_flags(props, OBS_PROPERTIES_DEFER_UPDATE);

    service_list = obs_properties_add_list(
        props, "service_name", "Service",
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    if (mdns_browser) {
        g_auto(GStrv) names = mdns_browser_get_available(mdns_browser);
        int i;

        for (i = 0; names[i] != NULL; i++) {
            if (!g_strcmp0(remote->service_name, names[i])) {
                service_found = true;
            }
            obs_property_list_add_string(service_list, names[i], names[i]);
        }
    }
    if (!service_found) {
        size_t index = obs_property_list_add_string(
            service_list, remote->service_name, remote->service_name);
        obs_property_list_item_disable(service_list, index, true);
    }

    obs_properties_add_bool(props, "hw_decode",
                            "Use hardware decoding when available");

    return props;
}

static void
remote_source_update_media_source(struct remote_source *remote) {
    obs_data_t *media_settings;

    media_settings = obs_data_create();
    obs_data_set_bool(media_settings, "is_local_file", false);
    obs_data_set_string(media_settings, "input",
                        remote->rtsp_url ? remote->rtsp_url : "");
    obs_data_set_string(media_settings, "input_format", "");
    obs_data_set_int(media_settings, "reconnect_delay_sec", 10);
    obs_data_set_int(media_settings, "buffering_mb", 0);
    obs_data_set_int(media_settings, "speed_percent", 100);
    obs_data_set_bool(media_settings, "close_when_inactive", false);
    obs_data_set_bool(media_settings, "clear_on_media_end", true);
    obs_data_set_bool(media_settings, "restart_on_activate", false);
    obs_data_set_bool(media_settings, "seekable", false);
    obs_data_set_bool(media_settings, "hw_decode", remote->hw_decode);

    obs_source_update(remote->media_source, media_settings);

    obs_data_release(media_settings);
}

static void
remote_source_update(void *user_data, obs_data_t *settings) {
    struct remote_source *remote = user_data;
    g_autofree char *new_url = NULL;

    // Set RTSP URL from settings
    g_clear_pointer(&remote->service_name, g_free);
    remote->service_name = g_strdup(obs_data_get_string(settings, "service_name"));
    remote->hw_decode = obs_data_get_bool(settings, "hw_decode");

    g_clear_pointer(&remote->rtsp_url, g_free);
    if (mdns_browser) {
        remote->rtsp_url = mdns_browser_get_uri(
            mdns_browser, remote->service_name, &remote->last_stamp);
    }
    g_message("rtsp url for %s is %s", remote->service_name, remote->rtsp_url);

    remote_source_update_media_source(remote);
}

static uint32_t
remote_source_get_height(void *user_data)
{
    struct remote_source *remote = user_data;

    return obs_source_get_height(remote->media_source);
}

static uint32_t
remote_source_get_width(void *user_data)
{
    struct remote_source *remote = user_data;

    return obs_source_get_width(remote->media_source);
}

static void
remote_source_activate(void *user_data) {
    struct remote_source *remote = user_data;

    if (remote->rtsp_url != NULL) {
        active_notify_send(active_notify, remote->rtsp_url, TRUE);
    }
    obs_source_add_active_child(remote->source, remote->media_source);
}

static void
remote_source_deactivate(void *user_data) {
    struct remote_source *remote = user_data;

    if (remote->rtsp_url != NULL) {
        active_notify_send(active_notify, remote->rtsp_url, FALSE);
    }
    obs_source_remove_active_child(remote->source, remote->media_source);
}

static void
remote_source_video_tick(void *user_data, float seconds) {
    struct remote_source *remote = user_data;
    gint new_stamp;
    g_autofree char *new_url = NULL;

    if (!mdns_browser) {
        return;
    }

    // Perform quick check to see if service browser state has changed
    new_stamp = mdns_browser_get_stamp(mdns_browser);
    if (new_stamp == remote->last_stamp) {
        return;
    }

    // Is the new URL for our service different?
    new_url = mdns_browser_get_uri(
        mdns_browser, remote->service_name, &remote->last_stamp);
    if (g_strcmp0(new_url, remote->rtsp_url) != 0) {
        g_clear_pointer(&remote->rtsp_url, g_free);
        remote->rtsp_url = g_steal_pointer(&new_url);
        remote_source_update_media_source(remote);
    }
    obs_source_update_properties(remote->source);
}

static void
remote_source_enum_active_sources(void *user_data,
                                  obs_source_enum_proc_t enum_callback,
                                  void *param) {
    struct remote_source *remote = user_data;

    if (obs_source_active(remote->media_source))
        enum_callback(remote->source, remote->media_source, param);
}

static void
remote_source_enum_all_sources(void *user_data,
                               obs_source_enum_proc_t enum_callback,
                               void *param) {
    struct remote_source *remote = user_data;

    enum_callback(remote->source, remote->media_source, param);
}

static void
remote_source_video_render(void *user_data, gs_effect_t *effect) {
    struct remote_source *remote = user_data;

    obs_source_video_render(remote->media_source);
}

static bool
remote_source_audio_render(void *user_data,
                           uint64_t *ts_out,
                           struct obs_source_audio_mix *audio_output,
                           uint32_t mixers,
                           size_t channels,
                           size_t sample_rate) {
    struct remote_source *remote = user_data;
    struct obs_source_audio_mix child_audio;
    uint64_t source_ts;

    if (obs_source_audio_pending(remote->media_source))
        return false;

    source_ts = obs_source_get_audio_timestamp(remote->media_source);
    if (!source_ts)
        return false;

    obs_source_get_audio_mix(remote->media_source, &child_audio);
    for (size_t mix = 0; mix < MAX_AUDIO_MIXES; mix++) {
        if ((mixers & (1 << mix)) == 0)
            continue;

        for (size_t ch = 0; ch < channels; ch++) {
            float *out = audio_output->output[mix].data[ch];
            float *in = child_audio.output[mix].data[ch];

            memcpy(out, in,
                   AUDIO_OUTPUT_FRAMES * MAX_AUDIO_CHANNELS * sizeof(float));
        }
    }

    *ts_out = source_ts;

    return true;
}

struct obs_source_info remote_source = {
    .id = "rtsp_remote_source",
    .type = OBS_SOURCE_TYPE_INPUT,
    .icon_type = OBS_ICON_TYPE_MEDIA,
    .output_flags = (OBS_SOURCE_VIDEO | OBS_SOURCE_COMPOSITE |
                     OBS_SOURCE_DO_NOT_DUPLICATE),

    .get_name = remote_source_get_name,
    .create = remote_source_create,
    .destroy = remote_source_destroy,

    .get_defaults = remote_source_get_defaults,
    .get_properties = remote_source_get_properties,
    .update = remote_source_update,

    .get_width = remote_source_get_width,
    .get_height = remote_source_get_height,

    .activate = remote_source_activate,
    .deactivate = remote_source_deactivate,

    .video_tick = remote_source_video_tick,

    .enum_active_sources = remote_source_enum_active_sources,
    .enum_all_sources = remote_source_enum_all_sources,
    .video_render = remote_source_video_render,
    .audio_render = remote_source_audio_render,
};
