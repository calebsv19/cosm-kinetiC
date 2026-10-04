#!/usr/bin/env python3
"""Numerical checkpoint only: independent packed-field readback, not agent completion."""
import array
import hashlib
import json
import math
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/c3d-wall"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load(name):
    a = array.array("d")
    a.frombytes((OUT / name).read_bytes())
    assert all(math.isfinite(v) for v in a), name
    return a


def layout(f, n, periodic):
    nx, ny, nz = n
    shapes = [(nx if periodic else nx + 1, ny, nz), (nx, ny - 1, nz), (nx, ny, nz - 1)]
    sizes = [math.prod(s) for s in shapes]
    offsets = [0, sizes[0], sizes[0] + sizes[1]]
    m = sum(sizes)
    assert len(f) == m + nx * ny * nz
    def face(a, i, j, k):
        c = [i, j, k]
        if a:
            if c[a] == 0 or c[a] == n[a]:
                return 0.
            c[a] -= 1
        if periodic:
            c[0] %= nx
        assert all(0 <= c[b] < shapes[a][b] for b in range(3))
        return f[offsets[a] + (c[2] * shapes[a][1] + c[1]) * shapes[a][0] + c[0]]
    return shapes, offsets, m, face


def divergence(f, n, length, periodic):
    _, _, _, face = layout(f, n, periodic)
    h = [L / count for L, count in zip(length, n)]
    value = 0.
    for k in range(n[2]):
        for j in range(n[1]):
            for i in range(n[0]):
                c = [i, j, k]
                d = 0.
                for a in range(3):
                    hi = c.copy()
                    hi[a] += 1
                    d += (face(a, *hi) - face(a, *c)) / h[a]
                value = max(value, abs(d))
    assert value < 1e-8
    return value


def sinc(x):
    return math.sin(x) / x if abs(x) > 1e-12 else 1.


def manufactured(name, row, L, periodic):
    f = load(name)
    size = row["n"]
    n = [size, size, size] if periodic else [int(L * size / 2), size, size]
    length = [2, 2.5, 3] if periodic else [L, 2, 2]
    ref_length = length if periodic else [4, 2, 2]
    h = [a/b for a, b in zip(length, n)]
    shapes, offsets, m, _ = layout(f, n, periodic)
    error = norm = perr = pnorm = 0.
    amplitude = 1 + .2 * math.sin(2 * math.pi * row["time"])
    for a, shape in enumerate(shapes):
        for k in range(shape[2]):
            for j in range(shape[1]):
                for i in range(shape[0]):
                    c = [i, j, k]
                    if a:
                        c[a] += 1
                    base = []; derivative = []
                    for b in range(3):
                        x = (c[b] + (0 if a == b else .5)) * h[b]
                        wave = 2 * math.pi / ref_length[b]
                        avg = sinc(wave * (0 if a == b else h[b]) / 2)
                        base.append(math.sin(wave*x)*avg if b == 0 else .5-.5*math.cos(wave*x)*avg)
                        derivative.append(wave*math.cos(wave*x)*avg if b == 0 else .5*wave*math.sin(wave*x)*avg)
                    x, y, z = base; dx, dy, dz = derivative
                    exact = amplitude * [.013*x*dy*z-.007*x*y*dz,
                                         .011*x*y*dz-.013*dx*y*z,
                                         .007*dx*y*z-.011*x*dy*z][a]
                    value = f[offsets[a] + (k*shape[1]+j)*shape[0]+i]
                    weight = .5 if not periodic and a == 0 and i in (0, n[0]) else 1.
                    error += weight*(value-exact)**2
                    norm += weight*exact**2
    pamp = .01 * math.cos(2 * math.pi * row["time"] + .3)
    for k in range(n[2]):
        for j in range(n[1]):
            for i in range(n[0]):
                x, y, z = [(c+.5)*hh for c, hh in zip((i,j,k), h)]
                value = (math.cos(2*math.pi*x/ref_length[0]+.2) *
                         (math.sin(math.pi*y/ref_length[1]+.3)+.5*math.cos(2*math.pi*y/ref_length[1])) *
                         (math.cos(math.pi*z/ref_length[2]+.4)+.4*math.sin(2*math.pi*z/ref_length[2])))
                native = f[m+(k*n[1]+j)*n[0]+i]
                perr += (native-pamp*value)**2
                pnorm += .0001*value*value
    velocity = math.sqrt(error / norm)
    pressure = math.sqrt(perr / pnorm)
    assert abs(velocity-row["velocity_error"]) < 1e-10
    assert abs(pressure-row["pressure_error"]) < 1e-10
    return {"field": name, "sha256": sha(OUT/name), "velocity_error": velocity,
            "pressure_error_peak_normalized": pressure,
            "max_divergence_s_inv": divergence(f, n, length, periodic), "passed": True}


