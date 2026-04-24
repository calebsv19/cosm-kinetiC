#include "render/font_bridge.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core_font.h"
#include "font_paths.h"
#include "render/text_draw.h"
#include "render/text_upload_policy.h"

typedef struct PhysicsSimFontBridgeSlotSpec {
    CoreFontRoleId role_id;
    CoreFontTextSizeTier text_tier;
    int min_point_size;
    const char *const *legacy_paths;
    size_t legacy_path_count;
} PhysicsSimFontBridgeSlotSpec;

static const char *const k_title_paths[] = {
    FONT_TITLE_PATH_1,
    FONT_TITLE_PATH_2
};

static const char *const k_body_paths[] = {
    FONT_BODY_PATH_1,
    FONT_BODY_PATH_2
};

static const char *const k_body_then_title_paths[] = {
    FONT_BODY_PATH_1,
    FONT_BODY_PATH_2,
    FONT_TITLE_PATH_1,
    FONT_TITLE_PATH_2
};

static CoreResult physics_sim_font_bridge_invalid(const char *message) {
    CoreResult r = { CORE_ERR_INVALID_ARG, message };
    return r;
}

static int physics_sim_font_bridge_parse_bool_env(const char *value, int *out_value) {
    char lowered[16];
    size_t i = 0;

    if (!value || !value[0] || !out_value) {
        return 0;
    }

    for (; value[i] && i < sizeof(lowered) - 1; ++i) {
        lowered[i] = (char)tolower((unsigned char)value[i]);
    }
    lowered[i] = '\0';

    if (strcmp(lowered, "1") == 0 || strcmp(lowered, "true") == 0 || strcmp(lowered, "yes") == 0 ||
        strcmp(lowered, "on") == 0) {
        *out_value = 1;
        return 1;
    }
    if (strcmp(lowered, "0") == 0 || strcmp(lowered, "false") == 0 || strcmp(lowered, "no") == 0 ||
        strcmp(lowered, "off") == 0) {
        *out_value = 0;
        return 1;
    }
    return 0;
}

static int physics_sim_font_bridge_shared_enabled(void) {
    int enabled = 1;

    if (physics_sim_font_bridge_parse_bool_env(getenv("PHYSICS_SIM_USE_SHARED_FONT"), &enabled)) {
        return enabled;
    }
    if (physics_sim_font_bridge_parse_bool_env(getenv("PHYSICS_SIM_USE_SHARED_THEME_FONT"), &enabled)) {
        return enabled;
    }
    return 1;
}

static int physics_sim_font_bridge_path_is_absolute(const char *path) {
    if (!path || !path[0]) {
        return 0;
    }
#if defined(_WIN32)
    if (((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) &&
        path[1] == ':' &&
        (path[2] == '\\' || path[2] == '/')) {
        return 1;
    }
#endif
    return path[0] == '/';
}

static void physics_sim_font_bridge_copy_text(char *dst, size_t dst_cap, const char *src) {
    if (!dst || dst_cap == 0) {
        return;
    }
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_cap - 1);
    dst[dst_cap - 1] = '\0';
}

static TTF_Font *physics_sim_font_bridge_try_open_font_path(const char *font_path,
                                                            int point_size,
                                                            char *out_path,
                                                            size_t out_path_cap) {
    TTF_Font *font = NULL;
    char *base_path = NULL;
    int depth = 0;

    if (!font_path || !font_path[0] || point_size < 1) {
        return NULL;
    }

    font = TTF_OpenFont(font_path, point_size);
    if (font) {
        physics_sim_font_bridge_copy_text(out_path, out_path_cap, font_path);
        return font;
    }

    if (strncmp(font_path, "shared/", 7) == 0) {
        char adjusted[384];
        snprintf(adjusted, sizeof(adjusted), "../%s", font_path);
        font = TTF_OpenFont(adjusted, point_size);
        if (font) {
            physics_sim_font_bridge_copy_text(out_path, out_path_cap, adjusted);
            return font;
        }
    }

    if (physics_sim_font_bridge_path_is_absolute(font_path)) {
        return NULL;
    }

    base_path = SDL_GetBasePath();
    if (!base_path || !base_path[0]) {
        if (base_path) {
            SDL_free(base_path);
        }
        return NULL;
    }

    for (depth = 0; depth <= 8; ++depth) {
        char candidate[2048];
        int written = snprintf(candidate, sizeof(candidate), "%s", base_path);
        int offset = written;
        int i = 0;

        if (written <= 0 || (size_t)written >= sizeof(candidate)) {
            continue;
        }

        for (i = 0; i < depth; ++i) {
            written = snprintf(candidate + offset,
                               sizeof(candidate) - (size_t)offset,
                               "../");
            if (written <= 0 || (size_t)written >= (sizeof(candidate) - (size_t)offset)) {
                offset = -1;
                break;
            }
            offset += written;
        }
        if (offset < 0) {
            continue;
        }

        written = snprintf(candidate + offset,
                           sizeof(candidate) - (size_t)offset,
                           "%s",
                           font_path);
        if (written <= 0 || (size_t)written >= (sizeof(candidate) - (size_t)offset)) {
            continue;
        }

        font = TTF_OpenFont(candidate, point_size);
        if (font) {
            physics_sim_font_bridge_copy_text(out_path, out_path_cap, candidate);
            SDL_free(base_path);
            return font;
        }
    }

    SDL_free(base_path);
    return NULL;
}

