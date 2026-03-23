#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

VF2D_PATH="$TMP_DIR/frame_000001.vf2d"
DATASET_PATH="$TMP_DIR/frame_000001.dataset.json"

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
    h.grid_w = 2;
    h.grid_h = 2;
    h.time_seconds = 1.25;
    h.frame_index = 7;
    h.dt_seconds = 0.016;
    h.origin_x = 0.0f;
    h.origin_y = 0.0f;
    h.cell_size = 1.0f;
    h.obstacle_mask_crc32 = 1234u;

    float density[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    float velx[4] = {1.0f, 0.0f, -1.0f, 0.5f};
    float vely[4] = {0.0f, 1.0f, 0.0f, -1.0f};

    if (fwrite(&h, sizeof(h), 1, f) != 1) return 4;
    if (fwrite(density, sizeof(float), 4, f) != 4) return 5;
    if (fwrite(velx, sizeof(float), 4, f) != 4) return 6;
    if (fwrite(vely, sizeof(float), 4, f) != 4) return 7;
    fclose(f);
    return 0;
}
EOF

cc -std=c11 -Wall -Wextra -Wpedantic -o "$TMP_DIR/gen_vf2d" "$TMP_DIR/gen_vf2d.c"
"$TMP_DIR/gen_vf2d" "$VF2D_PATH"

"$ROOT_DIR/vf2d_dataset_tool" "$VF2D_PATH" "$DATASET_PATH"

if [[ ! -f "$DATASET_PATH" ]]; then
    echo "dataset export test failed: missing output json at $DATASET_PATH"
    exit 10
fi

grep -q '"profile"[[:space:]]*:[[:space:]]*"physics_sim_volume_dataset_v1"' "$DATASET_PATH" || {
    echo "dataset export test failed: missing profile marker"
    exit 11
}
grep -q '"frame_header"' "$DATASET_PATH" || {
    echo "dataset export test failed: missing frame_header object"
    exit 12
}
grep -q '"frame_index"[[:space:]]*:[[:space:]]*7' "$DATASET_PATH" || {
    echo "dataset export test failed: missing frame_index value"
    exit 13
}
grep -q '"dataset_schema"[[:space:]]*:[[:space:]]*"physics_sim.volume_dataset"' "$DATASET_PATH" || {
    echo "dataset export test failed: missing dataset schema marker"
    exit 14
}
grep -q '"schema_family"[[:space:]]*:[[:space:]]*"physics_sim_volume_dataset"' "$DATASET_PATH" || {
    echo "dataset export test failed: missing schema_family marker"
    exit 17
}
grep -q '"schema_variant"[[:space:]]*:[[:space:]]*"vf2d"' "$DATASET_PATH" || {
    echo "dataset export test failed: missing schema_variant marker"
    exit 18
}
grep -q '"volume_frame_session_v1"' "$DATASET_PATH" || {
    echo "dataset export test failed: missing session table marker"
    exit 15
}
grep -q '"volume_frame_fields_v1"' "$DATASET_PATH" || {
    echo "dataset export test failed: missing field table marker"
    exit 16
}

echo "vf2d dataset export test passed."
