#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PACK_CLI="$ROOT_DIR/../shared/core/core_pack/build/pack_cli"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

VF2D_PATH="$TMP_DIR/frame_000005.vf2d"
PACK_PATH="$TMP_DIR/frame_000005.pack"
DATASET_PATH="$TMP_DIR/frame_000005.dataset.json"
PACK_INSPECT="$TMP_DIR/pack_inspect.txt"

cat > "$TMP_DIR/gen_vf2d.c" <<'EOF'
#include <stdint.h>
#include <stdio.h>

typedef struct VolumeFrameHeaderV2 {
    uint32_t magic;
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    double   time_seconds;
    uint64_t frame_index;
    double   dt_seconds;
    float    origin_x;
    float    origin_y;
    float    cell_size;
    uint32_t obstacle_mask_crc32;
    uint32_t reserved[3];
} VolumeFrameHeaderV2;

int main(int argc, char **argv) {
    if (argc != 2) return 2;

    FILE *f = fopen(argv[1], "wb");
    if (!f) return 3;

    VolumeFrameHeaderV2 h = {0};
    h.magic = ('V' << 24) | ('F' << 16) | ('R' << 8) | ('M');
    h.version = 2;
    h.grid_w = 3;
    h.grid_h = 2;
    h.time_seconds = 2.0;
    h.frame_index = 5;
    h.dt_seconds = 0.02;
    h.origin_x = 1.0f;
    h.origin_y = -2.0f;
    h.cell_size = 0.5f;
    h.obstacle_mask_crc32 = 6789u;

    float density[6] = {1, 2, 3, 4, 5, 6};
    float velx[6] = {0, 1, 0, 1, 0, 1};
    float vely[6] = {1, 0, 1, 0, 1, 0};

    if (fwrite(&h, sizeof(h), 1, f) != 1) return 4;
    if (fwrite(density, sizeof(float), 6, f) != 6) return 5;
    if (fwrite(velx, sizeof(float), 6, f) != 6) return 6;
    if (fwrite(vely, sizeof(float), 6, f) != 6) return 7;
    fclose(f);
    return 0;
}
EOF

cc -std=c11 -Wall -Wextra -Wpedantic -o "$TMP_DIR/gen_vf2d" "$TMP_DIR/gen_vf2d.c"
"$TMP_DIR/gen_vf2d" "$VF2D_PATH"

"$ROOT_DIR/vf2d_pack_tool" "$VF2D_PATH" "$PACK_PATH"
"$ROOT_DIR/vf2d_dataset_tool" "$VF2D_PATH" "$DATASET_PATH"

if [[ ! -f "$PACK_PATH" || ! -f "$DATASET_PATH" ]]; then
    echo "vf2d pack/dataset parity test failed: expected artifacts missing"
    exit 10
fi

make -C "$ROOT_DIR/../shared/core/core_pack" tools >/dev/null
"$PACK_CLI" inspect "$PACK_PATH" > "$PACK_INSPECT"

grep -q "chunk_count=4" "$PACK_INSPECT" || {
    echo "vf2d pack/dataset parity test failed: unexpected chunk_count"
    exit 11
}
grep -q "type=VFHD" "$PACK_INSPECT" || {
    echo "vf2d pack/dataset parity test failed: missing VFHD"
    exit 12
}
grep -q "type=DENS" "$PACK_INSPECT" || {
    echo "vf2d pack/dataset parity test failed: missing DENS"
    exit 13
}
grep -q "type=VELX" "$PACK_INSPECT" || {
    echo "vf2d pack/dataset parity test failed: missing VELX"
    exit 14
}
grep -q "type=VELY" "$PACK_INSPECT" || {
    echo "vf2d pack/dataset parity test failed: missing VELY"
    exit 15
}

grep -q '"schema_family"[[:space:]]*:[[:space:]]*"physics_sim_volume_dataset"' "$DATASET_PATH" || {
    echo "vf2d pack/dataset parity test failed: missing schema_family"
    exit 16
}
grep -q '"schema_variant"[[:space:]]*:[[:space:]]*"vf2d"' "$DATASET_PATH" || {
    echo "vf2d pack/dataset parity test failed: missing schema_variant"
    exit 17
}
grep -q '"frame_index"[[:space:]]*:[[:space:]]*5' "$DATASET_PATH" || {
    echo "vf2d pack/dataset parity test failed: missing frame_index"
    exit 18
}
grep -q '"grid_w"[[:space:]]*:[[:space:]]*3' "$DATASET_PATH" || {
    echo "vf2d pack/dataset parity test failed: missing grid_w"
    exit 19
}
grep -q '"grid_h"[[:space:]]*:[[:space:]]*2' "$DATASET_PATH" || {
    echo "vf2d pack/dataset parity test failed: missing grid_h"
    exit 20
}

echo "vf2d pack/dataset parity test passed."
