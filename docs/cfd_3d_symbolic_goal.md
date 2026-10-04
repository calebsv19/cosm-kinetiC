# Exact symbolic factor cost and vector-graph ordering

Declared after the optional allocator pressure control preserves original inputs,
fields and forces but saves only 1.83% owned peak RSS and costs 12.5% more total
wall time on the matched 18816-tet body6 base. Reject it as the next adopted path;
the exact 23616-tet normal mesh still has no accepted field. Original resource and
numerical/physical gates remain unchanged. Stage 1 and the broad goal stay open.

First expose exact symbolic factor storage and numerical workspace requirements
before numerical factor creation. The installed Accelerate Sparse/Solve.h offers
explicit symbolic SparseFactor and public factorSize_Double/workspaceSize_Double
fields. Use a separate optional diagnostic C shim with owned symbolic lifetime,
status checks, validated input arrays, cleanup and frozen source/binary/SDK proof.
Symbolic success is only a graph/cost diagnostic: it is not a positive-definiteness
proof, converged field, force result or physical qualification.

Bounded graph candidates are the current component-major scalar METIS control,
an exact scalar permutation that interleaves the three velocity components at
each trace node, and a complete three-component block graph. Preserve all velocity
couplings and the original physical mixed operator, pressure terms, RHS and mesh.
Scalar permutation must be bijective; retain original coefficient values and
verify symmetric action after inverse permutation on anisotropic/refined controls.
The block graph joins a node pair whenever any component couples them; predicted
dense-block factor storage must be labeled honestly rather than treated as the
original scalar operator. No pruning, scaling, penalty or ignored rows/columns.

Prove symbolic lifetime/input preservation, block coverage and scalar action before
measuring the exact admitted base and stopped normal graph under the same
50000-tet/1800-MiB/180-s/3000-iteration envelope. Record original mesh/free/RHS
identity, structural hashes, factor/workspace predictions, phase costs and actual
RSS. Reject candidates whose predicted/live cost does not support real headroom;
do not spend large numerical runs merely to make another cap failure.

Only a measured favorable candidate may proceed to an explicit exact numerical
inverse with correct permutation/block conversion and owned input/factor/workspace
lifetimes. Prove dense and anisotropic/refined FE inverses, input preservation,
non-SPD rejection and cleanup. Compare accepted body6 fields/forces and completed
total cost, then retry the exact normal mesh only with demonstrated headroom.
Original full FE residual, conservation, raw traction/reaction, energy, resource
checks and atomic accepted-field publication remain final authority. If admitted,
continue separate refinement/domain/component/raw-reaction/stress physical gates.

No native/shared API, dependency, version, commit, package, install, release or
deployment change. Reuse existing FE/scene/observation semantics; generic FE/factor/
local-job extraction stays deferred. Native traction corrections, authored objects,
transient/outlet/inertial wake and more geometry follow reference qualification.
