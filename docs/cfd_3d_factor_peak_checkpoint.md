# Allocator pressure control measured and rejected

2026-10-03. Persistent Main Edit reference evidence. The optional explicit
malloc_zone_pressure_relief(NULL,0) control preserves live sparse inputs, pressure
diagonals/couplings, operator action, RHS, exact factors and original full FE
reconstruction. Five independent support tests pass, including owned-factor
survival, unsupported API behavior, invalid input rejection and zero-report
semantics. The installed SDK declaration/documentation is frozen with an API
receipt. Process owned high-water is never reset and resource sampling/caps are
unchanged.

The matched exact L4 body6 base completes in 126.78 s at 1455.3 MiB owned peak,
versus 112.70 s / 1482.4 MiB for the shared-input predecessor: only 1.83% memory
reduction with 12.5% more total time. The explicit control including hashes/action/
RSS observations takes 0.317 s; the API call takes 0.0167 s. Timing variation and
factor work dominate the total difference; do not assign all added time to the
pressure call. Current RSS changes 800.27 to 322.67 MiB around the call while the
API reports zero released bytes. This is a process observation, not proof that
the API returned or reclaimed 477.6 MiB. Owned high-water stays 845.95 MiB across
the pressure observation and reaches 1455.3 MiB during factor setup.

Mesh/free-DOF/RHS identities match the predecessor. Field changes are 4.57e-15
velocity and 9.11e-13 Pa pressure; force/scalar changes are below 3.45e-14 relative.
The complete original FE residual, divergence, flux, energy, numerical/resource
and atomic-publication gates pass. Raw surface/reaction mismatch stays 2.171% and
physical qualification remains open. Prior independently observed L4/L8 fields,
stress diagnostics and all native workers/receipts/source remain preserved.

The small completed-run saving does not demonstrate robust headroom for the
23616-tet normal mesh, which previously stopped at 1827.8 MiB during factor setup.
No normal retry is made from this control and no field is fabricated. Reject the
pressure control as the next adopted default; existing shared-input/triangle
runners remain unchanged. The helper remains an explicit optional diagnostic.
All 1800-MiB/180-s/50000-tet/3000-iteration and numerical/physical gates remain.

Evidence lives under `build/c3d-factor-peak/`: the frozen SDK/API/support receipt,
one accepted matched solver receipt and `checkpoint-audit.json`. Runtime entrypoint
is `scripts/run_cfd_reference3d_factor_peak.py`; local Make support/audit targets
are `test-cfd-reference3d-factor-peak` and `audit-cfd-3d-factor-peak`. No native/shared
API, version, dependency, commit, package, install, release or deployment changes.
Generic FE/factor/local-job extraction stays deferred.

The next [exact symbolic cost/vector-graph investigation](cfd_3d_symbolic_goal.md)
records factor storage and numeric workspace before expensive factor creation,
then compares bounded exact scalar permutations and a complete vector block graph.
Symbolic diagnostics must not be presented as accepted numerical fields. A favorable
candidate still needs exact inverse, field/force/cost and original full FE proof
before retrying the stopped normal mesh. Stage 1 and the broad object/wind-tunnel
goal remain active; physical reference qualification precedes native traction,
authored stationary objects, transient/outlet/inertial wake and broader geometry.
