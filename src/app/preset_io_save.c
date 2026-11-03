#include "app/preset_io_internal.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "app/physics_sim_persistence.h"

static const char *DEFAULT_SLOT_LABEL = "Custom Slot";
static const int PRESET_FILE_VERSION = 15;

bool preset_library_save(const char *path, const CustomPresetLibrary *lib) {
    if (!path || !lib || lib->slot_count < 0 || (size_t)lib->slot_count > PRESET_MAX_SLOTS || lib->slot_capacity < lib->slot_count ||
        (lib->slot_count > 0 && !lib->slots)) return false;
    for (int i = 0; i < lib->slot_count; ++i) {
        const CustomPresetSlot *slot = &lib->slots[i];
        if (slot->preset.emitter_count > MAX_FLUID_EMITTERS ||
            slot->preset.object_count > MAX_PRESET_OBJECTS ||
            !memchr(slot->name, 0, sizeof(slot->name)) ||
            !memchr(slot->preset.structural_scene_path, 0, sizeof(slot->preset.structural_scene_path)) ||
            strpbrk(slot->name, "\r\n") || strpbrk(slot->preset.structural_scene_path, "\r\n")) return false;
        for (size_t j = 0; j < slot->preset.object_count; ++j) {
            const PresetObject *obj = &slot->preset.objects[j];
            if (!isfinite(obj->position_x) || !isfinite(obj->position_y) ||
                !isfinite(obj->position_z) || !isfinite(obj->size_x) ||
                !isfinite(obj->size_y) || !isfinite(obj->size_z) || !isfinite(obj->angle)) return false;
        }
        for (size_t j = 0; j < BOUNDARY_EDGE_COUNT; ++j)
            if (!isfinite(slot->preset.boundary_flows[j].strength)) return false;
        size_t shapes = slot->preset.import_shape_count;
        if (shapes > MAX_IMPORTED_SHAPES) return false;
        for (size_t j = 0; j < shapes; ++j) {
            const ImportedShape *shape = &slot->preset.import_shapes[j];
            if (!memchr(shape->path, 0, sizeof(shape->path)) || !shape->path[0] ||
                strpbrk(shape->path, "\r\n") || shape->collider_part_count < 0 ||
                shape->collider_part_count > 16) return false;
            if (shape->collider_vert_count < 0 || shape->collider_vert_count > 32) return false;
            for (int k = 0; k < shape->collider_vert_count; ++k)
                if (!isfinite(shape->collider_verts[k].x) || !isfinite(shape->collider_verts[k].y)) return false;
            for (int k = 0; k < shape->collider_part_count; ++k) {
                int offset = shape->collider_part_offsets[k], count = shape->collider_part_counts[k];
                if (offset < 0 || offset >= 128 || count < 0 || count > 16 || count > 128 - offset) return false;
                for (int v = offset; v < offset + count; ++v)
                    if (!isfinite(shape->collider_parts_verts[v].x) ||
                        !isfinite(shape->collider_parts_verts[v].y)) return false;
            }
        }
    }
    PhysicsSimPersistence save;
    FILE *f = physics_sim_persistence_begin(path, &save);
    if (!f) return false;

    int count = lib->slot_count;
    fprintf(f, "%d %d %d\n", PRESET_FILE_VERSION, lib->active_slot, count);
    for (int i = 0; i < count; ++i) {
        const CustomPresetSlot *slot = &lib->slots[i];
        fprintf(f, "%d %d\n", slot->occupied ? 1 : 0, sanitize_domain(slot->preset.domain));
        float domain_w = sanitize_dimension_value(slot->preset.domain_width);
        float domain_h = sanitize_dimension_value(slot->preset.domain_height);
        fprintf(f, "%.9g %.9g\n", domain_w, domain_h);
        fprintf(f, "%d\n", (int)sanitize_dimension_mode(slot->preset.dimension_mode));
        const char *name = (slot->name[0] != '\0') ? slot->name : DEFAULT_SLOT_LABEL;
        fprintf(f, "%s\n", name);
        fprintf(f, "%s\n", slot->preset.structural_scene_path);
        AtmosphericPresetSettings atmosphere = slot->preset.atmosphere;
        sanitize_atmosphere(&atmosphere);
        fprintf(f,
                "ATMOS %d %d %u %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %zu\n",
                atmosphere.enabled ? 1 : 0,
                slot->preset.atmospheric_initial_state_enabled ? 1 : 0,
                (unsigned int)atmosphere.seed,
                atmosphere.base_density,
                atmosphere.density_scale,
                atmosphere.density_threshold,
                atmosphere.base_wind_x,
                atmosphere.base_wind_y,
                atmosphere.base_wind_z,
                atmosphere.turbulence_strength,
                atmosphere.noise_scale,
                atmosphere.detail_scale,
                atmosphere.band_min_y,
                atmosphere.band_max_y,
                atmosphere.band_edge_falloff,
                atmosphere.region_count);
        for (size_t r = 0; r < atmosphere.region_count; ++r) {
            const AtmosphericDensityRegion *region = &atmosphere.regions[r];
            fprintf(f,
                    "AREG %d %d %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",
                    region->enabled ? 1 : 0,
                    (int)region->shape,
                    region->center_x,
                    region->center_y,
                    region->center_z,
                    region->size_x,
                    region->size_y,
                    region->size_z,
                    region->density,
                    region->falloff);
        }
        fprintf(f, "%zu\n", slot->preset.emitter_count);
        for (size_t e = 0; e < slot->preset.emitter_count; ++e) {
            FluidEmitter emitter = slot->preset.emitters[e];
            sanitize_emitter(&emitter, sanitize_dimension_mode(slot->preset.dimension_mode));
            fprintf(f, "%d %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %d %d %d %d %d %.9g\n",
                    emitter.type,
                    emitter.position_x,
                    emitter.position_y,
                    emitter.position_z,
                    emitter.radius,
                    emitter.strength,
                    emitter.dir_x,
                    emitter.dir_y,
                    emitter.dir_z,
                    emitter.attached_object,
                    emitter.attached_import,
                    (int)emitter.source_mode_3d,
                    (int)emitter.surface_3d,
                    (int)emitter.obstacle_mode_3d,
                    emitter.thermal_buoyancy_3d);
        }
        fprintf(f, "FLOW %d\n", BOUNDARY_EDGE_COUNT);
        for (int edge = 0; edge < BOUNDARY_EDGE_COUNT; ++edge) {
            const BoundaryFlow *flow = &slot->preset.boundary_flows[edge];
            fprintf(f, "%d %d %.9g\n",
                    edge,
                    flow->mode,
                    flow->strength);
        }
        fprintf(f, "OBJ %zu\n", slot->preset.object_count);
        for (size_t o = 0; o < slot->preset.object_count; ++o) {
            const PresetObject *obj = &slot->preset.objects[o];
            fprintf(f, "%d %.9g %.9g %.9g %.9g %.9g %.9g %.9g %d %d\n",
                    obj->type,
                    obj->position_x,
                    obj->position_y,
                    obj->position_z,
                    obj->size_x,
                    obj->size_y,
                    obj->size_z,
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
            fprintf(f, "%.9g %.9g %.9g %.9g %.9g %d %.9g %.9g %d %d %d %d\n",
                    imp.position_x,
                    imp.position_y,
                    imp.position_z,
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
                fprintf(f, "%.9g %.9g\n", imp.collider_verts[vi].x, imp.collider_verts[vi].y);
            }
            for (int pi = 0; pi < imp.collider_part_count; ++pi) {
                fprintf(f, "%d %d\n", imp.collider_part_offsets[pi], imp.collider_part_counts[pi]);
                int offset = imp.collider_part_offsets[pi];
                for (int vi = 0; vi < imp.collider_part_counts[pi]; ++vi) {
                    int idx = offset + vi;
                    if (idx < 0 || idx >= 128) break;
                    fprintf(f, "%.9g %.9g\n",
                            imp.collider_parts_verts[idx].x,
                            imp.collider_parts_verts[idx].y);
                }
            }
        }
    }

    return physics_sim_persistence_finish(&save, f);
}
