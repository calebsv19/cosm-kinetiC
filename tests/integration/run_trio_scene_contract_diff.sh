#!/usr/bin/env bash
set -euo pipefail
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
source "$REPO/tests/integration/fixture_support.sh"
physics_fixture_supervise "$REPO" "$0" "$@"
SHARED="$REPO/third_party/codework_shared"
DIFF_DIR="$SHARED/core/core_scene_compile"
FIX_DIR="$SHARED/assets/scenes/trio_contract"
make -C "$DIFF_DIR" scene-contract-diff >/dev/null
DIFF_BIN="$DIFF_DIR/build/scene_contract_diff"
"$DIFF_BIN" "$FIX_DIR/scene_runtime_min.json" "$FIX_DIR/scene_runtime_min_reordered.json" >/dev/null
SCRATCH="$(physics_fixture_root "$REPO" trio_scene_contract_diff)"
actual="$SCRATCH/actual.json"
python3 - "$FIX_DIR/scene_runtime_min.json" "$actual" <<'PYTHON'
import json,sys
value=json.load(open(sys.argv[1]));value['space_mode_default']='3d'
with open(sys.argv[2],'w') as out:json.dump(value,out)
PYTHON
if "$DIFF_BIN" "$FIX_DIR/scene_runtime_min.json" "$actual" >/dev/null 2>&1; then
    echo "expected scene drift was not detected" >&2
    exit 1
fi
echo "scene contract diff smoke passed"
