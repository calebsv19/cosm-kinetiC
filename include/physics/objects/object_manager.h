#ifndef OBJECT_MANAGER_H
#define OBJECT_MANAGER_H

#include <stdbool.h>

#include "app/app_config.h"
#include "physics/math/math2d.h"
#include "physics/rigid/rigid2d.h"

typedef enum SceneObjectType {
    SCENE_OBJECT_CIRCLE = 0,
    SCENE_OBJECT_BOX,
    SCENE_OBJECT_POLY
} SceneObjectType;

typedef struct SceneObject {
    int               id;
    SceneObjectType   type;
    RigidBody2D       body;
    int               source_import; // index of source import (-1 if none)
} SceneObject;

typedef struct ObjectManager {
    SceneObject  *objects;
    int           count;
    int           capacity;
    int           next_id;
    Rigid2DWorld *world;
} ObjectManager;

void object_manager_init(ObjectManager *mgr, int capacity);
void object_manager_shutdown(ObjectManager *mgr);

SceneObject *object_manager_add_circle(ObjectManager *mgr,
                                       Vec2 position,
                                       float radius,
                                       bool is_static);
SceneObject *object_manager_add_box(ObjectManager *mgr,
                                    Vec2 position,
                                    Vec2 half_extents,
                                    bool is_static);
SceneObject *object_manager_add_poly(ObjectManager *mgr,
                                     Vec2 position,
                                     const Vec2 *verts,
                                     int vert_count,
                                     bool is_static);
SceneObject *object_manager_get(ObjectManager *mgr, int id);
bool         object_manager_remove(ObjectManager *mgr, int id);
void         object_manager_step(ObjectManager *mgr,
                                 double dt,
                                 const AppConfig *cfg,
                                 bool gravity_enabled);
int          object_manager_count(const ObjectManager *mgr);
SceneObject *object_manager_objects(ObjectManager *mgr);

// Remove any objects whose source_import matches the given index.
void object_manager_remove_by_source_import(ObjectManager *mgr, int source_import);

#endif // OBJECT_MANAGER_H
