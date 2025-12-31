#ifndef COLLIDER_CLASSIFY_H
#define COLLIDER_CLASSIFY_H

#include "physics/rigid/collider_types.h"

ColliderSegmentClass classify_span_basic(const HullPoint *span, int count);
int classify_segments(const HullPoint *pts,
                      int count,
                      const bool *corner_flags,
                      const bool *concave_flags,
                      ColliderSegment *out,
                      int max_out,
                      bool debug);

#endif // COLLIDER_CLASSIFY_H
