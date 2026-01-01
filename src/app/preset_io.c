#include "app/preset_io.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *DEFAULT_SLOT_LABEL = "Custom Slot";
    static const int PRESET_FILE_VERSION = 10;

static FluidSceneDomainType sanitize_domain(FluidSceneDomainType domain) {
    switch (domain) {
    case SCENE_DOMAIN_BOX:
    case SCENE_DOMAIN_WIND_TUNNEL:
        return domain;
    default:
        return SCENE_DOMAIN_BOX;
    }
}

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
    if (em->attached_object < 0 || em->attached_object >= MAX_PRESET_OBJECTS) em->attached_object = -1;
    if (em->attached_import < 0 || em->attached_import >= MAX_IMPORTED_SHAPES) em->attached_import = -1;

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
    obj->size_x = clampf(isfinite(obj->size_x) ? obj->size_x : 0.05f, 0.005f, 1.0f);
    obj->size_y = clampf(isfinite(obj->size_y) ? obj->size_y : obj->size_x, 0.005f, 1.0f);
    if (!isfinite(obj->angle)) obj->angle = 0.0f;
    obj->is_static = obj->is_static ? true : false;
    obj->gravity_enabled = obj->gravity_enabled ? true : false;
    if (obj->type != PRESET_OBJECT_CIRCLE && obj->type != PRESET_OBJECT_BOX) {
        obj->type = PRESET_OBJECT_CIRCLE;
    }
}

static void sanitize_import_shape(ImportedShape *imp) {
    if (!imp) return;
    if (imp->shape_id < -1) imp->shape_id = -1;
    // Allow wider-than-unit spans for wide/tall canvases; keep reasonable bounds.
    if (!isfinite(imp->position_x)) imp->position_x = 0.5f;
    if (!isfinite(imp->position_y)) imp->position_y = 0.5f;
    const float POS_MIN = -8.0f;
    const float POS_MAX = 8.0f;
    if (imp->position_x < POS_MIN) imp->position_x = POS_MIN;
    if (imp->position_x > POS_MAX) imp->position_x = POS_MAX;
    if (imp->position_y < POS_MIN) imp->position_y = POS_MIN;
    if (imp->position_y > POS_MAX) imp->position_y = POS_MAX;
    if (!isfinite(imp->rotation_deg)) imp->rotation_deg = 0.0f;
    if (!isfinite(imp->scale) || imp->scale <= 0.0f) imp->scale = 1.0f;
    if (!isfinite(imp->density) || imp->density <= 0.0f) imp->density = 1.0f;
    if (!isfinite(imp->friction) || imp->friction < 0.0f) imp->friction = 0.2f;
    imp->is_static = imp->is_static ? true : false;
    imp->enabled = imp->enabled && imp->path[0] != '\0';
    imp->gravity_enabled = imp->gravity_enabled ? true : false;
    if (imp->collider_vert_count < 0) imp->collider_vert_count = 0;
    if (imp->collider_vert_count > 32) imp->collider_vert_count = 32;
    if (imp->collider_part_count < 0) imp->collider_part_count = 0;
    if (imp->collider_part_count > 16) imp->collider_part_count = 16;
    for (int i = 0; i < imp->collider_part_count; ++i) {
        if (imp->collider_part_counts[i] < 0) imp->collider_part_counts[i] = 0;
        if (imp->collider_part_counts[i] > 16) imp->collider_part_counts[i] = 16;
    }
}

static void boundary_flows_reset(BoundaryFlow flows[BOUNDARY_EDGE_COUNT]) {
    if (!flows) return;
    for (int i = 0; i < BOUNDARY_EDGE_COUNT; ++i) {
        flows[i].mode = BOUNDARY_FLOW_DISABLED;
        flows[i].strength = 0.0f;
    }
}

static void boundary_flow_sanitize(BoundaryFlow *flow) {
    if (!flow) return;
    if (flow->mode < BOUNDARY_FLOW_DISABLED || flow->mode > BOUNDARY_FLOW_RECEIVE) {
        flow->mode = BOUNDARY_FLOW_DISABLED;
    }
    if (!isfinite(flow->strength) || flow->strength < 0.0f) {
        flow->strength = 0.0f;
    }
}

