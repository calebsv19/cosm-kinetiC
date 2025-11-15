#ifndef EXPORT_PATHS_H
#define EXPORT_PATHS_H

#include <stdbool.h>

#define EXPORT_ROOT_DIR "export"
#define EXPORT_VOLUME_SUBDIR "volume_frames"
#define EXPORT_RENDER_SUBDIR "render_frames"
#define EXPORT_RENDER_VIDEO_SUBDIR "render_vid"

bool export_paths_init(void);
const char *export_volume_dir(void);
const char *export_render_dir(void);
const char *export_render_video_dir(void);

#endif // EXPORT_PATHS_H
