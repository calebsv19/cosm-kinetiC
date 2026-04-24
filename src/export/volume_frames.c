#include "export/volume_frames.h"

#include "app/scene_state.h"
#include "export/export_paths.h"
#include "export/volume_frames_vf3d.h"
#include "core_data.h"
#include "core_io.h"
#include "core_pack.h"
#include "core_scene.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "cJSON.h"

typedef struct VolumeFrameHeaderV2 {
    uint32_t magic;
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    double   time_seconds;
    uint64_t frame_index;
    double   dt_seconds;
    float    origin_x;
    float    origin_y;
    float    cell_size;
    uint32_t obstacle_mask_crc32;
    uint32_t reserved[3];
} VolumeFrameHeaderV2;

typedef struct VolumeFrameHeaderV1 {
    uint32_t magic;
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    double   time_seconds;
    uint64_t frame_index;
} VolumeFrameHeaderV1;

static const uint32_t VOLUME_MAGIC = ('V' << 24) | ('F' << 16) | ('R' << 8) | ('M');
static const uint32_t VOLUME_VERSION_V2 = 2;
static const uint32_t VOLUME_VERSION_V1 = 1;

static bool build_replace_extension_path(const char *src_path,
                                         const char *expected_ext,
                                         const char *new_ext,
                                         char *out_path,
                                         size_t out_size) {
    size_t len = 0;
    const char *ext = NULL;
    size_t stem_len = 0;
    size_t new_ext_len = 0;
    if (!src_path || !expected_ext || !new_ext || !out_path || out_size == 0) return false;
    len = strlen(src_path);
    if (len + 1 > out_size) return false;
    memcpy(out_path, src_path, len + 1);

    ext = strrchr(out_path, '.');
    if (!ext || strcmp(ext, expected_ext) != 0) return false;
    stem_len = (size_t)(ext - out_path);
    new_ext_len = strlen(new_ext);
    if (stem_len + new_ext_len + 1 > out_size) return false;
    memcpy(out_path + stem_len, new_ext, new_ext_len + 1);
    return true;
}

#ifndef VOLUME_FRAMES_DATASET_TOOL_ONLY
static bool build_pack_path(const char *frame_path, char *out_pack_path, size_t out_size) {
    if (build_replace_extension_path(frame_path, ".vf2d", ".pack", out_pack_path, out_size)) return true;
    if (build_replace_extension_path(frame_path, ".vf3d", ".pack", out_pack_path, out_size)) return true;
    return false;
}
#endif

static bool build_dataset_json_path(const char *vf2d_path, char *out_json_path, size_t out_size) {
    return build_replace_extension_path(vf2d_path,
                                        ".vf2d",
                                        ".dataset.json",
                                        out_json_path,
                                        out_size);
}

static const CoreTableColumnTyped *find_typed_column(const CoreDataItem *item, const char *name) {
    if (!item || !name || item->kind != CORE_DATA_TABLE_TYPED) return NULL;
    for (uint32_t i = 0; i < item->as.table_typed.column_count; ++i) {
        const CoreTableColumnTyped *col = &item->as.table_typed.columns[i];
        if (col->name && strcmp(col->name, name) == 0) return col;
    }
    return NULL;
}

static bool append_dataset_items_json(cJSON *items, const CoreDataset *dataset) {
    if (!items || !dataset) return false;
    for (size_t i = 0; i < dataset->item_count; ++i) {
        const CoreDataItem *item = &dataset->items[i];
        if (item->kind == CORE_DATA_SCALAR_F64) {
            cJSON *entry = cJSON_CreateObject();
            if (!entry) return false;
            cJSON_AddStringToObject(entry, "name", item->name ? item->name : "unnamed");
            cJSON_AddStringToObject(entry, "kind", "scalar_f64");
            cJSON_AddNumberToObject(entry, "value", item->as.scalar_f64);
            cJSON_AddItemToArray(items, entry);
            continue;
        }
        if (item->kind == CORE_DATA_TABLE_TYPED && item->as.table_typed.row_count > 0) {
            cJSON *entry = cJSON_CreateObject();
            cJSON *row = cJSON_CreateObject();
            if (!entry || !row) {
                cJSON_Delete(entry);
                cJSON_Delete(row);
                return false;
            }
            cJSON_AddStringToObject(entry, "name", item->name ? item->name : "table_typed");
            cJSON_AddStringToObject(entry, "kind", "table_typed");
            cJSON_AddNumberToObject(entry, "rows", item->as.table_typed.row_count);
            cJSON_AddNumberToObject(entry, "columns", item->as.table_typed.column_count);

            for (uint32_t c = 0; c < item->as.table_typed.column_count; ++c) {
                const CoreTableColumnTyped *col = &item->as.table_typed.columns[c];
                const char *name = col->name ? col->name : "col";
                switch (col->type) {
                    case CORE_TABLE_COL_F32:
                        cJSON_AddNumberToObject(row, name, (double)col->as.f32_values[0]);
                        break;
                    case CORE_TABLE_COL_F64:
                        cJSON_AddNumberToObject(row, name, col->as.f64_values[0]);
                        break;
                    case CORE_TABLE_COL_I64:
                        cJSON_AddNumberToObject(row, name, (double)col->as.i64_values[0]);
                        break;
                    case CORE_TABLE_COL_U32:
                        cJSON_AddNumberToObject(row, name, (double)col->as.u32_values[0]);
                        break;
                    case CORE_TABLE_COL_BOOL:
                        cJSON_AddBoolToObject(row, name, col->as.bool_values[0] ? 1 : 0);
                        break;
                    default:
                        break;
                }
            }

            cJSON_AddItemToObject(entry, "row0", row);
            cJSON_AddItemToArray(items, entry);
        }
    }
    return true;
}

