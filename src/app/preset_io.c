#include "app/preset_io.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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

static void sanitize_preset_object(PresetObject *obj) {
    if (!obj) return;
    obj->position_x = clampf(isfinite(obj->position_x) ? obj->position_x : 0.5f, 0.0f, 1.0f);
    obj->position_y = clampf(isfinite(obj->position_y) ? obj->position_y : 0.5f, 0.0f, 1.0f);
    obj->size_x = clampf(isfinite(obj->size_x) ? obj->size_x : 0.05f, 0.01f, 1.0f);
    obj->size_y = clampf(isfinite(obj->size_y) ? obj->size_y : obj->size_x, 0.01f, 1.0f);
    if (!isfinite(obj->angle)) obj->angle = 0.0f;
    obj->is_static = obj->is_static ? true : false;
    if (obj->type != PRESET_OBJECT_CIRCLE && obj->type != PRESET_OBJECT_BOX) {
        obj->type = PRESET_OBJECT_CIRCLE;
    }
}

static void preset_slot_reset(CustomPresetSlot *slot, int index) {
    if (!slot) return;
    slot->occupied = false;
    snprintf(slot->name, sizeof(slot->name), "%s %d", DEFAULT_SLOT_LABEL, index + 1);
    memset(&slot->preset, 0, sizeof(slot->preset));
    slot->preset.name = slot->name;
    slot->preset.is_custom = true;
}

static bool preset_library_reserve(CustomPresetLibrary *lib, int desired) {
    if (!lib) return false;
    if (desired <= lib->slot_capacity) return true;
    int new_capacity = lib->slot_capacity > 0 ? lib->slot_capacity : CUSTOM_PRESET_LIBRARY_INITIAL_CAPACITY;
    while (new_capacity < desired) {
        new_capacity *= 2;
        if (new_capacity <= 0) {
            new_capacity = desired;
            break;
        }
    }
    CustomPresetSlot *new_slots = (CustomPresetSlot *)calloc((size_t)new_capacity, sizeof(CustomPresetSlot));
    if (!new_slots) return false;
    for (int i = 0; i < lib->slot_count; ++i) {
        new_slots[i] = lib->slots[i];
        new_slots[i].preset.name = new_slots[i].name;
    }
    for (int i = lib->slot_count; i < new_capacity; ++i) {
        preset_slot_reset(&new_slots[i], i);
    }
    free(lib->slots);
    lib->slots = new_slots;
    lib->slot_capacity = new_capacity;
    return true;
}

void preset_library_init(CustomPresetLibrary *lib) {
    if (!lib) return;
    memset(lib, 0, sizeof(*lib));
    lib->slots = NULL;
    lib->slot_capacity = 0;
    lib->slot_count = 0;
    lib->active_slot = 0;
}

void preset_library_shutdown(CustomPresetLibrary *lib) {
    if (!lib) return;
    free(lib->slots);
    lib->slots = NULL;
    lib->slot_capacity = 0;
    lib->slot_count = 0;
    lib->active_slot = 0;
}

int preset_library_count(const CustomPresetLibrary *lib) {
    if (!lib) return 0;
    return lib->slot_count;
}

CustomPresetSlot *preset_library_get_slot(CustomPresetLibrary *lib, int index) {
    if (!lib || index < 0 || index >= lib->slot_count) return NULL;
    return &lib->slots[index];
}

const CustomPresetSlot *preset_library_get_slot_const(const CustomPresetLibrary *lib,
                                                      int index) {
    if (!lib || index < 0 || index >= lib->slot_count) return NULL;
    return &lib->slots[index];
}

CustomPresetSlot *preset_library_add_slot(CustomPresetLibrary *lib,
                                          const char *name,
                                          const FluidScenePreset *preset_copy) {
    if (!lib) return NULL;
    if (!preset_library_reserve(lib, lib->slot_count + 1)) {
        return NULL;
    }

    CustomPresetSlot *slot = &lib->slots[lib->slot_count];
    preset_slot_reset(slot, lib->slot_count);
    if (name && name[0] != '\0') {
        snprintf(slot->name, sizeof(slot->name), "%s", name);
    }
    if (preset_copy) {
        slot->preset = *preset_copy;
        if (slot->preset.emitter_count > MAX_FLUID_EMITTERS) {
            slot->preset.emitter_count = MAX_FLUID_EMITTERS;
        }
        if (slot->preset.object_count > MAX_PRESET_OBJECTS) {
            slot->preset.object_count = MAX_PRESET_OBJECTS;
        }
        for (size_t e = 0; e < slot->preset.emitter_count; ++e) {
            sanitize_emitter(&slot->preset.emitters[e]);
        }
        for (size_t o = 0; o < slot->preset.object_count; ++o) {
            sanitize_preset_object(&slot->preset.objects[o]);
        }
    } else {
        memset(&slot->preset, 0, sizeof(slot->preset));
        slot->preset.is_custom = true;
    }
    slot->preset.name = slot->name;
    slot->occupied = true;
    lib->slot_count++;
    return slot;
}

