#include "import/runtime_scene_view_packet_readout.h"

#include <stdio.h>
#include <string.h>

static void write_diagnostics(char *out_diagnostics,
                              size_t out_diagnostics_size,
                              const char *message) {
    if (!out_diagnostics || out_diagnostics_size == 0u) return;
    snprintf(out_diagnostics, out_diagnostics_size, "%s", message ? message : "");
}

void physics_sim_scene_view_packet_readout_init(PhysicsSimSceneViewPacketReadout *readout) {
    if (!readout) return;
    memset(readout, 0, sizeof(*readout));
    readout->read_only = true;
    core_scene_view_packet_readback_init(&readout->core_readback);
}

bool physics_sim_scene_view_packet_readout_from_json(
    const char *packet_json,
    PhysicsSimSceneViewPacketReadout *out_readout,
    char *out_diagnostics,
    size_t out_diagnostics_size) {
    CoreResult result = core_result_ok();
    CoreSceneViewPacketReadback readback;

    if (out_readout) physics_sim_scene_view_packet_readout_init(out_readout);
    write_diagnostics(out_diagnostics, out_diagnostics_size, "");
    if (!packet_json || !out_readout) {
        write_diagnostics(out_diagnostics,
                          out_diagnostics_size,
                          "invalid scene-view readout args");
        return false;
    }

    core_scene_view_packet_readback_init(&readback);
    result = core_scene_view_packet_readback_from_json_string(packet_json, &readback);
    if (result.code != CORE_OK) {
        write_diagnostics(out_diagnostics,
                          out_diagnostics_size,
                          result.message ? result.message : "invalid scene-view packet");
        return false;
    }

    out_readout->valid = true;
    out_readout->read_only = true;
    out_readout->projected = readback.projected;
    out_readout->complete = readback.complete;
    out_readout->focused_object_index = readback.focusedObjectIndex;
    out_readout->triangle_count = readback.triangleCount;
    out_readout->face_group_count = readback.faceGroupCount;
    out_readout->first_face_group_index = readback.firstPickId.faceGroupIndex;
    out_readout->last_face_group_index = readback.lastPickId.faceGroupIndex;
    out_readout->first_alpha = readback.firstAlpha;
    out_readout->preview_quality = readback.previewQuality;
    out_readout->degraded_reason = readback.degradedReason;
    out_readout->material_preview =
        readback.previewQuality == CORE_SCENE_VIEW_PREVIEW_MATERIAL;
    out_readout->transparent_preview =
        (readback.firstDisplayFlags & CORE_SCENE_VIEW_DISPLAY_TRANSPARENT) != 0u ||
        readback.firstAlpha < 255u;
    out_readout->emissive_preview =
        (readback.firstDisplayFlags & CORE_SCENE_VIEW_DISPLAY_EMISSIVE) != 0u;
    out_readout->mirror_preview =
        (readback.firstDisplayFlags & CORE_SCENE_VIEW_DISPLAY_MIRROR) != 0u;
    out_readout->textured_preview =
        (readback.firstDisplayFlags & CORE_SCENE_VIEW_DISPLAY_TEXTURED) != 0u;
    out_readout->core_readback = readback;
    write_diagnostics(out_diagnostics, out_diagnostics_size, "scene-view packet read-only");
    return true;
}
