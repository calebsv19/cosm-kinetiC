#ifndef PHYSICS_SIM_RUNTIME_SCENE_VIEW_PACKET_READOUT_H
#define PHYSICS_SIM_RUNTIME_SCENE_VIEW_PACKET_READOUT_H

#include <stdbool.h>
#include <stddef.h>

#include "core_scene_view.h"

typedef struct PhysicsSimSceneViewPacketReadout {
    bool valid;
    bool read_only;
    bool material_preview;
    bool transparent_preview;
    bool emissive_preview;
    bool mirror_preview;
    bool textured_preview;
    bool projected;
    bool complete;
    int focused_object_index;
    int triangle_count;
    int face_group_count;
    int first_face_group_index;
    int last_face_group_index;
    unsigned char first_alpha;
    CoreSceneViewPreviewQuality preview_quality;
    CoreSceneViewDegradedReason degraded_reason;
    CoreSceneViewPacketReadback core_readback;
} PhysicsSimSceneViewPacketReadout;

void physics_sim_scene_view_packet_readout_init(PhysicsSimSceneViewPacketReadout *readout);

bool physics_sim_scene_view_packet_readout_from_json(
    const char *packet_json,
    PhysicsSimSceneViewPacketReadout *out_readout,
    char *out_diagnostics,
    size_t out_diagnostics_size);

#endif
