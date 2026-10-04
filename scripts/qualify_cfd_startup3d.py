#!/usr/bin/env python3
"""Physical startup: three grids, independent time refinement and open extensions."""
import array
import hashlib
import json
import math
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/c3d-wall"
BIN = OUT / "startup3d_test"


def run(n, dt, length, times, name):
    prefix = OUT / name
    command = [str(BIN), str(n), str(dt), str(max(times)), str(length), str(prefix),
               ",".join(str(t) for t in times)]
    start = time.monotonic()
    rows = []
    with (OUT / (name + ".stderr")).open("w") as err:
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=err, text=True)
        for line in process.stdout:
            row = json.loads(line)
            row["elapsed_wall_s"] = time.monotonic() - start
            rows.append(row)
            print(json.dumps({"case": name, **row}), flush=True)
        code = process.wait()
    (OUT / (name + ".json")).write_text(json.dumps(rows, indent=2) + "\n")
    assert code == 0 and len(rows) == len(times), (name, code)
    for row in rows:
        assert row["residual"] <= 1e-11 and row["divergence"] < 1e-8, row
    return rows


def field(name, t):
    a = array.array("d")
    a.frombytes((OUT / (name + "-t" + format(t, ".6g") + ".bin")).read_bytes())
    return a


def difference(a, b, m):
    assert len(a) == len(b)
    return {"velocity": math.sqrt(sum((x-y)**2 for x, y in zip(a[:m], b[:m])) / m),
            "pressure": math.sqrt(sum((x-y)**2 for x, y in zip(a[m:], b[m:])) / (len(a)-m))}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    result = {"schema": "physics_sim_c3d_startup_qualification_v1", "passed": False,
              "binary_sha256": hashlib.sha256(BIN.read_bytes()).hexdigest(),
              "scope": "pressure-driven laminar startup; no nonuniform open-transient claim"}
    path = OUT / "qualification-startup.json"
    try:
        times = [.5, 2, 12]
        spatial = [run(n, .02, 4, times, "c-spatial" + str(n)) for n in (8, 16, 32)]
        result["spatial"] = spatial
        orders = []
        for k, t in enumerate(times):
            current = []
            for a, b in zip(spatial, spatial[1:]):
                current.append({key: math.log(a[k][key] / b[k][key], 2)
                                for key in ("velocity_error", "flow_error", "wall_error", "dissipation_error")})
            finest = spatial[-1][k]
            assert current[-1]["velocity_error"] >= 1.8, (t, current)
            assert all(finest[key] <= limit for key, limit in
                       (("velocity_error", .01), ("pressure_error", .01), ("flow_error", .01),
                        ("wall_error", .02), ("dissipation_error", .02), ("energy_imbalance", .02))), finest
            orders.append({"time": t, "orders": current})
        result["spatial_orders"] = orders
        temporal = []
        names = []
        for dt in (.05, .025, .0125, .00625, .003125):
            name = "c-time" + str(dt)
            names.append(name)
            temporal.append(run(16, dt, 4, [.5, 2], name))
        result["temporal"] = temporal
        m = 3 * 32 * 16**2 + 16**2 - 32 * (16 + 16)
        time_results = []
        for t in (.5, 2):
            fields = [field(name, t) for name in names]
            changes = [difference(a, b, m) for a, b in zip(fields, fields[1:])]
            orders = [math.log(a["velocity"] / b["velocity"], 2) for a, b in zip(changes, changes[1:])]
            assert min(orders[-2:]) >= 1.8, (t, orders)
            # Pressure is exactly linear in this solution. Report its roundoff
            # differences; a fabricated temporal order would be meaningless.
            assert max(c["pressure"] for c in changes) < 1e-10
            time_results.append({"time": t, "changes": changes, "velocity_orders": orders,
                                 "pressure_order": "not_applicable_exact_linear_pressure"})
        result["temporal_refinement"] = time_results
        extensions = [spatial[-1][:2]]
        extensions += [run(32, .02, L, [.5, 2], "c-outlet" + str(L)) for L in (6, 8)]
        result["outlet_runs"] = extensions
        result["outlet_changes"] = []
        for a, b in zip(extensions, extensions[1:]):
            for j, t in enumerate((.5, 2)):
                x, y = a[j], b[j]
                delta = {"flow": abs(y["flow"] / x["flow"] - 1),
                         "kinetic_per_length": abs((y["kinetic"] / y["length"]) /
                                                  (x["kinetic"] / x["length"]) - 1),
                         "dissipation_per_length": abs((y["dissipation"] / y["length"]) /
                                                      (x["dissipation"] / x["length"]) - 1),
                         "wall_per_length": max(abs((yb / y["length"]) / (xa / x["length"]) - 1)
                                                for xa, yb in zip(x["wall_force"], y["wall_force"]))}
                assert max(delta.values()) <= .01, delta
                result["outlet_changes"].append({"time": t, "from": x["length"],
                                                "to": y["length"], **delta})
                assert y["velocity_error"] <= .01 and y["energy_imbalance"] <= .02
        result["steady_recovery"] = {"at_12s_reference_remaining_flow_fraction":
                                     abs(spatial[-1][-1]["reference_flow"] / .008 - 1),
                                     "measured_flow_error_to_steady": abs(spatial[-1][-1]["flow"] / .008 - 1)}
        assert result["steady_recovery"]["measured_flow_error_to_steady"] < .01
        result["passed"] = True
    finally:
        path.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps({"startup_gate_passed": True, "evidence": str(path)}), flush=True)


if __name__ == "__main__":
    main()
