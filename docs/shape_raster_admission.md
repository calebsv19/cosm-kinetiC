# Native shape raster admission

Main Edit now preflights the app-local shape_import_bounds/rasterize boundary
before drawing or clearing the caller mask. Positive grid products are capped at
67,108,864 cells, matching the CLI mask cap. The native API still requires the
caller to supply storage for an admitted grid; it has no capacity parameter.
Nonfinite options refuse, while existing finite nonpositive/default option
fallbacks remain supported for native callers. CLI domains remain stricter.

The source preflight bounds paths to 10,000 and conservative flattened-point
estimates to 1,048,576, validates segment kinds and used coordinates, and protects
shared curve sampling from overflowing its pre-clamp integer conversion. A
conservative control-polygon/float-roundoff bound handles curves far from origin;
coordinate limits leave room for the shared float length arithmetic. These are
local consumer guards, not changes or qualification of every shared flattener
customer. The shape asset conversion still uses its own unguarded geometry path.
Logical point estimates do not establish a hard total allocation or I/O quota.

Before drawing, every transformed segment admits finite coordinates with INT_MAX/8
headroom and bounded stroke radius. A whole-pass 67,108,864-work budget charges
segment interpolation square visits (including off-grid work) and polygon edge
tests using overflow-safe division checks. This prevents coordinate subtraction,
float-to-integer and stroke-loop hazards and refuses excessive aggregate work.
All checks finish before mask clearing; refusal leaves the caller bytes intact.
Ordinary draw math is preserved. The admitted count budget is not a wall-clock
or aggregate workflow/storage guarantee. Concurrent mutation of borrowed native
source memory is outside this trusted-local API contract.

Final validation: 15 native methods with address, undefined and float-cast
sanitizers pass, plus seven CLI argument and three shape Make/link methods
(25 distinct methods). Cases cover extreme scale/stroke/rotation, nonfinite
options/source, cubic overflow and large-offset roundoff, mask admission, line,
polygon and aggregate work, path/point inventory bounds and normal/fallback use.
Both CLI consumers build in a fresh shape-raster-admission-20261007 profile.
Five ordinary line/curve/transform cases match unchanged retained baseline
binaries byte for byte. Matching repeat preserves 12 object/binary/manifest
outputs, and read-only clean preview admits 22 files. No cleanup applied.
A separate terminal non-sanitized old-source probe returns 4 and reports
unexpected successful raster for a NaN source point. The repaired test refuses
without mutation. That is the behavioral before-code claim used here.

Two earlier sanitizer attempts remain held, not passed or terminal. Old-source
cubic child 10253 under waiter 10227 (exec 9077), and initial polygon fixture child
11915 under waiter 11883 (exec 90220), were observed in macOS ps state UE after
subprocess timeout. TERM/KILL requests did not yield observed exit/reaping.
The initial polygon fixture was wrong: its closed single segment flattens to two
points, so polygon work was not charged; it then supplied insufficient mask storage
for an admitted grid. That caller error was corrected to an actual three-point
flattened path. Distinct corrected/final harnesses passed and terminated. The
original sessions were not restarted or represented as passing baseline gates.
Their original logs/control state remain outside this sealed qualified packet;
held-attempts.json records the observation snapshot, not a termination guarantee.
Do not overwrite/restart them solely because another observation expires.

Remaining work includes these OS/process holds, source file/JSON admission and
argument bounds, asset flattener consumers, direct output path/publication and
PGM failure exit status, complete worker/I/O/workflow limits and retirement.
This is source/API qualification, not installed GUI, platform, physical CFD,
canonical adoption or complete lifecycle acceptance. No shared source/API edit,
commit, cleanup, install or release occurred. Independent newer-backup coverage
is unchanged; this packet is outside the frozen seventy-packet batch.
The complete TL01-TL13 goal remains active and incomplete.

Evidence: data/experiments/lifecycle-validation/20261007-shape-raster-admission.
