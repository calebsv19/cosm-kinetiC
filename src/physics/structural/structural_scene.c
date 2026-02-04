#include "physics/structural/structural_scene.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static float distance_sq(float x1, float y1, float x2, float y2) {
    float dx = x1 - x2;
    float dy = y1 - y2;
    return dx * dx + dy * dy;
}

static void structural_scene_add_default_materials(StructuralScene *scene) {
    if (!scene) return;
    scene->material_count = 0;
    StructMaterial steel = {
        .name = "Steel",
        .youngs_modulus = 20000000.0f,
        .area = 8.0f,
        .moment_inertia = 24.0f,
        .density = 7850.0f,
        .sigma_y = 200000.0f
    };
    StructMaterial aluminum = {
        .name = "Aluminum",
        .youngs_modulus = 7000000.0f,
        .area = 6.0f,
        .moment_inertia = 18.0f,
        .density = 2700.0f,
        .sigma_y = 70000.0f
    };
    StructMaterial wood = {
        .name = "Wood",
        .youngs_modulus = 1200000.0f,
        .area = 5.0f,
        .moment_inertia = 12.0f,
        .density = 600.0f,
        .sigma_y = 12000.0f
    };
    scene->materials[scene->material_count++] = steel;
    scene->materials[scene->material_count++] = aluminum;
    scene->materials[scene->material_count++] = wood;
}

static void structural_scene_add_default_load_cases(StructuralScene *scene) {
    if (!scene) return;
    scene->load_case_count = 0;
    StructLoadCase base = {.name = "Default"};
    scene->load_cases[scene->load_case_count++] = base;
    scene->active_load_case = 0;
}

void structural_scene_init(StructuralScene *scene) {
    if (!scene) return;
    memset(scene, 0, sizeof(*scene));
    scene->next_node_id = 1;
    scene->next_edge_id = 1;
    structural_scene_add_default_materials(scene);
    structural_scene_add_default_load_cases(scene);
    scene->ground_offset = 48.0f;
    scene->gravity_enabled = true;
    scene->gravity_strength = 9.8f;
}

void structural_scene_reset(StructuralScene *scene) {
    if (!scene) return;
    structural_scene_init(scene);
}

int structural_scene_add_node(StructuralScene *scene, float x, float y) {
    if (!scene || scene->node_count >= STRUCT_MAX_NODES) return -1;
    StructNode *node = &scene->nodes[scene->node_count++];
    node->id = scene->next_node_id++;
    node->x = x;
    node->y = y;
    node->selected = false;
    node->fixed_x = false;
    node->fixed_y = false;
    node->fixed_theta = false;
    return node->id;
}

int structural_scene_add_edge(StructuralScene *scene, int a_id, int b_id) {
    if (!scene || scene->edge_count >= STRUCT_MAX_EDGES) return -1;
    if (a_id == b_id) return -1;
    StructNode *a = structural_scene_get_node(scene, a_id);
    StructNode *b = structural_scene_get_node(scene, b_id);
    if (!a || !b) return -1;

    StructEdge *edge = &scene->edges[scene->edge_count++];
    edge->id = scene->next_edge_id++;
    edge->a_id = a_id;
    edge->b_id = b_id;
    edge->rest_length = sqrtf(distance_sq(a->x, a->y, b->x, b->y));
    edge->material_index = 0;
    edge->release_a = false;
    edge->release_b = false;
    edge->axial_force = 0.0f;
    edge->axial_stress = 0.0f;
    edge->shear_force_a = 0.0f;
    edge->shear_force_b = 0.0f;
    edge->bending_moment_a = 0.0f;
    edge->bending_moment_b = 0.0f;
    edge->selected = false;
    return edge->id;
}

int structural_scene_add_load(StructuralScene *scene, int node_id, float fx, float fy, float mz, int case_id) {
    if (!scene || scene->load_count >= STRUCT_MAX_LOADS) return -1;
    if (!structural_scene_get_node(scene, node_id)) return -1;
    if (case_id < 0 || case_id >= (int)scene->load_case_count) return -1;
    StructLoad *load = &scene->loads[scene->load_count++];
    load->node_id = node_id;
    load->fx = fx;
    load->fy = fy;
    load->mz = mz;
    load->case_id = case_id;
    return (int)(scene->load_count - 1);
}

int structural_scene_add_load_case(StructuralScene *scene, const char *name) {
    if (!scene || scene->load_case_count >= STRUCT_MAX_LOAD_CASES) return -1;
    StructLoadCase *load_case = &scene->load_cases[scene->load_case_count++];
    if (name && name[0]) {
        snprintf(load_case->name, sizeof(load_case->name), "%s", name);
    } else {
        snprintf(load_case->name, sizeof(load_case->name), "Case %zu", scene->load_case_count);
    }
    return (int)(scene->load_case_count - 1);
}

