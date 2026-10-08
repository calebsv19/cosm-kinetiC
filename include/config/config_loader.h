#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include <stdbool.h>

#include "app/app_config.h"

typedef struct ConfigLoadOptions {
    const char *path;
    bool        allow_missing; // if true, missing file falls back to defaults silently
} ConfigLoadOptions;

/* Seeds defaults, then consumes an admitted bounded regular configuration file.
 * allow_missing accepts only actual absence after full path syntax admission.
 * Linked/special/hardlinked/empty/oversized/changed/NUL-containing inputs hold.
 * Strict bounded JSON object and optional known-field representations are checked.
 * Application physical ranges and cross-field relations remain separate. */
bool config_loader_load(AppConfig *cfg, const ConfigLoadOptions *opts);
bool config_loader_save(const AppConfig *cfg, const char *path);

#endif // CONFIG_LOADER_H
