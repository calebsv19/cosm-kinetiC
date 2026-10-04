"""Growth-owned surface-source v1 staging contract. No consumer/injection code."""
import base64
import hashlib
import json
import math
import re
import struct
from pathlib import Path

FRAME = "growth_sim_fire_surface_source/v1"
BUNDLE = "growth_sim_fire_source_bundle/v1"
LIMIT = 64 * 1024 * 1024
HEADER = struct.Struct("<8s4I4d16f")
FIELDS = ("fuel", "fuel_type", "burn", "heat", "smoke", "moisture", "ash", "wind_x", "wind_y")
RULES = ("burn_rate", "heat_per_burn", "heat_spread", "heat_diffusion", "heat_cooling", "ignition_threshold", "ignition_gain", "burn_cooling", "smoke_per_fuel", "ash_per_fuel", "moisture_cooling", "fuel_reactivity", "upward_bias", "turbulence", "smoke_transport", "smoke_cooling")
CALIBRATION = ("fuel_kg_per_unit_m2", "energy_j_per_fuel_kg", "smoke_kg_per_fuel_kg", "energy_transfer_fraction", "smoke_transfer_fraction")
QUANTITIES = ("fuel_consumed_kg", "energy_generated_j", "energy_transferred_j", "energy_untransferred_j", "smoke_generated_kg", "smoke_transferred_kg", "smoke_untransferred_kg")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def keys(value, expected):
    require(type(value) is dict and set(value) == set(expected), f"unexpected keys; expected {tuple(expected)}")


def number(value, minimum=None, maximum=None):
    require(type(value) in (int, float), "numeric value required; booleans forbidden")
    try:
        finite = math.isfinite(value)
    except OverflowError:
        finite = False
    require(finite, "finite value required")
    require(minimum is None or value >= minimum, "value below minimum")
    require(maximum is None or value <= maximum, "value above maximum")
    return value


def integer(value, minimum, maximum):
    require(type(value) is int and minimum <= value <= maximum, "integer out of range")
    return value


def identifier(value):
    require(type(value) is str and re.fullmatch(r"[A-Za-z0-9_-]{1,80}", value), "invalid identifier")
    return value


def sha(value):
    return hashlib.sha256(value).hexdigest()


def canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"), ensure_ascii=True, allow_nan=False).encode()


def _digest_form(value):
    # Every supported JSON number is exactly represented as a finite binary64.
    # Hash tagged big-endian bits, so C/Python decimal formatting does not define
    # interchange identity. Schema validation excludes objects in numeric slots.
    if type(value) in (int, float):
        number(value)
        require(type(value) is not int or int(float(value)) == value, "integer not exactly representable as binary64")
        return {"$f64be": struct.pack(">d", float(value)).hex()}
    if type(value) is dict:
        return {k: _digest_form(v) for k, v in value.items()}
    if type(value) is list:
        return [_digest_form(v) for v in value]
    return value


def digest(value):
    return sha(canonical(_digest_form({k: v for k, v in value.items() if k != "digest"})))


def sealed(value):
    return {**value, "digest": digest(value)}


def strict_load(path):
    p = Path(path)
    require(p.stat().st_size <= LIMIT, "artifact exceeds 64 MiB")
    def pairs(items):
        out = {}
        for key, value in items:
            require(key not in out, "duplicate JSON key")
            out[key] = value
        return out
    def constant(value):
        raise ValueError(f"nonfinite JSON constant: {value}")
    with p.open("rb") as stream:
        raw = stream.read(LIMIT+1)
    require(len(raw) <= LIMIT, "artifact exceeds 64 MiB")
    try:
        return json.loads(raw, object_pairs_hook=pairs, parse_constant=constant)
    except RecursionError as error:
        raise ValueError("JSON nesting exceeds parser limit") from error


def config_valid(c):
    keys(c, ("run_id", "branch_id", "surface_id", "width", "height", "seed", "cell_size_m", "dt_s", "origin_m", "calibration"))
    for k in ("run_id", "branch_id", "surface_id"):
        identifier(c[k])
    integer(c["width"], 2, 256); integer(c["height"], 2, 256); integer(c["seed"], 0, 2**32-1)
    number(c["cell_size_m"], 1e-6, 1e6); number(c["dt_s"], 1e-6, 3600)
    require(type(c["origin_m"]) is list and len(c["origin_m"]) == 3, "origin must have 3 coordinates")
    for x in c["origin_m"]:
        number(x, -1e9, 1e9)
    keys(c["calibration"], ("id", *CALIBRATION))
    identifier(c["calibration"]["id"])
    for k in CALIBRATION:
        number(c["calibration"][k], 0, 1 if k.endswith("fraction") else 1e12)
    return c


