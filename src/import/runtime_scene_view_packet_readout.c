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
    core_scene_view_packet_summary_init(&readout->core_summary);
}

bool physics_sim_scene_view_packet_readout_from_json(
    const char *packet_json,
    PhysicsSimSceneViewPacketReadout *out_readout,
    char *out_diagnostics,
    size_t out_diagnostics_size) {
    CoreResult result = core_result_ok();
    CoreSceneViewPacketReadback readback;
    CoreSceneViewPacketSummary summary;

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
    core_scene_view_packet_summary_init(&summary);
    result = core_scene_view_packet_summary_from_readback(&readback, &summary);
    if (result.code != CORE_OK) {
        write_diagnostics(out_diagnostics,
                          out_diagnostics_size,
                          result.message ? result.message : "invalid scene-view summary");
        return false;
    }

    out_readout->valid = summary.valid;
    out_readout->read_only = summary.readOnly;
    out_readout->projected = summary.projected;
    out_readout->complete = summary.complete;
    out_readout->focused_object_index = summary.focusedObjectIndex;
    out_readout->triangle_count = summary.triangleCount;
    out_readout->face_group_count = summary.faceGroupCount;
    out_readout->first_face_group_index = summary.firstFaceGroupIndex;
    out_readout->last_face_group_index = summary.lastFaceGroupIndex;
    out_readout->first_alpha = summary.firstAlpha;
    out_readout->preview_quality = summary.previewQuality;
    out_readout->degraded_reason = summary.degradedReason;
    out_readout->material_preview = summary.materialPreview;
    out_readout->transparent_preview = summary.transparentPreview;
    out_readout->emissive_preview = summary.emissivePreview;
    out_readout->mirror_preview = summary.mirrorPreview;
    out_readout->textured_preview = summary.texturedPreview;
    out_readout->core_readback = readback;
    out_readout->core_summary = summary;
    write_diagnostics(out_diagnostics, out_diagnostics_size, "scene-view packet read-only");
    return true;
}
