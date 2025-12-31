#ifndef COLLIDER_GEOM_H
#define COLLIDER_GEOM_H

#include <stdbool.h>
#include "physics/rigid/collider_types.h"

float polygon_area(const HullPoint *pts, int count);
float polygon_area_signed(const HullPoint *pts, int count);
HullPoint polygon_centroid(const HullPoint *pts, int count);
bool point_in_polygon(const HullPoint *poly, int count, HullPoint p);
bool point_in_triangle(const HullPoint *p, const HullPoint *a, const HullPoint *b, const HullPoint *c);
bool polygon_convex(const HullPoint *pts, int count);
bool region_contains_point_mask(const bool mask[128][128],
                                const bool visited[128][128],
                                int res,
                                float minx,
                                float spanx,
                                float miny,
                                float spany,
                                float gx,
                                float gy);
int collider_build_regions(const HullPoint *pts,
                           int count,
                           ColliderRegion *regions,
                           int max_regions);

#endif // COLLIDER_GEOM_H