StructNode *structural_scene_get_node(StructuralScene *scene, int node_id) {
    if (!scene) return NULL;
    for (size_t i = 0; i < scene->node_count; ++i) {
        if (scene->nodes[i].id == node_id) return &scene->nodes[i];
    }
    return NULL;
}

StructEdge *structural_scene_get_edge(StructuralScene *scene, int edge_id) {
    if (!scene) return NULL;
    for (size_t i = 0; i < scene->edge_count; ++i) {
        if (scene->edges[i].id == edge_id) return &scene->edges[i];
    }
    return NULL;
}

int structural_scene_find_node_at(const StructuralScene *scene, float x, float y, float radius) {
    if (!scene || scene->node_count == 0) return -1;
    float r2 = radius * radius;
    for (size_t i = 0; i < scene->node_count; ++i) {
        const StructNode *node = &scene->nodes[i];
        if (distance_sq(node->x, node->y, x, y) <= r2) {
            return node->id;
        }
    }
    return -1;
}

int structural_scene_find_edge_at(const StructuralScene *scene, float x, float y, float radius) {
    if (!scene || scene->edge_count == 0) return -1;
    float r2 = radius * radius;
    for (size_t i = 0; i < scene->edge_count; ++i) {
        const StructEdge *edge = &scene->edges[i];
        const StructNode *a = NULL;
        const StructNode *b = NULL;
        for (size_t n = 0; n < scene->node_count; ++n) {
            if (scene->nodes[n].id == edge->a_id) a = &scene->nodes[n];
            if (scene->nodes[n].id == edge->b_id) b = &scene->nodes[n];
        }
        if (!a || !b) continue;
        float dx = b->x - a->x;
        float dy = b->y - a->y;
        float len2 = dx * dx + dy * dy;
        if (len2 < 1e-6f) continue;
        float t = ((x - a->x) * dx + (y - a->y) * dy) / len2;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        float px = a->x + t * dx;
        float py = a->y + t * dy;
        if (distance_sq(px, py, x, y) <= r2) {
            return edge->id;
        }
    }
    return -1;
}

bool structural_scene_remove_node(StructuralScene *scene, int node_id) {
    if (!scene) return false;
    size_t index = scene->node_count;
    for (size_t i = 0; i < scene->node_count; ++i) {
        if (scene->nodes[i].id == node_id) {
            index = i;
            break;
        }
    }
    if (index >= scene->node_count) return false;

    for (size_t i = 0; i < scene->edge_count;) {
        if (scene->edges[i].a_id == node_id || scene->edges[i].b_id == node_id) {
            structural_scene_remove_edge(scene, scene->edges[i].id);
            continue;
        }
        ++i;
    }

    scene->nodes[index] = scene->nodes[scene->node_count - 1];
    scene->node_count--;
    return true;
}

bool structural_scene_remove_edge(StructuralScene *scene, int edge_id) {
    if (!scene) return false;
    size_t index = scene->edge_count;
    for (size_t i = 0; i < scene->edge_count; ++i) {
        if (scene->edges[i].id == edge_id) {
            index = i;
            break;
        }
    }
    if (index >= scene->edge_count) return false;
    scene->edges[index] = scene->edges[scene->edge_count - 1];
    scene->edge_count--;
    return true;
}

void structural_scene_clear_solution(StructuralScene *scene) {
    if (!scene) return;
    scene->has_solution = false;
    for (size_t i = 0; i < scene->node_count; ++i) {
        scene->disp_x[i] = 0.0f;
        scene->disp_y[i] = 0.0f;
        scene->disp_theta[i] = 0.0f;
    }
    for (size_t i = 0; i < scene->edge_count; ++i) {
        scene->edges[i].axial_force = 0.0f;
        scene->edges[i].axial_stress = 0.0f;
        scene->edges[i].shear_force_a = 0.0f;
        scene->edges[i].shear_force_b = 0.0f;
        scene->edges[i].bending_moment_a = 0.0f;
        scene->edges[i].bending_moment_b = 0.0f;
    }
}

