#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include <stdbool.h>

#include "app/app_config.h"

typedef struct ConfigLoadOptions {
    const char *path;
    bool        allow_missing; // if true, missing file falls back to defaults silently
} ConfigLoadOptions;

// Loads the simulation configuration from disk.
// For now, the loader simply seeds the AppConfig with defaults and logs whether
// the requested file was found. The JSON parser hook will live here later.
bool config_loader_load(AppConfig *cfg, const ConfigLoadOptions *opts);

#endif // CONFIG_LOADER_H
