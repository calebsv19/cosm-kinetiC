#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "import/shape_asset_output.h"
#include "app/physics_sim_persistence.h"
#include "import/shape_asset_input.h"
#include <string.h>
#include <sys/stat.h>
bool physics_sim_shape_asset_publish(const ShapeAsset *asset, const char *input, const char *path) {
    if (!input || !path || !physics_sim_shape_asset_admitted(asset)) return false;
    struct stat source, output;
    if (stat(input, &source) == 0 && lstat(path, &output) == 0 &&
        source.st_dev == output.st_dev && source.st_ino == output.st_ino) {
        fprintf(stderr, "Output aliases shape input\n");
        return false;
    }
    char *text = shape_asset_to_json_text(asset);
    if (!text) return false;
    size_t length = strlen(text);
    const uint64_t byte_limit = PHYSICS_SIM_ASSET_MAX_BYTES;
    bool ok = false;
    if (length <= byte_limit && physics_sim_shape_asset_text_admitted(text, length)) {
        PhysicsSimPersistence save;
        FILE *stream = physics_sim_persistence_begin_bounded(path, byte_limit, &save);
        if (stream) {
            if (fwrite(text, 1, length, stream) == length)
                ok = physics_sim_persistence_finish(&save, stream);
            else physics_sim_persistence_abort(&save, stream);
        }
    }
    shape_asset_json_text_free(text);
    return ok;
}