bool structural_scene_save(const StructuralScene *scene, const char *path) {
    if (!scene || !path) return false;
    FILE *f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "STRUCT_SCENE_V4\n");
    fprintf(f, "NODES %zu\n", scene->node_count);
    for (size_t i = 0; i < scene->node_count; ++i) {
        const StructNode *node = &scene->nodes[i];
        fprintf(f, "%d %.6f %.6f %d %d %d\n",
                node->id, node->x, node->y,
                node->fixed_x ? 1 : 0,
                node->fixed_y ? 1 : 0,
                node->fixed_theta ? 1 : 0);
    }

    fprintf(f, "EDGES %zu\n", scene->edge_count);
    for (size_t i = 0; i < scene->edge_count; ++i) {
        const StructEdge *edge = &scene->edges[i];
        fprintf(f, "%d %d %d %.6f %d %d %d\n",
                edge->id, edge->a_id, edge->b_id,
                edge->rest_length, edge->material_index,
                edge->release_a ? 1 : 0,
                edge->release_b ? 1 : 0);
    }

    fprintf(f, "MATERIALS %zu\n", scene->material_count);
    for (size_t i = 0; i < scene->material_count; ++i) {
        const StructMaterial *mat = &scene->materials[i];
        fprintf(f, "%zu %s %.6f %.6f %.6f %.6f %.6f\n",
                i, mat->name, mat->youngs_modulus, mat->area,
                mat->moment_inertia, mat->density, mat->sigma_y);
    }

    fprintf(f, "LOAD_CASES %zu\n", scene->load_case_count);
    for (size_t i = 0; i < scene->load_case_count; ++i) {
        const StructLoadCase *lc = &scene->load_cases[i];
        fprintf(f, "%zu %s\n", i, lc->name);
    }
    fprintf(f, "ACTIVE_CASE %d\n", scene->active_load_case);

    fprintf(f, "GROUND_OFFSET %.6f\n", scene->ground_offset);
    fprintf(f, "GRAVITY %d %.6f\n",
            scene->gravity_enabled ? 1 : 0,
            scene->gravity_strength);

    fprintf(f, "LOADS %zu\n", scene->load_count);
    for (size_t i = 0; i < scene->load_count; ++i) {
        const StructLoad *load = &scene->loads[i];
        fprintf(f, "%d %.6f %.6f %.6f %d\n",
                load->node_id, load->fx, load->fy, load->mz, load->case_id);
    }

    fclose(f);
    return true;
}