def state_decode(raw):
    require(type(raw) is bytes and HEADER.size <= len(raw) <= LIMIT, "invalid native state size")
    h = HEADER.unpack_from(raw)
    require(h[0] == b"GFSRC001", "native state version")
    w, y, seed, tick = h[1:5]
    integer(w, 2, 256); integer(y, 2, 256)
    n = w*y
    require(len(raw) == HEADER.size+9*n*4, "native state length")
    for x in h[5:]:
        number(x)
    require(h[5] > 0 and h[6] > 0, "native space/time")
    unit_interval_rules = {"burn_rate", "heat_diffusion", "heat_cooling", "burn_cooling", "smoke_transport", "smoke_cooling"}
    for name, value in zip(RULES, h[9:]):
        number(value, 0, 1 if name in unit_interval_rules else None)
    fields = {}
    for k, name in enumerate(FIELDS):
        row = list(struct.unpack_from(f"<{n}f", raw, HEADER.size+4*n*k))
        for x in row:
            number(x, 0 if k < 7 else None)
            if name in ("fuel_type", "burn", "moisture"):
                number(x, 0, 1)
            if k >= 7:
                require(x == 0, "v1 offline producer supports zero uniform wind only")
        fields[name] = row
    return {"width": w, "height": y, "seed": seed, "tick": tick, "cell_size_m": h[5], "dt_s": h[6], "origin_xy": list(h[7:9]), "rules": dict(zip(RULES, h[9:])), "fields": fields}


def areas(c):
    # Node-centered dual areas clipped to [0,(w-1)h] x [0,(h-1)h].
    h2 = c["cell_size_m"]**2
    return [h2 * (0.5 if x in (0, c["width"]-1) else 1) * (0.5 if y in (0, c["height"]-1) else 1)
            for y in range(c["height"]) for x in range(c["width"])]


def hash_valid(value):
    require(type(value) is str and re.fullmatch(r"[0-9a-f]{64}", value), "invalid SHA-256")


def frame_valid(f):
    keys(f, ("schema", "event", "producer", "config", "interval", "surface", "cells", "totals", "diagnostics", "digest"))
    require(f["schema"] == FRAME and f["digest"] == digest(f), "frame schema/digest")
    c = config_valid(f["config"]); n = c["width"]*c["height"]
    keys(f["producer"], ("worker_sha256", "adapter_sha256"))
    for value in f["producer"].values(): hash_valid(value)
    keys(f["event"], ("run_id", "branch_id", "sequence"))
    require(f["event"]["run_id"] == c["run_id"] and f["event"]["branch_id"] == c["branch_id"], "event lineage")
    integer(f["event"]["sequence"], 0, 2**32-1)
    i = f["interval"]; keys(i, ("start_tick", "end_tick", "start_s", "end_s", "allocation"))
    integer(i["start_tick"], 0, 2**32-1); integer(i["end_tick"], i["start_tick"]+1, min(2**32-1, i["start_tick"]+256))
    require(i["allocation"] == "uniform_rate_over_half_open_interval", "time allocation policy")
    number(i["start_s"], 0); number(i["end_s"], 0)
    require(i["start_s"] == i["start_tick"]*c["dt_s"] and i["end_s"] == i["end_tick"]*c["dt_s"] and i["end_s"] > i["start_s"], "interval clock")
    s = f["surface"]; keys(s, ("frame", "origin_m", "basis_u", "basis_v", "normal", "sampling", "order", "area_m2", "mask"))
    for key in ("origin_m", "basis_u", "basis_v", "normal"):
        require(type(s[key]) is list and len(s[key]) == 3, "surface vector dimensions")
        for value in s[key]: number(value)
    require(type(s["area_m2"]) is list, "surface areas must be an array")
    for value in s["area_m2"]: number(value, 0)
    require(s["frame"] == "right_handed_z_up_meters" and s["origin_m"] == c["origin_m"] and s["basis_u"] == [1,0,0] and s["basis_v"] == [0,1,0] and s["normal"] == [0,0,1], "unsupported surface transform")
    require(s["sampling"] == "node_centered_clipped_dual_area" and s["order"] == "row_major_x_fast", "sampling convention")
    require(s["area_m2"] == areas(c) and s["mask"] == [1]*n and all(type(x) is int for x in s["mask"]), "surface areas/mask")
    keys(f["cells"], QUANTITIES); keys(f["totals"], QUANTITIES)
    for k in QUANTITIES:
        row = f["cells"][k]; require(type(row) is list and len(row) == n, "quantity dimensions")
        for x in row: number(x, 0)
        total = number(f["totals"][k], 0)
        require(math.isclose(total, math.fsum(row), rel_tol=1e-12, abs_tol=1e-12), "quantity total")
    cal = c["calibration"]
    def close(a,b): require(math.isclose(a,b,rel_tol=1e-12,abs_tol=1e-12), "cell budget/calibration")
    for j in range(n):
        q = {k:f["cells"][k][j] for k in QUANTITIES}
        close(q["energy_generated_j"], q["fuel_consumed_kg"]*cal["energy_j_per_fuel_kg"])
        close(q["energy_transferred_j"], q["energy_generated_j"]*cal["energy_transfer_fraction"])
        close(q["energy_generated_j"], q["energy_transferred_j"]+q["energy_untransferred_j"])
        close(q["smoke_generated_kg"], q["fuel_consumed_kg"]*cal["smoke_kg_per_fuel_kg"])
        close(q["smoke_transferred_kg"], q["smoke_generated_kg"]*cal["smoke_transfer_fraction"])
        close(q["smoke_generated_kg"], q["smoke_transferred_kg"]+q["smoke_untransferred_kg"])
    keys(f["diagnostics"], ("legacy_heat_generated_units", "legacy_smoke_generated_units", "physical_status"))
    for k in ("legacy_heat_generated_units", "legacy_smoke_generated_units"): number(f["diagnostics"][k], 0)
    require(f["diagnostics"]["physical_status"] == "authored_calibration_not_combustion_qualification", "physical status")
    return f