bool preset_library_remove_slot(CustomPresetLibrary *lib, int index) {
    if (!lib || index < 0 || index >= lib->slot_count) return false;
    for (int i = index; i + 1 < lib->slot_count; ++i) {
        lib->slots[i] = lib->slots[i + 1];
    }
    lib->slot_count--;
    if (lib->active_slot >= lib->slot_count) {
        lib->active_slot = lib->slot_count - 1;
        if (lib->active_slot < 0) lib->active_slot = 0;
    }
    return true;
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

    if (stored_slots < 0) stored_slots = 0;
    if (!preset_library_reserve(lib, stored_slots)) {
        fclose(f);
        return false;
    }

    for (int i = 0; i < stored_slots; ++i) {
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

        CustomPresetSlot slot = {0};
        preset_slot_reset(&slot, i);
        slot.occupied = occupied != 0;
        if (name_buf[0] != '\0') {
            snprintf(slot.name, sizeof(slot.name), "%s", name_buf);
        }
        slot.preset.name = slot.name;
        slot.preset.is_custom = true;
        slot.preset.emitter_count = 0;

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
            slot.preset.emitters[e] = emitter;
            slot.preset.emitter_count++;
        }

        long marker_pos = ftell(f);
        char marker[4] = {0};
        if (fscanf(f, "%3s", marker) == 1 && strcmp(marker, "OBJ") == 0) {
            int object_count = 0;
            if (fscanf(f, "%d\n", &object_count) != 1) {
                object_count = 0;
            }
            if (object_count < 0) object_count = 0;
            if (object_count > (int)MAX_PRESET_OBJECTS) object_count = (int)MAX_PRESET_OBJECTS;
            for (int o = 0; o < object_count; ++o) {
                int type = 0;
                int is_static = 0;
                PresetObject obj = {0};
                if (fscanf(f, "%d %f %f %f %f %f %d\n",
                           &type,
                           &obj.position_x,
                           &obj.position_y,
                           &obj.size_x,
                           &obj.size_y,
                           &obj.angle,
                           &is_static) != 7) {
                    break;
                }
                obj.type = (type == PRESET_OBJECT_BOX) ? PRESET_OBJECT_BOX : PRESET_OBJECT_CIRCLE;
                obj.is_static = (is_static != 0);
                sanitize_preset_object(&obj);
                slot.preset.objects[slot.preset.object_count++] = obj;
            }
        } else {
            fseek(f, marker_pos, SEEK_SET);
        }

        lib->slots[i] = slot;
        lib->slots[i].preset.name = lib->slots[i].name;
        lib->slot_count++;
    }

    lib->active_slot = (active_slot >= 0 && active_slot < lib->slot_count)
                           ? active_slot
                           : 0;

    fclose(f);
    return true;
}

bool preset_library_save(const char *path, const CustomPresetLibrary *lib) {
    if (!path || !lib) return false;
    FILE *f = fopen(path, "w");
    if (!f) return false;

    int count = lib->slot_count;
    if (count < 0) count = 0;
    fprintf(f, "%d %d\n", lib->active_slot, count);
    for (int i = 0; i < count; ++i) {
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
        fprintf(f, "OBJ %zu\n", slot->preset.object_count);
        for (size_t o = 0; o < slot->preset.object_count; ++o) {
            const PresetObject *obj = &slot->preset.objects[o];
            fprintf(f, "%d %.6f %.6f %.6f %.6f %.6f %d\n",
                    obj->type,
                    obj->position_x,
                    obj->position_y,
                    obj->size_x,
                    obj->size_y,
                    obj->angle,
                    obj->is_static ? 1 : 0);
        }
    }

    fclose(f);
    return true;
}
