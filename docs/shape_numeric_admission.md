# Shape CLI numeric admission

Main Edit shape_mask_tool and shape_asset_tool now check conversion errno and
finite float results. Mask grid conversion checks long-to-int bounds before
narrowing. Mask dimensions must be positive and their product at most 67,108,864
cells (64 MiB of one-byte mask storage). Division-based admission precedes input
loading and allocation, avoiding overflow in the product check. This cap bounds
the mask allocation only, not total geometry memory, output storage or run time.

Mask options require nonnegative margin, positive stroke/tolerance/scale and
normalized position in [0,1]. Asset flatten tolerance must be positive. Invalid
explicit options previously could silently fall back to rasterizer defaults;
they now fail at CLI admission, even where fit mode would ignore an option.
Rotation remains any finite representable float. Conversion range errors,
including strtof-reported underflow, are refused. Existing strto syntax and
locale behavior otherwise remain; this is not a new decimal-only grammar.

The retained old native binaries report 51 failed subcases and two FIFO timeout
errors across seven methods. Many failed subcases show wrong-stage rejection
(input loading), not successful malformed conversions. One real input probe
specifically demonstrates 4294967328 narrowing to 32 and successfully producing
a mask in the old binary. The repaired native binary rejects it before loading
input or creating output. Both FIFO timeouts killed/reaped the directly blocked
test process. No baseline binary was rebuilt; tests check both binary and retained
source digests remain unchanged.

Seven native methods now pass: grid representation/allocation admission, mask
float representation, mask option domains, asset tolerance, FIFO pre-open refusal,
valid conversions and actual overflow-to-valid-grid refusal. Invalid-input tests
also preserve a pre-existing output sentinel. The test requires both explicitly
selected binaries via PHYSICS_SIM_SHAPE_MASK_TEST_BIN and
PHYSICS_SIM_SHAPE_ASSET_TEST_BIN; missing selection fails instead of silently
skipping. Three existing actual-Make shape dependency/link methods also pass.
The fresh shape-numeric-admission-20261007 profile builds both tools. Normal
32x32 U-shape mask and default asset outputs match retained baseline outputs byte
for byte; the source remains unchanged. Matching repeat preserves 12 object,
binary and manifest outputs. Read-only clean preview admits 22 owned files.
No cleanup was applied.

Remaining shape boundaries include extreme finite transform/stroke arithmetic
and safe integer conversion inside rasterization, source geometry/schema and
flatten complexity, bounded argument lengths, full resource limits and direct
PGM/asset output path admission/publication. The PGM CLI still reports success
after output-write failure; that separate behavior needs repair with publication
migration. Finite CLI admission does not establish safety for all finite geometry
or transforms. No shared solver/API behavior, canonical source, installed package,
release, retirement eligibility or independent backup coverage changed.
This evidence is outside the frozen seventy-packet backup. The complete TL01-TL13
goal remains active and incomplete.

Evidence: data/experiments/lifecycle-validation/20261007-shape-numeric-admission.