bool volume_frame_write_core_dataset_json(const char *vf2d_path,
                                          const char *dataset_json_path,
                                          const char *manifest_path) {
    if (!vf2d_path || !vf2d_path[0]) return false;

    char out_path[512];
    const char *json_path = dataset_json_path;
    if (!json_path || !json_path[0]) {
        if (!build_dataset_json_path(vf2d_path, out_path, sizeof(out_path))) return false;
        json_path = out_path;
    }

    VolumeFrameInfo info;
    if (!volume_frame_read_header(vf2d_path, &info)) return false;

    CoreDataset dataset;
    core_dataset_init(&dataset);

    uint32_t sample_count = info.grid_w * info.grid_h;

    CoreResult r = core_dataset_add_metadata_string(&dataset, "profile", "physics_sim_volume_dataset_v1");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "schema_family", "physics_sim_volume_dataset");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "schema_variant", "vf2d");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "dataset_schema", "physics_sim.volume_dataset");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_i64(&dataset, "dataset_contract_version", 1);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_i64(&dataset, "schema_version", 1); // Kept for compatibility.
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "header_table", "volume_frame_header_v1");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "session_table", "volume_frame_session_v1");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "field_table", "volume_frame_fields_v1");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "source_vf2d", vf2d_path);
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_bool(&dataset, "has_manifest", manifest_path && manifest_path[0]);
    if (r.code != CORE_OK) goto fail;
    if (manifest_path && manifest_path[0]) {
        r = core_dataset_add_metadata_string(&dataset, "manifest_path", manifest_path);
        if (r.code != CORE_OK) goto fail;
    }

    r = core_dataset_add_scalar_f64(&dataset, "time_seconds", info.time_seconds);
    if (r.code != CORE_OK) goto fail;

    const char *cols[] = {
        "frame_index", "time_seconds", "dt_seconds", "grid_w", "grid_h",
        "origin_x", "origin_y", "cell_size", "obstacle_mask_crc32"
    };
    CoreTableColumnType types[] = {
        CORE_TABLE_COL_I64, CORE_TABLE_COL_F64, CORE_TABLE_COL_F64, CORE_TABLE_COL_U32, CORE_TABLE_COL_U32,
        CORE_TABLE_COL_F64, CORE_TABLE_COL_F64, CORE_TABLE_COL_F64, CORE_TABLE_COL_U32
    };
    int64_t frame_index_col[] = {(int64_t)info.frame_index};
    double time_col[] = {info.time_seconds};
    double dt_col[] = {info.dt_seconds};
    uint32_t grid_w_col[] = {info.grid_w};
    uint32_t grid_h_col[] = {info.grid_h};
    double origin_x_col[] = {(double)info.origin_x};
    double origin_y_col[] = {(double)info.origin_y};
    double cell_size_col[] = {(double)info.cell_size};
    uint32_t crc_col[] = {info.obstacle_mask_crc32};
    const void *column_data[] = {
        frame_index_col, time_col, dt_col, grid_w_col, grid_h_col,
        origin_x_col, origin_y_col, cell_size_col, crc_col
    };
    r = core_dataset_add_table_typed(&dataset,
                                     "volume_frame_header_v1",
                                     cols,
                                     types,
                                     (uint32_t)(sizeof(cols) / sizeof(cols[0])),
                                     1u,
                                     column_data);
    if (r.code != CORE_OK) goto fail;

    {
        const char *session_cols[] = {
            "frame_index", "has_manifest", "dataset_contract_version",
            "field_count", "sample_count", "obstacle_mask_crc32"
        };
        CoreTableColumnType session_types[] = {
            CORE_TABLE_COL_I64, CORE_TABLE_COL_BOOL, CORE_TABLE_COL_I64,
            CORE_TABLE_COL_U32, CORE_TABLE_COL_U32, CORE_TABLE_COL_U32
        };
        int64_t session_frame_index_col[] = {(int64_t)info.frame_index};
        bool has_manifest_col[] = {manifest_path && manifest_path[0]};
        int64_t contract_col[] = {1};
        uint32_t field_count_col[] = {3u};
        uint32_t sample_count_col[] = {sample_count};
        uint32_t session_crc_col[] = {info.obstacle_mask_crc32};
        const void *session_data[] = {
            session_frame_index_col, has_manifest_col, contract_col,
            field_count_col, sample_count_col, session_crc_col
        };

        r = core_dataset_add_table_typed(&dataset,
                                         "volume_frame_session_v1",
                                         session_cols,
                                         session_types,
                                         (uint32_t)(sizeof(session_cols) / sizeof(session_cols[0])),
                                         1u,
                                         session_data);
        if (r.code != CORE_OK) goto fail;
    }

    {
        const char *field_cols[] = {
            "field_id", "field_kind", "components", "unit_id", "grid_w", "grid_h", "sample_count"
        };
        CoreTableColumnType field_types[] = {
            CORE_TABLE_COL_I64, CORE_TABLE_COL_I64, CORE_TABLE_COL_U32, CORE_TABLE_COL_I64,
            CORE_TABLE_COL_U32, CORE_TABLE_COL_U32, CORE_TABLE_COL_U32
        };
        int64_t field_id_col[] = {1, 2, 3};
        int64_t field_kind_col[] = {
            1, // density
            2, // velocity_x
            3  // velocity_y
        };
        uint32_t components_col[] = {1u, 1u, 1u};
        int64_t unit_id_col[] = {
            1, // normalized_density
            2, // meters_per_second
            2
        };
        uint32_t field_grid_w_col[] = {info.grid_w, info.grid_w, info.grid_w};
        uint32_t field_grid_h_col[] = {info.grid_h, info.grid_h, info.grid_h};
        uint32_t field_sample_count_col[] = {sample_count, sample_count, sample_count};
        const void *field_data[] = {
            field_id_col, field_kind_col, components_col, unit_id_col,
            field_grid_w_col, field_grid_h_col, field_sample_count_col
        };

        r = core_dataset_add_table_typed(&dataset,
                                         "volume_frame_fields_v1",
                                         field_cols,
                                         field_types,
                                         (uint32_t)(sizeof(field_cols) / sizeof(field_cols[0])),
                                         3u,
                                         field_data);
        if (r.code != CORE_OK) goto fail;
    }

    r = core_dataset_add_metadata_string(&dataset, "field_kind_1", "density");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "field_kind_2", "velocity_x");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "field_kind_3", "velocity_y");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "unit_1", "normalized_density");
    if (r.code != CORE_OK) goto fail;
    r = core_dataset_add_metadata_string(&dataset, "unit_2", "meters_per_second");
    if (r.code != CORE_OK) goto fail;

    cJSON *root = cJSON_CreateObject();
    cJSON *metadata = cJSON_CreateObject();
    cJSON *items = cJSON_CreateArray();
    if (!root || !metadata || !items) {
        cJSON_Delete(root);
        cJSON_Delete(metadata);
        cJSON_Delete(items);
        goto fail;
    }

    const CoreMetadataItem *profile = core_dataset_find_metadata(&dataset, "profile");
    const CoreMetadataItem *schema_version = core_dataset_find_metadata(&dataset, "schema_version");
    cJSON_AddStringToObject(root,
                            "profile",
                            (profile && profile->type == CORE_META_STRING && profile->as.string_value)
                                ? profile->as.string_value
                                : "physics_sim_volume_dataset_v1");
    {
        const CoreMetadataItem *schema_family = core_dataset_find_metadata(&dataset, "schema_family");
        cJSON_AddStringToObject(root,
                                "schema_family",
                                (schema_family && schema_family->type == CORE_META_STRING && schema_family->as.string_value)
                                    ? schema_family->as.string_value
                                    : "physics_sim_volume_dataset");
    }
    {
        const CoreMetadataItem *schema_variant = core_dataset_find_metadata(&dataset, "schema_variant");
        cJSON_AddStringToObject(root,
                                "schema_variant",
                                (schema_variant && schema_variant->type == CORE_META_STRING && schema_variant->as.string_value)
                                    ? schema_variant->as.string_value
                                    : "vf2d");
    }
    {
        const CoreMetadataItem *dataset_schema = core_dataset_find_metadata(&dataset, "dataset_schema");
        cJSON_AddStringToObject(root,
                                "dataset_schema",
                                (dataset_schema && dataset_schema->type == CORE_META_STRING && dataset_schema->as.string_value)
                                    ? dataset_schema->as.string_value
                                    : "physics_sim.volume_dataset");
    }
    cJSON_AddNumberToObject(root,
                            "schema_version",
                            (schema_version && schema_version->type == CORE_META_I64)
                                ? (double)schema_version->as.i64_value
                                : 1.0);

    for (size_t i = 0; i < dataset.metadata_count; ++i) {
        const CoreMetadataItem *m = &dataset.metadata[i];
        if (!m->key) continue;
        switch (m->type) {
            case CORE_META_STRING:
                cJSON_AddStringToObject(metadata, m->key, m->as.string_value ? m->as.string_value : "");
                break;
            case CORE_META_F64:
                cJSON_AddNumberToObject(metadata, m->key, m->as.f64_value);
                break;
            case CORE_META_I64:
                cJSON_AddNumberToObject(metadata, m->key, (double)m->as.i64_value);
                break;
            case CORE_META_BOOL:
                cJSON_AddBoolToObject(metadata, m->key, m->as.bool_value ? 1 : 0);
                break;
            default:
                break;
        }
    }
    cJSON_AddItemToObject(root, "metadata", metadata);

    if (!append_dataset_items_json(items, &dataset)) {
        cJSON_Delete(root);
        goto fail;
    }
    cJSON_AddItemToObject(root, "items", items);

    const CoreDataItem *table = core_dataset_find(&dataset, "volume_frame_header_v1");
    if (table && table->kind == CORE_DATA_TABLE_TYPED) {
        cJSON *header_json = cJSON_CreateObject();
        if (!header_json) {
            cJSON_Delete(root);
            goto fail;
        }
        const CoreTableColumnTyped *col_frame_index = find_typed_column(table, "frame_index");
        const CoreTableColumnTyped *col_grid_w = find_typed_column(table, "grid_w");
        const CoreTableColumnTyped *col_grid_h = find_typed_column(table, "grid_h");
        if (col_frame_index && col_frame_index->type == CORE_TABLE_COL_I64) {
            cJSON_AddNumberToObject(header_json, "frame_index", (double)col_frame_index->as.i64_values[0]);
        }
        if (col_grid_w && col_grid_w->type == CORE_TABLE_COL_U32) {
            cJSON_AddNumberToObject(header_json, "grid_w", (double)col_grid_w->as.u32_values[0]);
        }
        if (col_grid_h && col_grid_h->type == CORE_TABLE_COL_U32) {
            cJSON_AddNumberToObject(header_json, "grid_h", (double)col_grid_h->as.u32_values[0]);
        }
        cJSON_AddNumberToObject(header_json, "time_seconds", info.time_seconds);
        cJSON_AddNumberToObject(header_json, "dt_seconds", info.dt_seconds);
        cJSON_AddNumberToObject(header_json, "origin_x", (double)info.origin_x);
        cJSON_AddNumberToObject(header_json, "origin_y", (double)info.origin_y);
        cJSON_AddNumberToObject(header_json, "cell_size", (double)info.cell_size);
        cJSON_AddNumberToObject(header_json, "obstacle_mask_crc32", (double)info.obstacle_mask_crc32);
        cJSON_AddItemToObject(root, "frame_header", header_json);
    }

    char *json_text = cJSON_Print(root);
    cJSON_Delete(root);
    if (!json_text) goto fail;
    r = core_io_write_all(json_path, json_text, strlen(json_text));
    free(json_text);
    core_dataset_free(&dataset);
    return r.code == CORE_OK;