static CoreFontPresetId physics_sim_font_bridge_font_preset_id(void) {
    const char *preset_name = getenv("PHYSICS_SIM_FONT_PRESET");
    CoreFontPreset preset;

    if (preset_name && preset_name[0] &&
        core_font_get_preset_by_name(preset_name, &preset).code == CORE_OK) {
        return preset.id;
    }
    return CORE_FONT_PRESET_IDE;
}

static CoreResult physics_sim_font_bridge_context_init(KitRenderContext *ctx,
                                                       const AppConfig *cfg) {
    CoreResult result;

    if (!ctx) {
        return physics_sim_font_bridge_invalid("null context");
    }

    result = kit_render_context_init(ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_GREYSCALE,
                                     physics_sim_font_bridge_font_preset_id());
    if (result.code != CORE_OK) {
        return result;
    }

    if (cfg) {
        result = kit_render_set_text_zoom_step(ctx, cfg->text_zoom_step);
        if (result.code != CORE_OK) {
            kit_render_context_shutdown(ctx);
            return result;
        }
    }

    return core_result_ok();
}

static int physics_sim_font_bridge_hinting_sdl(KitRenderTextHintingMode hinting) {
    switch (hinting) {
        case KIT_RENDER_TEXT_HINTING_LIGHT:
            return TTF_HINTING_LIGHT;
        case KIT_RENDER_TEXT_HINTING_DEFAULT:
        default:
            return TTF_HINTING_NORMAL;
    }
}

static void physics_sim_font_bridge_apply_quality(TTF_Font *font,
                                                  const KitRenderResolvedTextRun *text_run) {
    if (!font || !text_run) {
        return;
    }
    TTF_SetFontStyle(font, TTF_STYLE_NORMAL);
    TTF_SetFontHinting(font, physics_sim_font_bridge_hinting_sdl(text_run->hinting));
    TTF_SetFontKerning(font, text_run->kerning_enabled);
}

static const PhysicsSimFontBridgeSlotSpec *physics_sim_font_bridge_slot_spec(PhysicsSimFontSlot slot) {
    static const PhysicsSimFontBridgeSlotSpec specs[] = {
        {
            CORE_FONT_ROLE_UI_BOLD,
            CORE_FONT_TEXT_SIZE_TITLE,
            8,
            k_title_paths,
            sizeof(k_title_paths) / sizeof(k_title_paths[0])
        },
        {
            CORE_FONT_ROLE_UI_MEDIUM,
            CORE_FONT_TEXT_SIZE_BASIC,
            6,
            k_body_paths,
            sizeof(k_body_paths) / sizeof(k_body_paths[0])
        },
        {
            CORE_FONT_ROLE_UI_MEDIUM,
            CORE_FONT_TEXT_SIZE_CAPTION,
            6,
            k_body_paths,
            sizeof(k_body_paths) / sizeof(k_body_paths[0])
        },
        {
            CORE_FONT_ROLE_UI_MEDIUM,
            CORE_FONT_TEXT_SIZE_BASIC,
            8,
            k_body_then_title_paths,
            sizeof(k_body_then_title_paths) / sizeof(k_body_then_title_paths[0])
        },
        {
            CORE_FONT_ROLE_UI_MEDIUM,
            CORE_FONT_TEXT_SIZE_HEADER,
            8,
            k_body_then_title_paths,
            sizeof(k_body_then_title_paths) / sizeof(k_body_then_title_paths[0])
        },
        {
            CORE_FONT_ROLE_UI_MEDIUM,
            CORE_FONT_TEXT_SIZE_BASIC,
            6,
            k_body_then_title_paths,
            sizeof(k_body_then_title_paths) / sizeof(k_body_then_title_paths[0])
        },
        {
            CORE_FONT_ROLE_UI_MEDIUM,
            CORE_FONT_TEXT_SIZE_HEADER,
            6,
            k_body_then_title_paths,
            sizeof(k_body_then_title_paths) / sizeof(k_body_then_title_paths[0])
        }
    };
    if ((int)slot < 0 || (size_t)slot >= (sizeof(specs) / sizeof(specs[0]))) {
        return NULL;
    }
    return &specs[(size_t)slot];
}

