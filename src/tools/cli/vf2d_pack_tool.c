#include <stdio.h>
#include <string.h>

#include "core_pack.h"

static int usage(const char *argv0) {
    fprintf(stderr, "usage: %s <vf2d_path> <pack_path> [manifest_json]\n", argv0);
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 3 && argc != 4) {
        return usage(argv[0]);
    }

    const char *vf2d_path = argv[1];
    const char *pack_path = argv[2];
    const char *manifest = (argc == 4) ? argv[3] : NULL;

    CoreResult r = core_pack_convert_vf2d(vf2d_path, pack_path, manifest);
    if (r.code != CORE_OK) {
        fprintf(stderr, "vf2d_pack_tool: convert failed: %s\n", r.message ? r.message : "unknown error");
        return 2;
    }

    printf("converted %s -> %s\n", vf2d_path, pack_path);
    if (manifest && manifest[0]) {
        printf("manifest embedded: %s\n", manifest);
    }
    return 0;
}