fail:
    core_dataset_free(&dataset);
    return false;
}

#ifndef VOLUME_FRAMES_DATASET_TOOL_ONLY
static uint32_t obstacle_mask_crc32(const SceneState *scene) {
    SceneObstacleFieldView2D obstacles = {0};
    if (!scene) return 0;
    if (!scene_backend_obstacle_view_2d(scene, &obstacles) || !obstacles.solid_mask) return 0;
    size_t count = obstacles.cell_count;
    uint32_t hash = 2166136261u; // FNV-1a
    for (size_t i = 0; i < count; ++i) {
        hash ^= (uint32_t)obstacles.solid_mask[i];
        hash *= 16777619u;
    }
    return hash;
}

static void json_set_number(cJSON *obj, const char *name, double value) {
    if (!obj || !name) return;
    cJSON_DeleteItemFromObject(obj, name);
    cJSON_AddNumberToObject(obj, name, value);
}

static bool manifest_append(const SceneState *scene,
                            const VolumeFrameHeaderV2 *header,
                            const char *frame_path,
                            const char *run_dir) {
    if (!scene || !header || !frame_path) return false;

    const char *dir = run_dir ? run_dir : export_volume_dir();
    if (!dir || !dir[0]) return false;
    char manifest_path[512];
    snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json", dir);

    FILE *f = fopen(manifest_path, "rb");
    cJSON *root = NULL;
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz > 0) {
            char *buf = (char *)malloc((size_t)sz + 1);
            if (buf) {
                size_t n = fread(buf, 1, (size_t)sz, f);
                if (n == (size_t)sz) {
                    buf[sz] = '\0';
                    root = cJSON_Parse(buf);
                }
                free(buf);
            }
        }
        fclose(f);
    }

    if (!root) {
        root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "manifest_version", 1);
        cJSON_AddNumberToObject(root, "grid_w", header->grid_w);
        cJSON_AddNumberToObject(root, "grid_h", header->grid_h);
        cJSON_AddNumberToObject(root, "cell_size", header->cell_size);
        cJSON_AddNumberToObject(root, "origin_x", header->origin_x);
        cJSON_AddNumberToObject(root, "origin_y", header->origin_y);
        cJSON_AddNumberToObject(root, "obstacle_mask_crc32", header->obstacle_mask_crc32);
        cJSON_AddStringToObject(root, "run_name", dir);
        if (scene->preset && scene->preset->name) {
            cJSON_AddStringToObject(root, "preset", scene->preset->name);
        }

        if (scene->import_shape_count > 0) {
            cJSON *imports = cJSON_AddArrayToObject(root, "imports");
            if (imports) {
                for (size_t i = 0; i < scene->import_shape_count; ++i) {
                    const ImportedShape *imp = &scene->import_shapes[i];
                    if (!imp->path[0]) continue;
                    cJSON *obj = cJSON_CreateObject();
                    if (!obj) continue;
                    cJSON_AddStringToObject(obj, "path", imp->path);
                    cJSON_AddNumberToObject(obj, "pos_x_norm", imp->position_x);
                    cJSON_AddNumberToObject(obj, "pos_y_norm", imp->position_y);
                    cJSON_AddNumberToObject(obj, "rotation_deg", imp->rotation_deg);
                    cJSON_AddNumberToObject(obj, "scale", imp->scale);
                    cJSON_AddBoolToObject(obj, "is_static", imp->is_static);
                    cJSON_AddItemToArray(imports, obj);
                }
            }
        }

        cJSON_AddArrayToObject(root, "frames");
    }

    cJSON *space_contract = cJSON_GetObjectItem(root, "space_contract");
    if (!cJSON_IsObject(space_contract)) {
        cJSON_DeleteItemFromObject(root, "space_contract");
        space_contract = cJSON_CreateObject();
        if (space_contract) cJSON_AddItemToObject(root, "space_contract", space_contract);
    }
    if (cJSON_IsObject(space_contract)) {
        int author_w = (scene->config && scene->config->window_w > 0)
            ? scene->config->window_w
            : (int)header->grid_w;
        int author_h = (scene->config && scene->config->window_h > 0)
            ? scene->config->window_h
            : (int)header->grid_h;
        const float import_fit = 0.25f;

        json_set_number(space_contract, "version", 1);
        json_set_number(space_contract, "grid_w", header->grid_w);
        json_set_number(space_contract, "grid_h", header->grid_h);
        json_set_number(space_contract, "origin_x", header->origin_x);
        json_set_number(space_contract, "origin_y", header->origin_y);
        json_set_number(space_contract, "cell_size", header->cell_size);
        json_set_number(space_contract, "author_window_w", author_w);
        json_set_number(space_contract, "author_window_h", author_h);
        json_set_number(space_contract, "import_fit", import_fit);
    }

    cJSON *frames = cJSON_GetObjectItem(root, "frames");
    if (!frames) {
        frames = cJSON_AddArrayToObject(root, "frames");
    }
    if (!frames) {
        cJSON_Delete(root);
        return false;
    }

    cJSON *entry = cJSON_CreateObject();
    if (!entry) { cJSON_Delete(root); return false; }
    cJSON_AddNumberToObject(entry, "frame_index", (double)header->frame_index);
    cJSON_AddNumberToObject(entry, "time_seconds", header->time_seconds);
    cJSON_AddNumberToObject(entry, "dt_seconds", header->dt_seconds);
    cJSON_AddStringToObject(entry, "path", frame_path);
    cJSON_AddItemToArray(frames, entry);

    char *text = cJSON_Print(root);
    cJSON_Delete(root);
    if (!text) return false;

    f = fopen(manifest_path, "wb");
    if (!f) {
        free(text);
        return false;
    }
    size_t len = strlen(text);
    bool ok = fwrite(text, 1, len, f) == len;
    fclose(f);
    free(text);
    return ok;
}