CoreResult physics_sim_font_bridge_open(SDL_Renderer *renderer,
                                        const AppConfig *cfg,
                                        PhysicsSimFontSlot slot,
                                        TTF_Font **out_font,
                                        PhysicsSimResolvedFont *out_resolved) {
    const PhysicsSimFontBridgeSlotSpec *spec = physics_sim_font_bridge_slot_spec(slot);
    KitRenderContext render_ctx;
    KitRenderResolvedTextRun text_run;
    CoreResult result;
    TTF_Font *font = NULL;
    size_t i = 0;
    float render_scale = 1.0f;
    char resolved_path[384] = {0};

    if (!spec || !out_font) {
        return physics_sim_font_bridge_invalid("invalid font bridge request");
    }

    result = physics_sim_font_bridge_context_init(&render_ctx, cfg);
    if (result.code != CORE_OK) {
        return result;
    }

    if (renderer) {
        render_scale = physics_sim_text_raster_scale(renderer);
    }

    result = kit_render_resolve_text_run(&render_ctx,
                                         spec->role_id,
                                         spec->text_tier,
                                         render_scale,
                                         &text_run);
    if (result.code != CORE_OK) {
        kit_render_context_shutdown(&render_ctx);
        return result;
    }
    if (text_run.logical_point_size < spec->min_point_size) {
        text_run.logical_point_size = spec->min_point_size;
    }
    if (text_run.raster_point_size < text_run.logical_point_size) {
        text_run.raster_point_size = text_run.logical_point_size;
    }
    text_run.raster_scale =
        (text_run.logical_point_size > 0)
            ? ((float)text_run.raster_point_size / (float)text_run.logical_point_size)
            : 1.0f;
    text_run.upload_filter =
        (text_run.raster_scale > 1.0f) ? KIT_RENDER_TEXT_UPLOAD_FILTER_NEAREST
                                       : KIT_RENDER_TEXT_UPLOAD_FILTER_LINEAR;

    if (physics_sim_font_bridge_shared_enabled()) {
        const char *font_paths[2] = {
            text_run.role_spec.primary_path,
            text_run.role_spec.fallback_path
        };
        for (i = 0; i < 2; ++i) {
            resolved_path[0] = '\0';
            font = physics_sim_font_bridge_try_open_font_path(font_paths[i],
                                                              text_run.raster_point_size,
                                                              resolved_path,
                                                              sizeof(resolved_path));
            if (font) {
                physics_sim_font_bridge_apply_quality(font, &text_run);
                physics_sim_text_register_font_source(font,
                                                      resolved_path[0] ? resolved_path : font_paths[i],
                                                      text_run.logical_point_size,
                                                      text_run.raster_point_size,
                                                      text_run.kerning_enabled);
                if (out_resolved) {
                    physics_sim_font_bridge_copy_text(out_resolved->resolved_path,
                                                      sizeof(out_resolved->resolved_path),
                                                      resolved_path[0] ? resolved_path : font_paths[i]);
                    out_resolved->text_run = text_run;
                    out_resolved->used_shared_font = 1;
                }
                *out_font = font;
                kit_render_context_shutdown(&render_ctx);
                return core_result_ok();
            }
        }
    }

    for (i = 0; i < spec->legacy_path_count; ++i) {
        resolved_path[0] = '\0';
        font = physics_sim_font_bridge_try_open_font_path(spec->legacy_paths[i],
                                                          text_run.raster_point_size,
                                                          resolved_path,
                                                          sizeof(resolved_path));
        if (font) {
            physics_sim_font_bridge_apply_quality(font, &text_run);
            physics_sim_text_register_font_source(font,
                                                  resolved_path[0] ? resolved_path : spec->legacy_paths[i],
                                                  text_run.logical_point_size,
                                                  text_run.raster_point_size,
                                                  text_run.kerning_enabled);
            if (out_resolved) {
                physics_sim_font_bridge_copy_text(out_resolved->resolved_path,
                                                  sizeof(out_resolved->resolved_path),
                                                  resolved_path[0] ? resolved_path : spec->legacy_paths[i]);
                out_resolved->text_run = text_run;
                out_resolved->used_shared_font = 0;
            }
            *out_font = font;
            kit_render_context_shutdown(&render_ctx);
            return core_result_ok();
        }
    }

    if (out_resolved) {
        memset(out_resolved, 0, sizeof(*out_resolved));
        out_resolved->text_run = text_run;
    }
    *out_font = NULL;
    kit_render_context_shutdown(&render_ctx);
    return physics_sim_font_bridge_invalid("failed to open bridge font");
}

void physics_sim_font_bridge_close(TTF_Font **font) {
    if (!font || !*font) {
        return;
    }
    physics_sim_text_unregister_font_source(*font);
    TTF_CloseFont(*font);
    *font = NULL;
}
