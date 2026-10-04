#!/usr/bin/env python3
"""C3D-7C controlled nonuniform natural-traction space/time/extension gate."""
import array
import hashlib
import json
import math
from pathlib import Path
import subprocess
import time
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/c3d-wall"
BIN = OUT / "open_transient3d_test"


def run(n, dt, L, name):
    field = OUT / (name + ".bin")
    start = time.monotonic()
    p = subprocess.run([str(BIN), str(n), str(dt), ".4", str(field), str(L)],
                       check=True, capture_output=True, text=True)
    row = json.loads(p.stdout)
    row["wall_s"] = time.monotonic() - start
    assert row["residual"] <= 1e-11 and row["max_divergence"] < 1e-8, row
    (OUT / (name + ".json")).write_text(json.dumps(row, indent=2) + "\n")
    print(json.dumps({"case": name, **row}), flush=True)
    a = array.array("d")
    a.frombytes(field.read_bytes())
    return row, a


def rms(a, b):
    assert len(a) == len(b)
    return math.sqrt(sum((x-y)**2 for x, y in zip(a, b)) / len(a))


def upstream(f, L):
    n = 32
    nx = int(L * n / 2)
    ucount = (nx + 1) * n * n
    vcount = nx * (n - 1) * n
    m = ucount + vcount + nx * n * (n - 1)
    v = []
    for a, shape, offset in ((0, (nx + 1, n, n), 0),
                             (1, (nx, n - 1, n), ucount),
                             (2, (nx, n, n - 1), ucount + vcount)):
        for k in range(shape[2]):
            for j in range(shape[1]):
                for i in range(16, 49 if a == 0 else 48):
                    v.append(f[offset + (k * shape[1] + j) * shape[0] + i])
    p = [f[m + (k * n + j) * nx + i]
         for k in range(n) for j in range(n) for i in range(16, 48)]
    return v, p


def accepted(row):
    return all(row[k] <= limit for k, limit in (("velocity_error", .03), ("pressure_error", .03),
               ("wall_error", .05), ("dissipation_error", .05), ("energy_imbalance", .05)))


def main():
    result = {"schema": "physics_sim_c3d_nonuniform_open_transient_v1", "passed": False,
              "binary_sha256": hashlib.sha256(BIN.read_bytes()).hexdigest(),
              "scope": "independently forced unsteady Stokes with declared natural tractions"}
    path = OUT / "qualification-open-transient.json"
    try:
        spatial = [run(n, .005, 4, "co-spatial" + str(n)) for n in (8, 16, 32)]
        result["spatial"] = [r for r, _ in spatial]
        orders = [{k: math.log(a[k] / b[k], 2) for k in
                   ("velocity_error", "pressure_error", "wall_error", "dissipation_error")}
                  for (a, _), (b, _) in zip(spatial, spatial[1:])]
        result["spatial_orders"] = orders
        assert accepted(spatial[-1][0]), spatial[-1][0]
        assert min(orders[-1].values()) >= 1.8, orders
        temporal = [run(16, dt, 4, "co-time" + str(dt)) for dt in (.04, .02, .01, .005, .0025)]
        result["temporal"] = [r for r, _ in temporal]
        m = (32 + 1) * 16**2 + 2 * 32 * 16 * 15
        changes = [{"velocity": rms(a[:m], b[:m]), "pressure": rms(a[m:], b[m:])}
                   for (_, a), (_, b) in zip(temporal, temporal[1:])]
        time_orders = [{k: math.log(a[k] / b[k], 2) for k in a}
                       for a, b in zip(changes, changes[1:])]
        result["temporal_changes"] = changes
        result["temporal_orders"] = time_orders
        assert all(v >= 1.8 for row in time_orders[-2:] for v in row.values()), time_orders
        extensions = [spatial[-1]] + [run(32, .005, L, "co-outlet" + str(L)) for L in (6, 8)]
        result["outlet_runs"] = [r for r, _ in extensions]
        result["outlet_changes"] = []
        for (a, fa), (b, fb) in zip(extensions, extensions[1:]):
            va, pa = upstream(fa, a["length"])
            vb, pb = upstream(fb, b["length"])
            delta = {"velocity_upstream": rms(va, vb) / math.sqrt(sum(v*v for v in va) / len(va)),
                     "pressure_upstream_peak_normalized": rms(pa, pb) / .01,
                     "kinetic_per_length": abs((b["kinetic_j"] / b["length"]) /
                                              (a["kinetic_j"] / a["length"]) - 1),
                     "dissipation_per_length": abs((b["dissipation_w"] / b["length"]) /
                                                  (a["dissipation_w"] / a["length"]) - 1)}
            result["outlet_changes"].append({"from": a["length"], "to": b["length"], **delta})
            assert accepted(b) and max(delta.values()) <= .01, (b, delta)
        result["passed"] = True
    finally:
        path.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"nonuniform_open_gate_passed": True, "evidence": str(path)}), flush=True)


if __name__ == "__main__":
    main()
