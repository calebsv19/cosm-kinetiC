#ifndef OBJECT_MANAGER_H
#define OBJECT_MANAGER_H

#include <stdbool.h>

#include "app/app_config.h"
#include "physics/math/math2d.h"
#include "physics/rigid/rigid2d.h"

typedef enum SceneObjectType {
    SCENE_OBJECT_CIRCLE = 0,
    SCENE_OBJECT_BOX
} SceneObjectType;

typedef struct SceneObject {
    int               id;
    SceneObjectType   type;
    RigidBody2D       body;
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
SceneObject *object_manager_get(ObjectManager *mgr, int id);
bool         object_manager_remove(ObjectManager *mgr, int id);
void         object_manager_step(ObjectManager *mgr,
                                 double dt,
                                 const AppConfig *cfg,
                                 bool gravity_enabled);
int          object_manager_count(const ObjectManager *mgr);
SceneObject *object_manager_objects(ObjectManager *mgr);

#endif // OBJECT_MANAGER_H
