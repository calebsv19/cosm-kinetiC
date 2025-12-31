#ifndef COLLIDER_PRIMITIVES_H
#define COLLIDER_PRIMITIVES_H

#include "app/app_config.h"
#include "physics/rigid/collider_types.h"

int collider_fit_primitives(const HullPoint *pts,
                            int count,
                            const ColliderSegment *segments,
                            int seg_count,
                            const AppConfig *cfg,
                            ColliderPrimitive *out,
                            int max_out,
                            const bool region_mask[128][128],
                            const bool region_visited[128][128],
                            int region_res,
                            float minx, float spanx,
                            float miny, float spany);

#endif // COLLIDER_PRIMITIVES_H
