#ifndef EXPORT_PATHS_H
#define EXPORT_PATHS_H

#include <stdbool.h>
#include <stddef.h>

#define EXPORT_ROOT_DIR "export"
#define EXPORT_VOLUME_SUBDIR "volume_frames"
#define EXPORT_RENDER_SUBDIR "render_frames"
#define EXPORT_RENDER_VIDEO_SUBDIR "render_vid"

// Build a per-run volume directory: export/volume_frames/<run_name>
// run_name is typically the preset name, sanitized.
bool export_paths_volume_run(const char *run_name, char *out_dir, size_t out_size);

bool export_paths_init(void);
const char *export_volume_dir(void);
const char *export_render_dir(void);
const char *export_render_video_dir(void);

#endif // EXPORT_PATHS_H
