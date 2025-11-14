#include "app/preset_io.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *DEFAULT_SLOT_LABEL = "Custom Slot";

static float clampf(float v, float min_v, float max_v) {
    if (v < min_v) return min_v;
    if (v > max_v) return max_v;
    return v;
}

static void sanitize_emitter(FluidEmitter *em) {
    if (!em) return;
    if (!isfinite(em->position_x)) em->position_x = 0.5f;
    if (!isfinite(em->position_y)) em->position_y = 0.5f;
    em->position_x = clampf(em->position_x, 0.0f, 1.0f);
    em->position_y = clampf(em->position_y, 0.0f, 1.0f);

    if (!isfinite(em->radius) || em->radius < 0.01f) em->radius = 0.08f;
    if (em->radius > 0.6f) em->radius = 0.6f;

    if (!isfinite(em->strength)) {
        em->strength = 0.0f;
    }

    float dx = em->dir_x;
    float dy = em->dir_y;
    if (!isfinite(dx) || !isfinite(dy)) {
        em->dir_x = 0.0f;
        em->dir_y = -1.0f;
        return;
    }
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-4f) {
        em->dir_x = 0.0f;
        em->dir_y = -1.0f;
        return;
    }
    em->dir_x = dx / len;
    em->dir_y = dy / len;
}

static void preset_slot_reset(CustomPresetSlot *slot, int index) {
    if (!slot) return;
    slot->occupied = false;
    snprintf(slot->name, sizeof(slot->name), "%s %d", DEFAULT_SLOT_LABEL, index + 1);
    memset(&slot->preset, 0, sizeof(slot->preset));
    slot->preset.name = slot->name;
    slot->preset.is_custom = true;
}

void preset_library_init(CustomPresetLibrary *lib) {
    if (!lib) return;
    lib->active_slot = 0;
    for (int i = 0; i < CUSTOM_PRESET_SLOT_COUNT; ++i) {
        preset_slot_reset(&lib->slots[i], i);
    }
}

CustomPresetSlot *preset_library_get_slot(CustomPresetLibrary *lib, int index) {
    if (!lib || index < 0 || index >= CUSTOM_PRESET_SLOT_COUNT) return NULL;
    return &lib->slots[index];
}

const CustomPresetSlot *preset_library_get_slot_const(const CustomPresetLibrary *lib,
                                                      int index) {
    if (!lib || index < 0 || index >= CUSTOM_PRESET_SLOT_COUNT) return NULL;
    return &lib->slots[index];
}

static bool read_line(FILE *f, char *buffer, size_t buffer_size) {
    if (!f || !buffer || buffer_size == 0) return false;
    if (!fgets(buffer, (int)buffer_size, f)) {
        return false;
    }
    size_t len = strlen(buffer);
    if (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) {
        buffer[--len] = '\0';
        if (len > 0 && buffer[len - 1] == '\r') {
            buffer[--len] = '\0';
        }
    }
    return true;
}

bool preset_library_load(const char *path, CustomPresetLibrary *lib) {
    if (!lib) return false;
    preset_library_init(lib);
    if (!path) return false;
    FILE *f = fopen(path, "r");
    if (!f) return false;

    int active_slot = 0;
    int stored_slots = 0;
    if (fscanf(f, "%d %d\n", &active_slot, &stored_slots) != 2) {
        fclose(f);
        return false;
    }

    lib->active_slot = (active_slot >= 0 && active_slot < CUSTOM_PRESET_SLOT_COUNT)
                           ? active_slot
                           : 0;

    for (int i = 0; i < stored_slots && i < CUSTOM_PRESET_SLOT_COUNT; ++i) {
        int occupied = 0;
        if (fscanf(f, "%d\n", &occupied) != 1) {
            break;
        }
        char name_buf[CUSTOM_PRESET_NAME_MAX] = {0};
        if (!read_line(f, name_buf, sizeof(name_buf))) {
            break;
        }
        int emitter_count = 0;
        if (fscanf(f, "%d\n", &emitter_count) != 1) {
            break;
        }
        CustomPresetSlot *slot = &lib->slots[i];
        slot->occupied = occupied != 0;
        if (name_buf[0] != '\0') {
            snprintf(slot->name, sizeof(slot->name), "%s", name_buf);
        } else {
            snprintf(slot->name, sizeof(slot->name), "%s %d", DEFAULT_SLOT_LABEL, i + 1);
        }
        slot->preset.name = slot->name;
        slot->preset.is_custom = true;
        slot->preset.emitter_count = 0;

        emitter_count = (emitter_count < 0) ? 0 :
                        (emitter_count > (int)MAX_FLUID_EMITTERS ? (int)MAX_FLUID_EMITTERS : emitter_count);

        for (int e = 0; e < emitter_count; ++e) {
            FluidEmitter emitter = {0};
            int type = 0;
            if (fscanf(f, "%d %f %f %f %f %f %f\n",
                       &type,
                       &emitter.position_x,
                       &emitter.position_y,
                       &emitter.radius,
                       &emitter.strength,
                       &emitter.dir_x,
                       &emitter.dir_y) != 7) {
                break;
            }
            emitter.type = (FluidEmitterType)type;
            sanitize_emitter(&emitter);
            slot->preset.emitters[e] = emitter;
            slot->preset.emitter_count++;
        }
    }

    fclose(f);
    return true;
}

bool preset_library_save(const char *path, const CustomPresetLibrary *lib) {
    if (!path || !lib) return false;
    FILE *f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "%d %d\n", lib->active_slot, CUSTOM_PRESET_SLOT_COUNT);
    for (int i = 0; i < CUSTOM_PRESET_SLOT_COUNT; ++i) {
        const CustomPresetSlot *slot = &lib->slots[i];
        fprintf(f, "%d\n", slot->occupied ? 1 : 0);
        const char *name = (slot->name[0] != '\0') ? slot->name : DEFAULT_SLOT_LABEL;
        fprintf(f, "%s\n", name);
        fprintf(f, "%zu\n", slot->preset.emitter_count);
        for (size_t e = 0; e < slot->preset.emitter_count; ++e) {
            const FluidEmitter *em = &slot->preset.emitters[e];
            fprintf(f, "%d %.6f %.6f %.6f %.6f %.6f %.6f\n",
                    em->type,
                    em->position_x,
                    em->position_y,
                    em->radius,
                    em->strength,
                    em->dir_x,
                    em->dir_y);
        }
    }

    fclose(f);
    return true;
}