#endif

static bool resolve_manifest_latest_frame(const char *manifest_path, char *out_frame_path, size_t out_size) {
    if (!manifest_path || !out_frame_path || out_size == 0) return false;
    out_frame_path[0] = '\0';

    FILE *f = fopen(manifest_path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(f);
        return false;
    }

    char *buf = (char *)malloc((size_t)sz + 1u);
    if (!buf) {
        fclose(f);
        return false;
    }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (n != (size_t)sz) {
        free(buf);
        return false;
    }
    buf[sz] = '\0';

    bool ok = false;
    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return false;

    cJSON *frames = cJSON_GetObjectItem(root, "frames");
    int count = cJSON_IsArray(frames) ? cJSON_GetArraySize(frames) : 0;
    if (count > 0) {
        cJSON *last = cJSON_GetArrayItem(frames, count - 1);
        cJSON *path = cJSON_IsObject(last) ? cJSON_GetObjectItem(last, "path") : NULL;
        if (cJSON_IsString(path) && path->valuestring && path->valuestring[0]) {
            CoreResult r = core_result_ok();
            if (path->valuestring[0] == '/') {
                snprintf(out_frame_path, out_size, "%s", path->valuestring);
                ok = true;
            } else {
                char manifest_dir[512];
                r = core_scene_dirname(manifest_path, manifest_dir, sizeof(manifest_dir));
                if (r.code == CORE_OK) {
                    r = core_scene_resolve_path(manifest_dir,
                                                path->valuestring,
                                                out_frame_path,
                                                out_size);
                    ok = r.code == CORE_OK;
                }
            }
        }
    }
    cJSON_Delete(root);
    return ok;
}

