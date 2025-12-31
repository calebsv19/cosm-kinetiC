#ifndef COLLIDER_PARTITION_H
#define COLLIDER_PARTITION_H

#include <stdbool.h>
#include "physics/rigid/collider_types.h"

int collider_partition_trapezoids(const HullPoint *pts,
                                  int count,
                                  const bool *concave_flags,
                                  HullPoint parts[][8],
                                  int *part_counts,
                                  int max_parts,
                                  bool debug_logs);

#endif // COLLIDER_PARTITION_H
