# Single-window prescribed-heater room (CP0)

Development-only,2026-11-02. This first compartment-flow benchmark uses a mild
prescribed heater and passive smoke tracer. It is not fuel combustion, a sealed
room expansion solver, calibrated soot, radiation or wall heat-loss prediction.

`rig_geometry.schema="physics_sim_single_window_room/v1"` selects an independently
checked1m³room with0.0625mwalls and ceiling and a solid floor boundary.
`window_profile="small"` gives0.125m², or`"large"`0.25m², in the east wall.
Both spanz0.375–0.875m. No other boundary opening or internal partition exists.
The native mask uses ordinary no-slip blocked-face/pressure/scalar ownership.
Every fluid component reaches the declared window. Native and Python mask/boxes
are separate implementations; existing partition rig behavior is retained.

CP0 prescribes20Wheat and1e-6kg/spassive smoke overx[.25,.5),y[.375,.625),
z[0,.125). This is a fixed physical support on16³/32³grids. It stays within the
existing10%temperature-contrast admission. An explicit zero-strength momentum
driver preserves native work receipts but adds no momentum. The room worker
refuses unknown window profiles; Python additionally refuses nonzero wind.

`coupling.room_heater` owns prescribed-source support, compressed lossless full
state publication, parent/hash/binding checks and dark VF3D scene export without
manufacturing a Fire bundle or joint-coupling receipt. Head publication is atomic;
source/worker/configuration identity is fixed, and existing state paths refuse
replacement. Native restore, conservation/pressure/CFL checks remain authority.
Each0.1ssnapshot preserves fields and checkpoint; rendering keeps source states
unchanged. Display-only west/front/ceiling cutaways and8×optical extinction improve
review visibility; they do not alter physical walls or simulated smoke mass.

Diagnostics report floor-to-ceiling temperature/smoke profiles, centroid height,
upper-quarter tracer fraction, peak temperature, height-resolved inward/outward
window volumes, cumulative native boundary heat/tracer discharge and stored/input
budgets. The upper-quarter fraction is not a calibrated layer-interface estimate.
Near-zero net volume flux is expected for an incompressible room with one opening;
separate incoming/outgoing rates are essential. Pressure is relative to ambient
hydrostatic reference; this does not establish thermal-expansion pressurization.

Local strict geometry/ordinary-weather tests passed. Isolated PC strict and
ASan/UBSan weather, integration, broad-gust and room tests passed(8runs).
Cold-rest and4sprescribed-heating/no-buoyancy/buoyant controls pass with native
reopen: centroid0.0625mwithout versus0.163041mwithbuoyancy; natural window
in/out5.72856e-5m³/s. Full-grid prefix/render/cost admission follows. Both12s
small/largewindow science/histories completed:~60.9%tracer in upperquarter,
centroid~0.67m; naturalwindowin/out0.585/1.272L/swith no forcing. Almost
alltracer remains inside; substantial smokeventing is not demonstrated. Original
video render timed out at108smallframes; render-only recovery is separate. See CodeWork private compartment-fire
roadmap and`_private_workspace_artifacts/fire-fluid-room-cp0-20261102`.

## Render supervision

The active room comparison now uses finite-frame, activity-monitored supervision,
not an aggregate elapsed-time cutoff. Each image and native-source proof is
persisted before staging retirement. Resume verifies saved PNG/checkpoint hashes
and reuses completed frames. An inactivity watchdog observes CPU, available I/O
and output changes; it stops only after600sof no observed activity. This cannot
diagnose a CPU-active algorithmic loop. Worker/physics/source states and memory/
storage guards are unchanged. The original aggregate timeout remains historical
evidence; its helper is not the current supervision entrypoint.