bool structural_scene_load(StructuralScene *scene, const char *path) {
    if (!scene || !path) return false;
    FILE *f = fopen(path, "r");
    if (!f) return false;

    char header[32] = {0};
    if (fscanf(f, "%31s", header) != 1) {
        fclose(f);
        return false;
    }
    bool v2 = strcmp(header, "STRUCT_SCENE_V2") == 0;
    bool v3 = strcmp(header, "STRUCT_SCENE_V3") == 0;
    bool v4 = strcmp(header, "STRUCT_SCENE_V4") == 0;
    if (!v2 && !v3 && !v4 && strcmp(header, "STRUCT_SCENE_V1") != 0) {
        fclose(f);
        return false;
    }
    if (v4) {
        v3 = true;
    }
    if (v3) v2 = true;

    structural_scene_reset(scene);

    size_t count = 0;
    if (fscanf(f, "%31s %zu", header, &count) != 2 || strcmp(header, "NODES") != 0) {
        fclose(f);
        return false;
    }
    scene->node_count = 0;
    int max_node_id = 0;
    for (size_t i = 0; i < count && i < STRUCT_MAX_NODES; ++i) {
        StructNode node = {0};
        int fixed_x = 0;
        int fixed_y = 0;
        int fixed_theta = 0;
        if (v2) {
            if (fscanf(f, "%d %f %f %d %d %d",
                       &node.id, &node.x, &node.y,
                       &fixed_x, &fixed_y, &fixed_theta) != 6) {
                fclose(f);
                return false;
            }
        } else {
            if (fscanf(f, "%d %f %f %d %d",
                       &node.id, &node.x, &node.y, &fixed_x, &fixed_y) != 5) {
                fclose(f);
                return false;
            }
        }
        node.fixed_x = fixed_x != 0;
        node.fixed_y = fixed_y != 0;
        node.fixed_theta = fixed_theta != 0;
        scene->nodes[scene->node_count++] = node;
        if (node.id > max_node_id) max_node_id = node.id;
    }
    scene->next_node_id = max_node_id + 1;

    if (fscanf(f, "%31s %zu", header, &count) != 2 || strcmp(header, "EDGES") != 0) {
        fclose(f);
        return false;
    }
    scene->edge_count = 0;
    int max_edge_id = 0;
    for (size_t i = 0; i < count && i < STRUCT_MAX_EDGES; ++i) {
        StructEdge edge = {0};
        if (v4) {
            int release_a = 0;
            int release_b = 0;
            if (fscanf(f, "%d %d %d %f %d %d %d",
                       &edge.id, &edge.a_id, &edge.b_id,
                       &edge.rest_length, &edge.material_index,
                       &release_a, &release_b) != 7) {
                fclose(f);
                return false;
            }
            edge.release_a = release_a != 0;
            edge.release_b = release_b != 0;
        } else {
            if (fscanf(f, "%d %d %d %f %d",
                       &edge.id, &edge.a_id, &edge.b_id,
                       &edge.rest_length, &edge.material_index) != 5) {
                fclose(f);
                return false;
            }
            edge.release_a = false;
            edge.release_b = false;
        }
        scene->edges[scene->edge_count++] = edge;
        if (edge.id > max_edge_id) max_edge_id = edge.id;
    }
    scene->next_edge_id = max_edge_id + 1;

    if (fscanf(f, "%31s %zu", header, &count) == 2 && strcmp(header, "MATERIALS") == 0) {
        scene->material_count = 0;
        for (size_t i = 0; i < count && i < STRUCT_MAX_MATERIALS; ++i) {
            StructMaterial mat = {0};
            size_t index = 0;
            if (v4) {
                if (fscanf(f, "%zu %31s %f %f %f %f %f",
                           &index, mat.name, &mat.youngs_modulus, &mat.area,
                           &mat.moment_inertia, &mat.density, &mat.sigma_y) != 7) {
                    fclose(f);
                    return false;
                }
            } else if (v2) {
                if (fscanf(f, "%zu %31s %f %f %f %f",
                           &index, mat.name, &mat.youngs_modulus, &mat.area,
                           &mat.moment_inertia, &mat.density) != 6) {
                    fclose(f);
                    return false;
                }
                mat.sigma_y = 0.0f;
            } else {
                if (fscanf(f, "%zu %31s %f %f %f",
                           &index, mat.name, &mat.youngs_modulus, &mat.area, &mat.density) != 5) {
                    fclose(f);
                    return false;
                }
                mat.moment_inertia = 1.0f;
                mat.sigma_y = 0.0f;
            }
            scene->materials[scene->material_count++] = mat;
        }
    } else {
        fclose(f);
        return false;
    }

    if (fscanf(f, "%31s %zu", header, &count) == 2 && strcmp(header, "LOAD_CASES") == 0) {
        scene->load_case_count = 0;
        for (size_t i = 0; i < count && i < STRUCT_MAX_LOAD_CASES; ++i) {
            StructLoadCase lc = {0};
            size_t index = 0;
            if (fscanf(f, "%zu %31s", &index, lc.name) != 2) {
                fclose(f);
                return false;
            }
            scene->load_cases[scene->load_case_count++] = lc;
        }
    } else {
        fclose(f);
        return false;
    }

    int active_case = 0;
    if (fscanf(f, "%31s %d", header, &active_case) == 2 &&
        strcmp(header, "ACTIVE_CASE") == 0) {
        scene->active_load_case = active_case;
    } else {
        fclose(f);
        return false;
    }

    scene->ground_offset = 48.0f;
    scene->gravity_enabled = true;
    scene->gravity_strength = 9.8f;

    if (v3) {
        float ground_offset = 0.0f;
        if (fscanf(f, "%31s %f", header, &ground_offset) != 2 ||
            strcmp(header, "GROUND_OFFSET") != 0) {
            fclose(f);
            return false;
        }
        scene->ground_offset = ground_offset;

        int gravity_enabled = 0;
        float gravity_strength = 0.0f;
        if (fscanf(f, "%31s %d %f", header, &gravity_enabled, &gravity_strength) != 3 ||
            strcmp(header, "GRAVITY") != 0) {
            fclose(f);
            return false;
        }
        scene->gravity_enabled = gravity_enabled != 0;
        scene->gravity_strength = gravity_strength;
    }

    if (fscanf(f, "%31s %zu", header, &count) != 2 || strcmp(header, "LOADS") != 0) {
        fclose(f);
        return false;
    }
    scene->load_count = 0;
    for (size_t i = 0; i < count && i < STRUCT_MAX_LOADS; ++i) {
        StructLoad load = {0};
        if (v2) {
            if (fscanf(f, "%d %f %f %f %d",
                       &load.node_id, &load.fx, &load.fy, &load.mz, &load.case_id) != 5) {
                fclose(f);
                return false;
            }
        } else {
            if (fscanf(f, "%d %f %f %d",
                       &load.node_id, &load.fx, &load.fy, &load.case_id) != 4) {
                fclose(f);
                return false;
            }
            load.mz = 0.0f;
        }
        scene->loads[scene->load_count++] = load;
    }

    fclose(f);
    structural_scene_clear_solution(scene);
    return true;
}
