#ifndef COLLIDER_TAGGING_H
#define COLLIDER_TAGGING_H

#include "geo/shape_library.h"
#include "render/import_project.h"
#include "physics/rigid/collider_types.h"

int collider_tag_closed_path(const ShapeAssetPath *path,
                             float corner_thresh_deg,
                             TaggedPoint *out,
                             int max_out,
                             int *out_corner_count);

int collider_tag_closed_points(const HullPoint *pts,
                               int count,
                               float corner_thresh_deg,
                               TaggedPoint *out,
                               int max_out,
                               int *out_corner_count);

int collider_simplify_loop_preserve_corners(const ShapeAssetPath *path,
                                            float corner_thresh_deg,
                                            float eps,
                                            HullPoint *out,
                                            int max_out,
                                            int *out_corner_count);

int collider_project_simplified_to_grid(const HullPoint *in,
                                        int in_count,
                                        float cx,
                                        float cy,
                                        float norm,
                                        const ImportProjectParams *proj,
                                        HullPoint *out,
                                        int max_out);

int collider_collapse_collinear(const HullPoint *in, int in_count, HullPoint *out, int max_out);
int collider_simplify_poly(const HullPoint *pts, int n, HullPoint *out, int max_out, float epsilon);
int collider_compute_convex_hull(const HullPoint *pts, int count, HullPoint *out, int max_out);
int collider_simplify_intent(const HullPoint *pts,
                             int n,
                             HullPoint *out,
                             int max_out,
                             float min_angle_deg,
                             float min_edge_len);

#endif // COLLIDER_TAGGING_H