def startup(name, row):
    f = load(name)
    n = row["grid"]; L = row["length"]
    _, _, m, face = layout(f, n, False)
    h = [L/n[0], 2/n[1], 2/n[2]]
    mean = 4/12 * (1-192/math.pi**5 * sum(math.tanh((2*j+1)*math.pi/2)/(2*j+1)**5 for j in range(1024)))
    G = .1*.008/(4*mean)
    pressure_error = 0.
    for q, p in enumerate(f[m:]):
        exact = G*(L-(q % n[0]+.5)*h[0])
        pressure_error = max(pressure_error, abs(p-exact))
    assert pressure_error < 1e-10
    flux = [sum(face(0, i, j, k)*h[1]*h[2] for k in range(n[2]) for j in range(n[1]))
            for i in (0, n[0]//2, n[0])]
    assert max(abs(v-row["flow"]) for v in flux) < 1e-10
    flow_ref = .008
    for a in range(128):
        for b in range(128):
            ky = (2*a+1)*math.pi/2; kz = (2*b+1)*math.pi/2
            wave = ky*ky+kz*kz
            c = 16*G/(.1*math.pi**2*(2*a+1)*(2*b+1)*wave)
            flow_ref -= c*math.exp(-.1*wave*row["time"])*4/(ky*kz)
    assert abs(flow_ref-row["reference_flow"]) < 1e-10
    return {"field": name, "sha256": sha(OUT/name), "pressure_max_error_pa": pressure_error,
            "section_flux_m3_s": flux, "independent_reference_flow_m3_s": flow_ref,
            "max_divergence_s_inv": divergence(f, n, [L,2,2], False), "passed": True}


def main():
    ab = json.loads((OUT/"qualification-ab.json").read_text())
    phase = json.loads((OUT/"qualification-phase.json").read_text())
    start = json.loads((OUT/"qualification-startup.json").read_text())
    opened = json.loads((OUT/"qualification-open-transient.json").read_text())
    assert all(x["passed"] for x in (ab, phase, start, opened))
    assert start["binary_sha256"] == sha(OUT/"startup3d_test")
    assert opened["binary_sha256"] == sha(OUT/"open_transient3d_test")
    records = [manufactured(label+"-spatial32.bin", ab["deliveries"][label]["spatial"][-1], 2, True)
               for label in ("a", "b")]
    records.append(manufactured("b-phase32.bin", phase["final"], 2, True))
    for row, name in zip(opened["outlet_runs"], ("co-spatial32.bin", "co-outlet6.bin", "co-outlet8.bin")):
        records.append(manufactured(name, row, row["length"], False))
    for t, row in zip((.5,2,12), start["spatial"][-1]):
        records.append(startup("c-spatial32-t"+format(t,".6g")+".bin", row))
    previous = json.loads((ROOT/"build/c3d/completion-audit.json").read_text())
    preserved = {p: sha(ROOT/p) == previous["source_sha256"][p] for p in
                 ("src/app/cfd_cartesian3d.c", "src/app/cfd_duct3d.c", "src/app/cfd_periodic3d.c", "src/app/cfd_sparse_mg.c")}
    assert all(preserved.values())
    source = ["src/app/cfd_mixed3d.c", "src/app/cfd_wall3d.c", "src/app/cfd_wall3d_reference.c",
              "src/app/cfd_startup3d.c", "src/app/cfd_startup3d_reference.c",
              "include/app/cfd_mixed3d.h", "include/app/cfd_wall3d.h", "include/app/cfd_startup3d.h"]
    evidence = ["qualification-ab.json", "qualification-phase.json", "qualification-startup.json",
                "qualification-open-transient.json", "wall-final-contract.log", "new-sanitize.log",
                "budget.log", "reconstruction.jsonl", "preserved-regression.log", "preserved-source.json"]
    result = {"schema": "physics_sim_c3d_transient_numerical_checkpoint_v1",
              "numerical_matrix_passed": True, "goal_complete": False, "agent_integration_complete": False,
              "remaining": ["shared scene/session integration", "worker checkpoint control adapter",
                            "retained agent artifact and diagnostic proof", "final full requirement audit"],
              "binary_sha256": {p: sha(OUT/p) for p in ("wall3d_test","open_transient3d_test","startup3d_test")},
              "source_sha256": {p: sha(ROOT/p) for p in source},
              "evidence_sha256": {p: sha(OUT/p) for p in evidence},
              "field_readback": records, "preserved_baseline_sources": preserved,
              "commit": False, "package": False,
              "limitations": ["controlled laminar references only", "no arbitrary nonlinear outflow/backflow",
                              "no obstacles, moving bodies, turbulence or local 3D refinement",
                              "LeakSanitizer unavailable on macOS; live numerical bytes checked"]}
    (OUT/"numerical-checkpoint.json").write_text(json.dumps(result,indent=2)+"\n")
    print(json.dumps({"numerical_matrix_passed": True, "field_readbacks": len(records), "goal_complete": False}))


if __name__ == "__main__":
    main()
