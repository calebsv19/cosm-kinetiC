#include "export/export_paths.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>
#endif

static bool s_initialized = false;
static char s_volume_dir[256] = {0};
static char s_render_dir[256] = {0};
static char s_render_video_dir[256] = {0};

static bool ensure_dir(const char *path) {
    if (!path || !*path) return false;
    int result = 0;
#ifdef _WIN32
    result = _mkdir(path);
#else
    result = mkdir(path, 0755);
#endif
    if (result == 0 || errno == EEXIST) {
        return true;
    }
    if (errno == EEXIST) return true;
    if (errno == ENOENT) return false;
    return false;
}

bool export_paths_init(void) {
    if (s_initialized) return true;

    if (!ensure_dir(EXPORT_ROOT_DIR)) {
        fprintf(stderr, "[export] Failed to create %s (%s)\n",
                EXPORT_ROOT_DIR, strerror(errno));
        return false;
    }

    snprintf(s_volume_dir, sizeof(s_volume_dir),
             "%s/%s", EXPORT_ROOT_DIR, EXPORT_VOLUME_SUBDIR);
    snprintf(s_render_dir, sizeof(s_render_dir),
             "%s/%s", EXPORT_ROOT_DIR, EXPORT_RENDER_SUBDIR);
    snprintf(s_render_video_dir, sizeof(s_render_video_dir),
             "%s/%s", EXPORT_ROOT_DIR, EXPORT_RENDER_VIDEO_SUBDIR);

    if (!ensure_dir(s_volume_dir)) {
        fprintf(stderr, "[export] Failed to create %s (%s)\n",
                s_volume_dir, strerror(errno));
        return false;
    }
    if (!ensure_dir(s_render_dir)) {
        fprintf(stderr, "[export] Failed to create %s (%s)\n",
                s_render_dir, strerror(errno));
        return false;
    }
    if (!ensure_dir(s_render_video_dir)) {
        fprintf(stderr, "[export] Failed to create %s (%s)\n",
                s_render_video_dir, strerror(errno));
        return false;
    }

    s_initialized = true;
    return true;
}

const char *export_volume_dir(void) {
    return s_volume_dir[0] ? s_volume_dir : NULL;
}

const char *export_render_dir(void) {
    return s_render_dir[0] ? s_render_dir : NULL;
}

const char *export_render_video_dir(void) {
    return s_render_video_dir[0] ? s_render_video_dir : NULL;
}
