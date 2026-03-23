#include <stdio.h>

#include "export/volume_frames.h"

static int usage(const char *argv0) {
    fprintf(stderr, "usage: %s <vf2d_path> <dataset_json_path> [manifest_json]\n", argv0);
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 3 && argc != 4) return usage(argv[0]);

    const char *vf2d_path = argv[1];
    const char *dataset_path = argv[2];
    const char *manifest_path = (argc == 4) ? argv[3] : NULL;

    if (!volume_frame_write_core_dataset_json(vf2d_path, dataset_path, manifest_path)) {
        fprintf(stderr, "vf2d_dataset_tool: failed to create dataset json\n");
        return 2;
    }

    printf("dataset json written: %s\n", dataset_path);
    return 0;
}
