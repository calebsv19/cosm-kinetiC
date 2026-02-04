#ifndef STRUCTURAL_SCENE_H
#define STRUCTURAL_SCENE_H

#include <stdbool.h>
#include <stddef.h>

#define STRUCT_MAX_NODES 512
#define STRUCT_MAX_EDGES 1024
#define STRUCT_MAX_LOADS 1024
#define STRUCT_MAX_MATERIALS 16
#define STRUCT_MAX_LOAD_CASES 8

#define STRUCT_NAME_MAX 32

typedef struct StructNode {
    int   id;
    float x;
    float y;
    bool  selected;
    bool  fixed_x;
    bool  fixed_y;
    bool  fixed_theta;
} StructNode;

typedef struct StructEdge {
    int   id;
    int   a_id;
    int   b_id;
    float rest_length;
    int   material_index;
    bool  release_a;
    bool  release_b;
    float axial_force;
    float axial_stress;
    float shear_force_a;
    float shear_force_b;
    float bending_moment_a;
    float bending_moment_b;
    bool  selected;
} StructEdge;

typedef struct StructLoad {
    int   node_id;
    float fx;
    float fy;
    float mz;
    int   case_id;
} StructLoad;

typedef struct StructMaterial {
    char  name[STRUCT_NAME_MAX];
    float youngs_modulus;
    float area;
    float moment_inertia;
    float section_modulus;
    float density;
    float sigma_y;
} StructMaterial;

typedef struct StructLoadCase {
    char name[STRUCT_NAME_MAX];
} StructLoadCase;

typedef struct StructuralScene {
    StructNode nodes[STRUCT_MAX_NODES];
    size_t     node_count;
    StructEdge edges[STRUCT_MAX_EDGES];
    size_t     edge_count;
    StructLoad loads[STRUCT_MAX_LOADS];
    size_t     load_count;

    StructMaterial materials[STRUCT_MAX_MATERIALS];
    size_t         material_count;
    StructLoadCase load_cases[STRUCT_MAX_LOAD_CASES];
    size_t         load_case_count;
    int            active_load_case;
    float          ground_offset;
    bool           gravity_enabled;
    float          gravity_strength;

    float disp_x[STRUCT_MAX_NODES];
    float disp_y[STRUCT_MAX_NODES];
    float disp_theta[STRUCT_MAX_NODES];
    bool  has_solution;

    int next_node_id;
    int next_edge_id;
} StructuralScene;

void structural_scene_init(StructuralScene *scene);
void structural_scene_reset(StructuralScene *scene);

int  structural_scene_add_node(StructuralScene *scene, float x, float y);
int  structural_scene_add_edge(StructuralScene *scene, int a_id, int b_id);
int  structural_scene_add_load(StructuralScene *scene, int node_id, float fx, float fy, float mz, int case_id);
int  structural_scene_add_load_case(StructuralScene *scene, const char *name);

StructNode *structural_scene_get_node(StructuralScene *scene, int node_id);
StructEdge *structural_scene_get_edge(StructuralScene *scene, int edge_id);

int  structural_scene_find_node_at(const StructuralScene *scene, float x, float y, float radius);
int  structural_scene_find_edge_at(const StructuralScene *scene, float x, float y, float radius);

bool structural_scene_remove_node(StructuralScene *scene, int node_id);
bool structural_scene_remove_edge(StructuralScene *scene, int edge_id);

void structural_scene_clear_solution(StructuralScene *scene);
bool structural_scene_save(const StructuralScene *scene, const char *path);
bool structural_scene_load(StructuralScene *scene, const char *path);

#endif // STRUCTURAL_SCENE_H