#ifndef VOLUME_FRAMES_DATASET_TOOL_ONLY
static bool write_scene_bundle(const SceneState *scene, const VolumeFrameHeaderV2 *header, const char *run_dir) {
    if (!scene || !header || !run_dir || !run_dir[0]) return false;
    char bundle_path[512];
    snprintf(bundle_path, sizeof(bundle_path), "%s/scene_bundle.json", run_dir);

    const char *camera_path = getenv("PHYSICS_SCENE_CAMERA_PATH");
    const char *light_path = getenv("PHYSICS_SCENE_LIGHT_PATH");
    const char *mapping_profile = getenv("PHYSICS_SCENE_ASSET_MAPPING_PROFILE");
    if (!mapping_profile || !mapping_profile[0]) mapping_profile = "physics_to_ray_v1";

    FILE *f = fopen(bundle_path, "wb");
    if (!f) return false;
    fprintf(f, "{\n");
    fprintf(f, "  \"bundle_type\": \"physics_scene_bundle_v1\",\n");
    fprintf(f, "  \"bundle_version\": 1,\n");
    fprintf(f, "  \"profile\": \"physics\",\n");
    fprintf(f, "  \"fluid_source\": {\n");
    fprintf(f, "    \"kind\": \"manifest\",\n");
    fprintf(f, "    \"path\": \"manifest.json\"\n");
    fprintf(f, "  },\n");
    fprintf(f, "  \"scene_metadata\": {\n");
    fprintf(f, "    \"asset_mapping_profile\": \"%s\",\n", mapping_profile);
    int author_w = (scene->config && scene->config->window_w > 0)
        ? scene->config->window_w
        : (int)header->grid_w;
    int author_h = (scene->config && scene->config->window_h > 0)
        ? scene->config->window_h
        : (int)header->grid_h;
    fprintf(f, "    \"space_contract\": {\n");
    fprintf(f, "      \"version\": 1,\n");
    fprintf(f, "      \"grid_w\": %u,\n", header->grid_w);
    fprintf(f, "      \"grid_h\": %u,\n", header->grid_h);
    fprintf(f, "      \"origin_x\": %.6f,\n", (double)header->origin_x);
    fprintf(f, "      \"origin_y\": %.6f,\n", (double)header->origin_y);
    fprintf(f, "      \"cell_size\": %.6f,\n", (double)header->cell_size);
    fprintf(f, "      \"author_window_w\": %d,\n", author_w);
    fprintf(f, "      \"author_window_h\": %d,\n", author_h);
    fprintf(f, "      \"import_fit\": 0.25\n");
    fprintf(f, "    }");
    if (camera_path && camera_path[0]) {
        fprintf(f, ",\n    \"camera_path\": \"%s\"", camera_path);
    }
    if (light_path && light_path[0]) {
        fprintf(f, ",\n    \"light_path\": \"%s\"", light_path);
    }
    fprintf(f, "\n  },\n");
    fprintf(f, "  \"notes\": \"Auto-generated by physics_sim export\"\n");
    fprintf(f, "}\n");
    fclose(f);
    return true;
}