static void boundary_flows_assign(BoundaryFlow dst[BOUNDARY_EDGE_COUNT],
                                  const BoundaryFlow src[BOUNDARY_EDGE_COUNT]) {
    if (!dst) return;
    if (!src) {
        boundary_flows_reset(dst);
        return;
    }
    for (int i = 0; i < BOUNDARY_EDGE_COUNT; ++i) {
        dst[i] = src[i];
        boundary_flow_sanitize(&dst[i]);
    }
}

static float sanitize_dimension_value(float value) {
    if (!isfinite(value) || value <= 0.0f) {
        return 1.0f;
    }
    if (value > 64.0f) {
        return 64.0f;
    }
    return value;
}

static void preset_slot_reset(CustomPresetSlot *slot, int index) {
    if (!slot) return;
    slot->occupied = false;
    snprintf(slot->name, sizeof(slot->name), "%s %d", DEFAULT_SLOT_LABEL, index + 1);
    memset(&slot->preset, 0, sizeof(slot->preset));
    slot->preset.name = slot->name;
    slot->preset.is_custom = true;
    boundary_flows_reset(slot->preset.boundary_flows);
    slot->preset.domain = SCENE_DOMAIN_BOX;
    slot->preset.domain_width = 1.0f;
    slot->preset.domain_height = 1.0f;
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
        slot->preset.domain = sanitize_domain(preset_copy->domain);
        slot->preset.domain_width = sanitize_dimension_value(preset_copy->domain_width);
        slot->preset.domain_height = sanitize_dimension_value(preset_copy->domain_height);
        if (slot->preset.emitter_count > MAX_FLUID_EMITTERS) {
            slot->preset.emitter_count = MAX_FLUID_EMITTERS;
        }
        if (slot->preset.object_count > MAX_PRESET_OBJECTS) {
            slot->preset.object_count = MAX_PRESET_OBJECTS;
        }
        if (slot->preset.import_shape_count > MAX_IMPORTED_SHAPES) {
            slot->preset.import_shape_count = MAX_IMPORTED_SHAPES;
        }
        for (size_t e = 0; e < slot->preset.emitter_count; ++e) {
            sanitize_emitter(&slot->preset.emitters[e]);
        }
        for (size_t o = 0; o < slot->preset.object_count; ++o) {
            sanitize_preset_object(&slot->preset.objects[o]);
        }
        for (size_t s = 0; s < slot->preset.import_shape_count; ++s) {
            sanitize_import_shape(&slot->preset.import_shapes[s]);
        }
        boundary_flows_assign(slot->preset.boundary_flows,
                              preset_copy->boundary_flows);
    } else {
        memset(&slot->preset, 0, sizeof(slot->preset));
        slot->preset.is_custom = true;
        boundary_flows_reset(slot->preset.boundary_flows);
        slot->preset.domain = SCENE_DOMAIN_BOX;
        slot->preset.domain_width = 1.0f;
        slot->preset.domain_height = 1.0f;
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

        int header_vals[3] = {0};
        int read_count = fscanf(f, "%d %d %d\n",
                            &header_vals[0],
                            &header_vals[1],
                            &header_vals[2]);
        if (read_count != 3 && read_count != 2) {
            fclose(f);
            return false;
        }
        int file_version = 0;
        int active_slot = 0;
    int stored_slots = 0;
    if (read_count == 3) {
        file_version = header_vals[0];
        active_slot = header_vals[1];
        stored_slots = header_vals[2];
    } else {
        file_version = 0;
        active_slot = header_vals[0];
        stored_slots = header_vals[1];
    }

    if (stored_slots < 0) stored_slots = 0;
    if (!preset_library_reserve(lib, stored_slots)) {
        fclose(f);
        return false;
    }

    for (int i = 0; i < stored_slots; ++i) {
        int occupied = 0;
        int domain_raw = SCENE_DOMAIN_BOX;
        if (file_version >= 1) {
            if (fscanf(f, "%d %d\n", &occupied, &domain_raw) < 1) {
                break;
            }
        } else {
            if (fscanf(f, "%d\n", &occupied) != 1) {
                break;
            }
        }
        FluidSceneDomainType domain = (file_version >= 1)
                                          ? sanitize_domain((FluidSceneDomainType)domain_raw)
                                          : SCENE_DOMAIN_BOX;
        float domain_width = 1.0f;
        float domain_height = 1.0f;
        if (file_version >= 2) {
            if (fscanf(f, "%f %f\n", &domain_width, &domain_height) != 2) {
                domain_width = 1.0f;
                domain_height = 1.0f;
            }
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
        slot.preset.domain = domain;
        slot.preset.domain_width = sanitize_dimension_value(domain_width);
        slot.preset.domain_height = sanitize_dimension_value(domain_height);

        emitter_count = (emitter_count < 0) ? 0 :
                        (emitter_count > (int)MAX_FLUID_EMITTERS ? (int)MAX_FLUID_EMITTERS : emitter_count);

        for (int e = 0; e < emitter_count; ++e) {
            FluidEmitter emitter = {0};
            int type = 0;
            int attached_obj = -1;
            int attached_imp = -1;
            if (file_version >= 6) {
                if (fscanf(f, "%d %f %f %f %f %f %f %d %d\n",
                           &type,
                           &emitter.position_x,
                           &emitter.position_y,
                           &emitter.radius,
                           &emitter.strength,
                           &emitter.dir_x,
                           &emitter.dir_y,
                           &attached_obj,
                           &attached_imp) != 9) {
                    break;
                }
                emitter.attached_object = attached_obj;
                emitter.attached_import = attached_imp;
            } else if (file_version >= 5) {
                if (fscanf(f, "%d %f %f %f %f %f %f %d\n",
                           &type,
                           &emitter.position_x,
                           &emitter.position_y,
                           &emitter.radius,
                           &emitter.strength,
                           &emitter.dir_x,
                           &emitter.dir_y,
                           &attached_obj) != 8) {
                    break;
                }
                emitter.attached_object = attached_obj;
                emitter.attached_import = -1;
            } else {
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
                emitter.attached_object = -1;
                emitter.attached_import = -1;
            }
            emitter.type = (FluidEmitterType)type;
            sanitize_emitter(&emitter);
            slot.preset.emitters[e] = emitter;
            slot.preset.emitter_count++;
        }

        boundary_flows_reset(slot.preset.boundary_flows);
        long after_emitters_pos = ftell(f);
        char marker[6] = {0};
        if (fscanf(f, "%5s", marker) == 1 && strcmp(marker, "FLOW") == 0) {
            int flow_count = 0;
            if (fscanf(f, "%d\n", &flow_count) != 1) {
                flow_count = 0;
            }
            flow_count = (flow_count < 0) ? 0 :
                         (flow_count > BOUNDARY_EDGE_COUNT ? BOUNDARY_EDGE_COUNT : flow_count);
            for (int b = 0; b < flow_count; ++b) {
                int edge = 0;
                int mode = 0;
                float strength = 0.0f;
                if (fscanf(f, "%d %d %f\n", &edge, &mode, &strength) != 3) {
                    break;
                }
                if (edge < 0 || edge >= BOUNDARY_EDGE_COUNT) {
                    continue;
                }
                slot.preset.boundary_flows[edge].mode = (BoundaryFlowMode)mode;
                slot.preset.boundary_flows[edge].strength = strength;
                boundary_flow_sanitize(&slot.preset.boundary_flows[edge]);
            }
            after_emitters_pos = ftell(f);
        } else {
            fseek(f, after_emitters_pos, SEEK_SET);
        }

        long marker_pos = ftell(f);
        char obj_marker[4] = {0};
        if (fscanf(f, "%3s", obj_marker) == 1 && strcmp(obj_marker, "OBJ") == 0) {
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
                int gravity_flag = 1;
                if (file_version >= 7) {
                    if (fscanf(f, "%d %f %f %f %f %f %d %d\n",
                               &type,
                               &obj.position_x,
                               &obj.position_y,
                               &obj.size_x,
                               &obj.size_y,
                               &obj.angle,
                               &is_static,
                               &gravity_flag) != 8) {
                        break;
                    }
                } else if (fscanf(f, "%d %f %f %f %f %f %d\n",
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
                obj.gravity_enabled = (gravity_flag != 0);
                sanitize_preset_object(&obj);
                slot.preset.objects[slot.preset.object_count++] = obj;
            }
        } else {
            fseek(f, marker_pos, SEEK_SET);
        }

        long shape_marker_pos = ftell(f);
            if (file_version >= 3) {
                char shape_marker[6] = {0};
                if (fscanf(f, "%5s", shape_marker) == 1 && strcmp(shape_marker, "SHAPE") == 0) {
                    int shape_count = 0;
                    if (fscanf(f, "%d\n", &shape_count) != 1) {
                        shape_count = 0;
                }
                if (shape_count < 0) shape_count = 0;
                if (shape_count > (int)MAX_IMPORTED_SHAPES) shape_count = (int)MAX_IMPORTED_SHAPES;
                for (int s = 0; s < shape_count; ++s) {
                    ImportedShape imp = {0};
                    if (!read_line(f, imp.path, sizeof(imp.path))) {
                        break;
                    }
                    int enabled = 1;
                    if (file_version >= 4) {
                        int static_int = 0;
                        int gravity_int = 0;
                        int vert_count = 0;
                        int part_count = 0;
                        if (fscanf(f, "%f %f %f %f %d %f %f %d %d %d %d\n",
                                   &imp.position_x,
                                   &imp.position_y,
                                   &imp.scale,
                                   &imp.rotation_deg,
                                   &enabled,
                                   &imp.density,
                                   &imp.friction,
                                   &static_int,
                                   &gravity_int,
                                   &vert_count,
                                   &part_count) < 5) {
                            break;
                        }
                        imp.is_static = static_int != 0;
                        imp.gravity_enabled = gravity_int != 0;
                        imp.collider_vert_count = vert_count;
                        if (imp.collider_vert_count < 0) imp.collider_vert_count = 0;
                        if (imp.collider_vert_count > 32) imp.collider_vert_count = 32;
                        for (int vi = 0; vi < imp.collider_vert_count; ++vi) {
                            if (fscanf(f, "%f %f\n", &imp.collider_verts[vi].x, &imp.collider_verts[vi].y) != 2) {
                                imp.collider_vert_count = vi;
                                break;
                            }
                        }
                        imp.collider_part_count = part_count;
                        if (imp.collider_part_count < 0) imp.collider_part_count = 0;
                        if (imp.collider_part_count > 16) imp.collider_part_count = 16;
                        for (int pi = 0; pi < imp.collider_part_count; ++pi) {
                            int offset = 0;
                            int pcount = 0;
                            if (fscanf(f, "%d %d\n", &offset, &pcount) != 2) {
                                imp.collider_part_count = pi;
                                break;
                            }
                            imp.collider_part_offsets[pi] = offset;
                            imp.collider_part_counts[pi] = pcount;
                            if (imp.collider_part_counts[pi] < 0) imp.collider_part_counts[pi] = 0;
                            if (imp.collider_part_counts[pi] > 16) imp.collider_part_counts[pi] = 16;
                            for (int vi = 0; vi < imp.collider_part_counts[pi]; ++vi) {
                                int idx = offset + vi;
                                if (idx < 0 || idx >= 128) {
                                    imp.collider_part_counts[pi] = vi;
                                    break;
                                }
                                if (fscanf(f, "%f %f\n",
                                           &imp.collider_parts_verts[idx].x,
                                           &imp.collider_parts_verts[idx].y) != 2) {
                                    imp.collider_part_counts[pi] = vi;
                                    break;
                                }
                            }
                        }
                    } else {
                        if (fscanf(f, "%f %f %f %f %d\n",
                                   &imp.position_x,
                                   &imp.position_y,
                                   &imp.scale,
                                   &imp.rotation_deg,
                                   &enabled) != 5) {
                            break;
                        }
                        imp.density = 1.0f;
                        imp.friction = 0.2f;
                        imp.is_static = true;
                        imp.gravity_enabled = false;
                        imp.collider_vert_count = 0;
                    }
                    imp.enabled = enabled != 0;
                    imp.shape_id = -1;
                    sanitize_import_shape(&imp);
                    if (imp.path[0] != '\0') {
                        slot.preset.import_shapes[slot.preset.import_shape_count++] = imp;
                    }
                }
            } else {
                fseek(f, shape_marker_pos, SEEK_SET);
            }
        } else {
            fseek(f, shape_marker_pos, SEEK_SET);
        }

        lib->slots[i] = slot;
        lib->slots[i].preset.name = lib->slots[i].name;
        lib->slots[i].preset.domain = domain;
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
    fprintf(f, "%d %d %d\n", PRESET_FILE_VERSION, lib->active_slot, count);
    for (int i = 0; i < count; ++i) {
        const CustomPresetSlot *slot = &lib->slots[i];
        fprintf(f, "%d %d\n", slot->occupied ? 1 : 0, sanitize_domain(slot->preset.domain));
        float domain_w = sanitize_dimension_value(slot->preset.domain_width);
        float domain_h = sanitize_dimension_value(slot->preset.domain_height);
        fprintf(f, "%.6f %.6f\n", domain_w, domain_h);
        const char *name = (slot->name[0] != '\0') ? slot->name : DEFAULT_SLOT_LABEL;
        fprintf(f, "%s\n", name);
        fprintf(f, "%zu\n", slot->preset.emitter_count);
        for (size_t e = 0; e < slot->preset.emitter_count; ++e) {
            const FluidEmitter *em = &slot->preset.emitters[e];
            fprintf(f, "%d %.6f %.6f %.6f %.6f %.6f %.6f %d %d\n",
                    em->type,
                    em->position_x,
                    em->position_y,
                    em->radius,
                    em->strength,
                    em->dir_x,
                    em->dir_y,
                    em->attached_object,
                    em->attached_import);
        }
        fprintf(f, "FLOW %d\n", BOUNDARY_EDGE_COUNT);
        for (int edge = 0; edge < BOUNDARY_EDGE_COUNT; ++edge) {
            const BoundaryFlow *flow = &slot->preset.boundary_flows[edge];
            fprintf(f, "%d %d %.6f\n",
                    edge,
                    flow->mode,
                    flow->strength);
        }
        fprintf(f, "OBJ %zu\n", slot->preset.object_count);
        for (size_t o = 0; o < slot->preset.object_count; ++o) {
            const PresetObject *obj = &slot->preset.objects[o];
            fprintf(f, "%d %.6f %.6f %.6f %.6f %.6f %d %d\n",
                    obj->type,
                    obj->position_x,
                    obj->position_y,
                    obj->size_x,
                    obj->size_y,
                    obj->angle,
                    obj->is_static ? 1 : 0,
                    obj->gravity_enabled ? 1 : 0);
        }
        size_t shape_count = slot->preset.import_shape_count;
        if (shape_count > MAX_IMPORTED_SHAPES) shape_count = MAX_IMPORTED_SHAPES;
        fprintf(f, "SHAPE %zu\n", shape_count);
        for (size_t s = 0; s < shape_count; ++s) {
            ImportedShape imp = slot->preset.import_shapes[s];
            sanitize_import_shape(&imp);
            fprintf(f, "%s\n", imp.path);
            fprintf(f, "%.6f %.6f %.6f %.6f %d %.6f %.6f %d %d %d %d\n",
                    imp.position_x,
                    imp.position_y,
                    imp.scale,
                    imp.rotation_deg,
                    imp.enabled ? 1 : 0,
                    imp.density,
                    imp.friction,
                    imp.is_static ? 1 : 0,
                    imp.gravity_enabled ? 1 : 0,
                    imp.collider_vert_count,
                    imp.collider_part_count);
            for (int vi = 0; vi < imp.collider_vert_count; ++vi) {
                fprintf(f, "%.6f %.6f\n", imp.collider_verts[vi].x, imp.collider_verts[vi].y);
            }
            for (int pi = 0; pi < imp.collider_part_count; ++pi) {
                fprintf(f, "%d %d\n", imp.collider_part_offsets[pi], imp.collider_part_counts[pi]);
                int offset = imp.collider_part_offsets[pi];
                for (int vi = 0; vi < imp.collider_part_counts[pi]; ++vi) {
                    int idx = offset + vi;
                    if (idx < 0 || idx >= 128) break;
                    fprintf(f, "%.6f %.6f\n",
                            imp.collider_parts_verts[idx].x,
                            imp.collider_parts_verts[idx].y);
                }
            }
        }
    }

    fclose(f);
    return true;
}
