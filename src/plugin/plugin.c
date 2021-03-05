#include <obs/obs-module.h>

#include "source.h"

OBS_DECLARE_MODULE();

bool
obs_module_load(void) {
    obs_register_source(&remote_source);
    return true;
}
