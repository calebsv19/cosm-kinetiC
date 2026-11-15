# Room plus simulated exterior air (CP1 development)

Typed opt-in `physics_sim_exterior_room/v1` selects `window_profile=small|large`
and `exterior_profile=near|far`. Existing room/rig inputs remain separate.

Room: x[0,1), y[offset,offset+1), z[0,1), with0.0625m shell and solid floor.
Window on east wall: local y[.375,.625)small or[.25,.75)large, z[.375,.875).
Near cube1.5m/24³coarse or48³fine; far cube2m/32³coarse or64³fine.
`offset=(domain_length-1)/2`. Fine interior spacing stays0.03125m; coarse0.0625m.
The window is an internal opening. Ambient pressure boundaries are on the
outer x/y/top faces; no hidden room ports, no imposed wind, no ceiling opening.
No sealed-pressure, combustion or expansion claim; mild Boussinesq20W heater,
passive tracer1e-6kg/s, dt0.005s, accepted full state every0.1s.

Native mask is independently checked against a Python physical-box union;
existing pressure connectivity, solid exclusions, transport/conservation,
kinetic budget and native restart gates remain authoritative. Domain/tracer
inventories distinguish room, exterior air, and distant-boundary discharge.
Net room transfer is inferred from source minus room inventory, including
return flow; it is not a separately integrated one-way window scalar flux.

Presentation: physically closed walls with display-only front/ceiling cutaways;
oblique camera from east/front shows room interior and air outside window.
Same dark navy background, teal walls,60000m²/kg display extinction. Exterior
rim light helps reveal dilute discharged smoke; no field/density multiplication.
Two cases share camera/light/optics and remain vertically stacked native10fps.
Visual proof requires20/25/30s native frames from both cases to show interior
layer, window aperture, and visible exterior plume with headroom. Reject a
framing that hides exterior smoke behind wall geometry or crops its main body.

Qualification: strict native geometry/old-room/weather tests; PC ASan/UBSan;
cold-rest, no-buoyancy and heated4s controls, near/far coarse boundary readback;
fine0.4s clocks/resource measurements; native full histories and signed budgets.
Near/far4s comparison is a sensitivity observation, not domain convergence.
Long pair stops before full rendering at a mandatory controller visual-review
boundary. Full rendering uses per-frame durable journal, inactivity watchdog,
no aggregate render cutoff. Artifacts stay under the task-owned/home PC run.
No installed-package change, app refresh, release or latest-good promotion.

## Full30s computation admission and continuation

The ordinary1e9 cumulative scalar-work allowance cannot cover6000steps at64³
with two SSPRK2 stages (minimum3,145,728,000workunits). New opt-in
`rig_geometry.computation_profile="exterior-room30/v1"` selects a closed
64³/2m/dt0.005/MC-SSPRK2/no-wind horizon. Allowance is
`6000 * 64³ * 2 * 4096 = 12,884,901,888,000`, covering every scalar cycle
already accepted by the ordinary solver. The ordinary4096cycle cap, CFL,
pressure/divergence, contrast, memory and byte limits remain unchanged.
The native parser independently enforces profile, exact allowance and endstep6000.

The shared open-policy source is historically pinned. `exterior_work30.py`
loads a private instance of that exact validator, changing only its scalar-work
resource admission interval for the closed profile. Process-global ordinary
validation remains unchanged and rejects the enlarged allowance. All accepted
physical data and the cumulative counter remain intact. The profile's native
capture reuses the ordinary anchored cleanup/protocol implementation with a
600s inactivity check on native-group CPU/I/O/output, excluding anchor polling.
There is no aggregate native-call timeout; byte limits and owned cleanup remain.
This cannot diagnose a CPU-active algorithmic loop without native semantic progress.

Qualification reproduces the last accepted packet with original and successor
workers, checks complete state-data equality and exact reopen, tests active/idle
capture and immutable default capacity refusal. A successor handoff records
old/new worker/config/checkpoint identities and equal physical data/counters;
original runs, bindings and failed attempts remain immutable. Earlier states are
referenced from original roots under their original bindings. New history begins
with an explicit capacity-only anchor and continues the same physical clock.
The6000step/301state/300frame path is audited against10000native clocksteps,
262144cell/VF3D admission, actual request/output size,512MiB numerical memory,
6GiB hostprocess memory and actual/home capacity. Future genuine numerical,
byte, memory, disk, I/O and stall refusals remain hard failures with evidence.