def bundle_valid(b):
    keys(b, ("schema", "operation", "checkpoint", "frame", "digest"))
    require(b["schema"] == BUNDLE and b["digest"] == digest(b), "bundle schema/digest")
    op = b["operation"]; keys(op, ("kind", "request_id", "parent_digest", "ticks"))
    require(op["kind"] in ("init", "advance", "fork"), "operation kind"); identifier(op["request_id"])
    integer(op["ticks"], 0, 256)
    if op["parent_digest"] is not None: hash_valid(op["parent_digest"])
    cp = b["checkpoint"]; keys(cp, ("config", "next_sequence", "producer", "native_state_b64", "native_state_sha256", "last_event_digest"))
    c = config_valid(cp["config"]); integer(cp["next_sequence"], 0, 2**32-1)
    keys(cp["producer"], ("worker_sha256", "adapter_sha256"))
    for value in cp["producer"].values(): hash_valid(value)
    require(type(cp["native_state_b64"]) is str, "native state base64 required")
    try: raw = base64.b64decode(cp["native_state_b64"], validate=True)
    except (ValueError, TypeError) as e: raise ValueError("invalid native state encoding") from e
    require(sha(raw) == cp["native_state_sha256"], "native state digest")
    state = state_decode(raw)
    require(all(state[k] == c[k] for k in ("width", "height", "seed", "cell_size_m", "dt_s")) and state["origin_xy"] == c["origin_m"][:2], "checkpoint/native space disagreement")
    if b["frame"] is not None:
        f = frame_valid(b["frame"])
        require(op["kind"] == "advance" and op["parent_digest"] is not None and op["ticks"] > 0, "frame operation")
        require(f["config"] == c and f["producer"] == cp["producer"] and cp["next_sequence"] == f["event"]["sequence"]+1 and cp["last_event_digest"] == f["digest"] and state["tick"] == f["interval"]["end_tick"] and op["ticks"] == f["interval"]["end_tick"]-f["interval"]["start_tick"], "frame/checkpoint continuation disagreement")
    else:
        require(op["kind"] != "advance" and op["ticks"] == 0 and cp["next_sequence"] == 0 and cp["last_event_digest"] is None, "initial/branch checkpoint cursor")
        require((op["kind"] == "init" and op["parent_digest"] is None and state["tick"] == 0) or (op["kind"] == "fork" and op["parent_digest"] is not None), "initial/branch lineage")
    return b