bool volume_frames_write(const SceneState *scene,
                         uint64_t frame_index) {
    SceneFluidFieldView2D fluid = {0};
    SimRuntimeBackendReport report = {0};
    bool export_vf3d = false;
    if (!scene) return false;
    if (!export_paths_init()) return false;
    char run_dir[512] = {0};
    const char *run_name = scene->preset && scene->preset->name ? scene->preset->name : "run";
    if (!export_paths_volume_run(run_name, run_dir, sizeof(run_dir))) {
        return false;
    }
    const char *dir = run_dir;
    export_vf3d = volume_frames_should_export_vf3d(scene, &report);

    if (export_vf3d) {
        VolumeFrameHeaderVf3dV1 header = {0};
        char path[512];
        char pack_path[512];
        char manifest_path[512];
        bool manifest_ok = false;

        snprintf(path, sizeof(path), "%s/frame_%06llu.vf3d",
                 dir, (unsigned long long)frame_index);
        if (!volume_frame_write_vf3d_raw(scene, frame_index, path, &report, &header)) {
            fprintf(stderr, "[export] Failed to write vf3d frame %s\n", path);
            return false;
        }

        manifest_ok = volume_frame_manifest_append_vf3d(scene, &header, path, dir);
        snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json", dir);
        if (build_pack_path(path, pack_path, sizeof(pack_path))) {
            const char *manifest_arg = manifest_ok ? manifest_path : NULL;
            CoreResult pack_r = core_pack_convert_vf3d(path, pack_path, manifest_arg);
            if (pack_r.code != CORE_OK) {
                fprintf(stderr, "[export] Warning: .pack export failed for %s (%s)\n", path, pack_r.message);
            }
        } else {
            fprintf(stderr, "[export] Warning: could not derive .pack path for %s\n", path);
        }
        if (!volume_frame_write_scene_bundle_vf3d(scene, &header, dir)) {
            fprintf(stderr, "[export] Warning: failed to update scene_bundle.json in %s\n", dir);
        }
        return true;
    }

    if (!scene_backend_fluid_view_2d(scene, &fluid)) return false;

    char path[512];
    snprintf(path, sizeof(path), "%s/frame_%06llu.vf2d",
             dir, (unsigned long long)frame_index);

    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "[export] Failed to open %s (%s)\n", path, strerror(errno));
        return false;
    }

    VolumeFrameHeaderV2 header = {
        .magic = VOLUME_MAGIC,
        .version = VOLUME_VERSION_V2,
        .grid_w = (uint32_t)fluid.width,
        .grid_h = (uint32_t)fluid.height,
        .time_seconds = scene->time,
        .frame_index = frame_index,
        .dt_seconds = scene->dt,
        .origin_x = 0.0f,
        .origin_y = 0.0f,
        .cell_size = 1.0f,
        .obstacle_mask_crc32 = obstacle_mask_crc32(scene),
        .reserved = {0, 0, 0}
    };
    size_t grid_count = fluid.cell_count;

    if (fwrite(&header, sizeof(header), 1, f) != 1) {
        fprintf(stderr, "[export] Failed to write header to %s\n", path);
        fclose(f);
        return false;
    }

    if (fwrite(fluid.density, sizeof(float), grid_count, f) != grid_count ||
        fwrite(fluid.velocity_x, sizeof(float), grid_count, f) != grid_count ||
        fwrite(fluid.velocity_y, sizeof(float), grid_count, f) != grid_count) {
        fprintf(stderr, "[export] Failed to write frame data to %s\n", path);
        fclose(f);
        return false;
    }

    fclose(f);

    bool manifest_ok = manifest_append(scene, &header, path, dir);
    char pack_path[512];
    char manifest_path[512];
    snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json", dir);
    if (build_pack_path(path, pack_path, sizeof(pack_path))) {
        const char *manifest_arg = manifest_ok ? manifest_path : NULL;
        CoreResult pack_r = core_pack_convert_vf2d(path, pack_path, manifest_arg);
        if (pack_r.code != CORE_OK) {
            fprintf(stderr, "[export] Warning: .pack export failed for %s (%s)\n", path, pack_r.message);
        }
    } else {
        fprintf(stderr, "[export] Warning: could not derive .pack path for %s\n", path);
    }
    if (!write_scene_bundle(scene, &header, dir)) {
        fprintf(stderr, "[export] Warning: failed to update scene_bundle.json in %s\n", dir);
    }
    if (!volume_frame_write_core_dataset_json(path, NULL, manifest_ok ? manifest_path : NULL)) {
        fprintf(stderr, "[export] Warning: failed to write core dataset sidecar for %s\n", path);
    }

    return true;
}
#else
bool volume_frames_write(const SceneState *scene,
                         uint64_t frame_index) {
    (void)scene;
    (void)frame_index;
    return false;
}
#endif

