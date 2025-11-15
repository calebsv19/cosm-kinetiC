#include "physics/objects/object_manager.h"

#include <stdlib.h>
#include <string.h>

static void object_manager_reset(ObjectManager *mgr) {
    if (!mgr) return;
    mgr->objects = NULL;
    mgr->count = 0;
    mgr->capacity = 0;
    mgr->next_id = 1;
    mgr->world = NULL;
}

void object_manager_init(ObjectManager *mgr, int capacity) {
    if (!mgr) return;
    object_manager_reset(mgr);
    if (capacity < 1) capacity = 8;
    mgr->objects = (SceneObject *)calloc((size_t)capacity, sizeof(SceneObject));
    if (!mgr->objects) {
        return;
    }
    mgr->capacity = capacity;
    mgr->world = rigid2d_create(capacity);
    if (!mgr->world) {
        free(mgr->objects);
        object_manager_reset(mgr);
    }
}

void object_manager_shutdown(ObjectManager *mgr) {
    if (!mgr) return;
    free(mgr->objects);
    mgr->objects = NULL;
    mgr->count = 0;
    mgr->capacity = 0;
    mgr->next_id = 1;
    rigid2d_destroy(mgr->world);
    mgr->world = NULL;
}

static bool ensure_world_capacity(ObjectManager *mgr, int desired) {
    if (!mgr) return false;
    if (desired < 1) desired = 1;
    if (!mgr->world) {
        mgr->world = rigid2d_create(desired);
        return mgr->world != NULL;
    }
    if (desired <= mgr->world->capacity) return true;
    int new_capacity = mgr->world->capacity > 0 ? mgr->world->capacity : 8;
    while (new_capacity < desired) {
        new_capacity *= 2;
    }
    RigidBody2D *new_data = (RigidBody2D *)realloc(
        mgr->world->bodies, (size_t)new_capacity * sizeof(RigidBody2D));
    if (!new_data) return false;
    mgr->world->bodies = new_data;
    mgr->world->capacity = new_capacity;
    return true;
}

static bool object_manager_reserve(ObjectManager *mgr, int desired) {
    if (!mgr) return false;
    if (desired <= mgr->capacity) return true;
    int new_capacity = mgr->capacity > 0 ? mgr->capacity : 8;
    while (new_capacity < desired) {
        new_capacity *= 2;
    }
    SceneObject *new_objects =
        (SceneObject *)realloc(mgr->objects, (size_t)new_capacity * sizeof(SceneObject));
    if (!new_objects) return false;
    mgr->objects = new_objects;
    mgr->capacity = new_capacity;

    return ensure_world_capacity(mgr, new_capacity);
}

static SceneObject *object_manager_emplace(ObjectManager *mgr) {
    if (!object_manager_reserve(mgr, mgr->count + 1)) {
        return NULL;
    }
    SceneObject *obj = &mgr->objects[mgr->count++];
    memset(obj, 0, sizeof(*obj));
    obj->id = mgr->next_id++;
    return obj;
}

static void setup_body_common(RigidBody2D *body, bool is_static) {
    body->mass = is_static ? 0.0f : 1.0f;
    body->inv_mass = (body->mass > 0.0f) ? 1.0f / body->mass : 0.0f;
    body->inertia = (body->mass > 0.0f) ? body->mass * 0.5f : 0.0f;
    body->inv_inertia = (body->inertia > 0.0f) ? 1.0f / body->inertia : 0.0f;
    body->restitution = 0.1f;
    body->friction = 0.2f;
    body->is_static = is_static ? 1 : 0;
    body->velocity = vec2(0.0f, 0.0f);
    body->angular_velocity = 0.0f;
}

SceneObject *object_manager_add_circle(ObjectManager *mgr,
                                       Vec2 position,
                                       float radius,
                                       bool is_static) {
    if (!mgr || radius <= 0.0f) return NULL;
    SceneObject *obj = object_manager_emplace(mgr);
    if (!obj) return NULL;
    obj->type = SCENE_OBJECT_CIRCLE;
    obj->body.shape = RIGID2D_SHAPE_CIRCLE;
    obj->body.position = position;
    obj->body.radius = radius;
    setup_body_common(&obj->body, is_static);
    return obj;
}

SceneObject *object_manager_add_box(ObjectManager *mgr,
                                    Vec2 position,
                                    Vec2 half_extents,
                                    bool is_static) {
    if (!mgr || half_extents.x <= 0.0f || half_extents.y <= 0.0f) return NULL;
    SceneObject *obj = object_manager_emplace(mgr);
    if (!obj) return NULL;
    obj->type = SCENE_OBJECT_BOX;
    obj->body.shape = RIGID2D_SHAPE_AABB;
    obj->body.position = position;
    obj->body.half_extents = half_extents;
    setup_body_common(&obj->body, is_static);
    return obj;
}

SceneObject *object_manager_get(ObjectManager *mgr, int id) {
    if (!mgr || id <= 0) return NULL;
    for (int i = 0; i < mgr->count; ++i) {
        if (mgr->objects[i].id == id) {
            return &mgr->objects[i];
        }
    }
    return NULL;
}

bool object_manager_remove(ObjectManager *mgr, int id) {
    if (!mgr || id <= 0) return false;
    for (int i = 0; i < mgr->count; ++i) {
        if (mgr->objects[i].id == id) {
            if (i + 1 < mgr->count) {
                memmove(&mgr->objects[i],
                        &mgr->objects[i + 1],
                        (size_t)(mgr->count - i - 1) * sizeof(SceneObject));
            }
            mgr->count--;
            return true;
        }
    }
    return false;
}

int object_manager_count(const ObjectManager *mgr) {
    return mgr ? mgr->count : 0;
}

SceneObject *object_manager_objects(ObjectManager *mgr) {
    return mgr ? mgr->objects : NULL;
}

void object_manager_step(ObjectManager *mgr,
                         double dt,
                         const AppConfig *cfg) {
    if (!mgr || !mgr->world || mgr->count == 0) return;
    if (!ensure_world_capacity(mgr, mgr->count)) return;

    for (int i = 0; i < mgr->count; ++i) {
        mgr->world->bodies[i] = mgr->objects[i].body;
    }
    mgr->world->count = mgr->count;

    rigid2d_step(mgr->world, dt, cfg);

    for (int i = 0; i < mgr->count; ++i) {
        mgr->objects[i].body = mgr->world->bodies[i];
    }
}
