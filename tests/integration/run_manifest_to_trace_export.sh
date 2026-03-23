#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PACK_CLI="$ROOT_DIR/../shared/core/core_pack/build/pack_cli"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

VF2D_PATH="$TMP_DIR/frame_000042.vf2d"
MANIFEST_PATH="$TMP_DIR/manifest.json"
TRACE_PATH="$TMP_DIR/trace.pack"
INSPECT_LOG="$TMP_DIR/inspect.txt"

cat > "$TMP_DIR/gen_trace_fixture.c" <<'EOF'
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
    if (argc != 3) return 2;

    FILE *f = fopen(argv[1], "wb");
    if (!f) return 3;

    VolumeFrameHeaderV2 h = {0};
    h.magic = ('V' << 24) | ('F' << 16) | ('R' << 8) | ('M');
    h.version = 2;
    h.grid_w = 2;
    h.grid_h = 2;
    h.time_seconds = 1.5;
    h.frame_index = 42;
    h.dt_seconds = 0.016;
    h.origin_x = 0.0f;
    h.origin_y = 0.0f;
    h.cell_size = 1.0f;
    h.obstacle_mask_crc32 = 1234u;

    float density[4] = {0.2f, 0.4f, 0.6f, 0.8f};
    float velx[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float vely[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    if (fwrite(&h, sizeof(h), 1, f) != 1) return 4;
    if (fwrite(density, sizeof(float), 4, f) != 4) return 5;
    if (fwrite(velx, sizeof(float), 4, f) != 4) return 6;
    if (fwrite(vely, sizeof(float), 4, f) != 4) return 7;
    fclose(f);

    FILE *m = fopen(argv[2], "wb");
    if (!m) return 8;
    fputs("{\n  \"frames\": [ { \"path\": \"frame_000042.vf2d\" } ]\n}\n", m);
    fclose(m);
    return 0;
}
EOF

cc -std=c11 -Wall -Wextra -Wpedantic -o "$TMP_DIR/gen_trace_fixture" "$TMP_DIR/gen_trace_fixture.c"
"$TMP_DIR/gen_trace_fixture" "$VF2D_PATH" "$MANIFEST_PATH"

"$ROOT_DIR/physics_trace_tool" "$MANIFEST_PATH" "$TRACE_PATH" 24
if [[ ! -f "$TRACE_PATH" ]]; then
    echo "manifest trace export test failed: missing trace pack at $TRACE_PATH"
    exit 10
fi

make -C "$ROOT_DIR/../shared/core/core_pack" tools >/dev/null
"$PACK_CLI" inspect "$TRACE_PATH" > "$INSPECT_LOG"

grep -q "chunk_count=3" "$INSPECT_LOG" || {
    echo "manifest trace export test failed: unexpected chunk_count"
    exit 11
}
grep -q "lane=frame_dt" "$INSPECT_LOG" || {
    echo "manifest trace export test failed: missing frame_dt lane"
    exit 12
}
grep -q "lane=solver_iterations" "$INSPECT_LOG" || {
    echo "manifest trace export test failed: missing solver_iterations lane"
    exit 13
}
grep -q "lane=density_avg" "$INSPECT_LOG" || {
    echo "manifest trace export test failed: missing density_avg lane"
    exit 14
}
grep -q "label=trace_start" "$INSPECT_LOG" || {
    echo "manifest trace export test failed: missing trace_start marker"
    exit 15
}
grep -q "label=trace_end" "$INSPECT_LOG" || {
    echo "manifest trace export test failed: missing trace_end marker"
    exit 16
}

echo "manifest trace export test passed."
