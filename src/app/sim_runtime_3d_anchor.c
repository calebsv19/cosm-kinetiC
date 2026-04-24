#include "app/sim_runtime_3d_anchor.h"

#include "app/scene_state.h"
#include "app/sim_runtime_3d_space.h"

CoreObjectVec3 sim_runtime_3d_anchor_retained_object_origin(const CoreSceneObjectContract *object) {
    if (!object) return (CoreObjectVec3){0};
    if (object->has_plane_primitive) return object->plane_primitive.frame.origin;
    if (object->has_rect_prism_primitive) return object->rect_prism_primitive.frame.origin;
    return object->object.transform.position;
}

static CoreObjectVec3 resolve_world_point(double position_x,
                                          double position_y,
                                          double position_z,
                                          const CoreObjectVec3 *world_min,
                                          const CoreObjectVec3 *world_max) {
    CoreObjectVec3 world = {0};
    world.x = sim_runtime_3d_space_resolve_world_axis(position_x, world_min->x, world_max->x);
    world.y = sim_runtime_3d_space_resolve_world_axis(position_y, world_min->y, world_max->y);
    if (position_z < world_min->z) {
        world.z = world_min->z;
    } else if (position_z > world_max->z) {
        world.z = world_max->z;
    } else {
        world.z = position_z;
    }
    return world;
}

bool sim_runtime_3d_anchor_resolve_emitter_world_anchor(const SceneState *scene,
                                                        int attached_object,
                                                        int attached_import,
                                                        double position_x,
                                                        double position_y,
                                                        double position_z,
                                                        const CoreObjectVec3 *world_min,
                                                        const CoreObjectVec3 *world_max,
                                                        CoreObjectVec3 *out_world) {
    if (!scene || !world_min || !world_max || !out_world) return false;

    if (attached_object >= 0) {
        int index = attached_object;
        if (index < scene->runtime_visual.retained_scene.retained_object_count) {
            *out_world = sim_runtime_3d_anchor_retained_object_origin(
                &scene->runtime_visual.retained_scene.objects[index]);
            return true;
        }
        if (scene->preset && index < (int)scene->preset->object_count) {
            const PresetObject *object = &scene->preset->objects[index];
            *out_world = resolve_world_point(object->position_x,
                                             object->position_y,
                                             object->position_z,
                                             world_min,
                                             world_max);
            return true;
        }
    }

    if (attached_import >= 0) {
        int index = attached_import;
        if (index < (int)scene->import_shape_count) {
            const ImportedShape *imp = &scene->import_shapes[index];
            *out_world = resolve_world_point(imp->position_x,
                                             imp->position_y,
                                             imp->position_z,
                                             world_min,
                                             world_max);
            return true;
        }
        if (scene->preset && index < (int)scene->preset->import_shape_count) {
            const ImportedShape *imp = &scene->preset->import_shapes[index];
            *out_world = resolve_world_point(imp->position_x,
                                             imp->position_y,
                                             imp->position_z,
                                             world_min,
                                             world_max);
            return true;
        }
    }

    *out_world = resolve_world_point(position_x,
                                     position_y,
                                     position_z,
                                     world_min,
                                     world_max);
    return true;
}

bool sim_runtime_3d_anchor_resolve_preset_emitter_world_anchor(const SceneState *scene,
                                                               const FluidEmitter *emitter,
                                                               const CoreObjectVec3 *world_min,
                                                               const CoreObjectVec3 *world_max,
                                                               CoreObjectVec3 *out_world) {
    if (!emitter) return false;
    return sim_runtime_3d_anchor_resolve_emitter_world_anchor(scene,
                                                              emitter->attached_object,
                                                              emitter->attached_import,
                                                              emitter->position_x,
                                                              emitter->position_y,
                                                              emitter->position_z,
                                                              world_min,
                                                              world_max,
                                                              out_world);
}

bool sim_runtime_3d_anchor_resolve_resolved_emitter_world_anchor(
    const SceneState *scene,
    const SimRuntime3DDomainDesc *desc,
    const SimRuntimeEmitterResolved *emitter,
    CoreObjectVec3 *out_world) {
    CoreObjectVec3 world_min = {0};
    CoreObjectVec3 world_max = {0};
    if (!desc || !emitter) return false;
    world_min.x = desc->world_min_x;
    world_min.y = desc->world_min_y;
    world_min.z = desc->world_min_z;
    world_max.x = desc->world_max_x;
    world_max.y = desc->world_max_y;
    world_max.z = desc->world_max_z;
    return sim_runtime_3d_anchor_resolve_emitter_world_anchor(scene,
                                                              emitter->attached_object,
                                                              emitter->attached_import,
                                                              emitter->position_x,
                                                              emitter->position_y,
                                                              emitter->position_z,
                                                              &world_min,
                                                              &world_max,
                                                              out_world);
}