bool volume_frame_read_header(const char *path, VolumeFrameInfo *out_info) {
    if (!path || !out_info) return false;
    if (core_scene_path_is_scene_bundle(path)) {
        CoreSceneBundleInfo bundle;
        CoreResult r = core_scene_bundle_resolve(path, &bundle);
        if (r.code != CORE_OK) return false;
        if (strcmp(bundle.fluid_source_path, path) == 0) return false;
        return volume_frame_read_header(bundle.fluid_source_path, out_info);
    }
    if (strstr(path, "manifest.json") != NULL) {
        char frame_path[512];
        if (!resolve_manifest_latest_frame(path, frame_path, sizeof(frame_path))) return false;
        if (strcmp(frame_path, path) == 0) return false;
        return volume_frame_read_header(frame_path, out_info);
    }
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    uint32_t magic = 0;
    if (fread(&magic, sizeof(magic), 1, f) != 1) { fclose(f); return false; }
    if (magic != VOLUME_MAGIC) { fclose(f); return false; }

    uint32_t version = 0;
    if (fread(&version, sizeof(version), 1, f) != 1) { fclose(f); return false; }

    memset(out_info, 0, sizeof(*out_info));
    out_info->version = version;

    if (version == VOLUME_VERSION_V1) {
        VolumeFrameHeaderV1 h = {0};
        h.magic = magic;
        h.version = version;
        if (fread(&h.grid_w, sizeof(h) - sizeof(h.magic) - sizeof(h.version), 1, f) != 1) {
            fclose(f);
            return false;
        }
        out_info->grid_w = h.grid_w;
        out_info->grid_h = h.grid_h;
        out_info->time_seconds = h.time_seconds;
        out_info->frame_index = h.frame_index;
        out_info->dt_seconds = 0.0;
        out_info->cell_size = 1.0f;
        out_info->origin_x = 0.0f;
        out_info->origin_y = 0.0f;
        out_info->obstacle_mask_crc32 = 0;
        fclose(f);
        return true;
    } else if (version == VOLUME_VERSION_V2) {
        VolumeFrameHeaderV2 h = {0};
        h.magic = magic;
        h.version = version;
        if (fread(&h.grid_w, sizeof(h) - sizeof(h.magic) - sizeof(h.version), 1, f) != 1) {
            fclose(f);
            return false;
        }
        out_info->grid_w = h.grid_w;
        out_info->grid_h = h.grid_h;
        out_info->time_seconds = h.time_seconds;
        out_info->frame_index = h.frame_index;
        out_info->dt_seconds = h.dt_seconds;
        out_info->origin_x = h.origin_x;
        out_info->origin_y = h.origin_y;
        out_info->cell_size = h.cell_size;
        out_info->obstacle_mask_crc32 = h.obstacle_mask_crc32;
        fclose(f);
        return true;
    }

    fclose(f);
    return false;
}
