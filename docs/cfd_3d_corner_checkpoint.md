# Signed force-gap targeting and finer-body resource checkpoint

2026-10-03. Persistent Main Edit, optional local reference. The preceding selective
outer solve was numerically accepted but worsened force/stress evidence. This
slice traces the signed force gap, rejects unsuitable corner geometry before PDE
solves, and attempts the existing better-shaped body6 family with the stronger
solver. It stops during factor setup at the unchanged memory cap. Physical cube
qualification and the full object/wind-tunnel goal remain open.

Signed component attribution reuses the immutable original quartic stress observer.
For each lift it sums pressure/viscous interior-jump minus volume-divergence terms
and independently verifies raw minus weak load and total raw minus reaction.
Component weak pressure/viscous splits depend on the lift; no individual split is
a physical force reference. Centroid edge-distance buckets are not geometrically
clipped bands. Their signed contributions guide the next test, not certification.

| Held L8 normal, 0.25-m lift | Signed X contribution |
| --- | ---: |
| Nearest centroid edge-distance bucket, below 0.025 m | -0.000987618 N |
| Sum of all centroid buckets | -0.000848945 N |
| Pressure raw minus weak component | -0.005671623 N |
| Viscous raw minus weak component | +0.004822678 N |

The near bucket is larger than the net gap because farther terms partly cancel
it. The same direction holds for the 0.4-m lift and matched L4/outer-selective
fields. The long-cell h-weighted indicator previously emphasized the outer domain;
this signed balance instead points to edge-near stress resolution. Both observations
are retained, with their different weighting and interpretation made explicit.
The derived report is `build/c3d-corner/force-attribution.json`, bound to all exact
observer/source/input receipts and independently rechecked by the audit.

Fixed-cost redistribution of the count4 interior nodes toward the cube corners
was tested geometrically at first edge distances .1/.08/.0625 m on L4/L8.
All six preserve topology and unknown count but increase corner maximum and mean
Jacobian condition numbers, so no PDE solve is run. Sixty-three nearest-edge
Y/Z-paired longest-edge controls and sixty-three multi-edge stellar controls also
fail prospective shape gates. A label/scale-invariant all-edge-length/volume
shape measure confirms that their affected worst cell shape deteriorates; this
is not solely a reference vertex numbering effect. Multi-edge stellar subdivision
splits each marked original edge across every incident tetrahedron with one shared
midpoint. It is geometrically conforming but still produces unsuitable transition
cells for this use. No rejected mesh is adopted or assessed as physically accurate.

Five focused tests pass: regular/anisotropic shape and all 24 vertex permutations
at three scales; stellar volume partition/shared-boundary conformity; actual
geometry rejection before algebra; independent original quartic FE action after
exact condensation on a stellar control; and signed pressure/viscous cancellation.
Full geometry surveys and failed prospective controls remain in `build/c3d-corner/`.

The existing six-interval tensor body family is a better candidate, without a new
mesh algorithm. The following nearest-edge statistics use tetrahedron centroids
below 0.025 m; they are geometry diagnostics, not force bounds.

| L4 geometry | Tetrahedra | Worst cell-shape score | Maximum Jacobian condition |
| --- | ---: | ---: | ---: |
| Count4 first-normal control | 13824 | 9.338 | 41.74 |
| Count6 base | 18816 | 6.077 | 29.65 |
| Count6 first-normal control | 23616 | 6.077 | 29.65 |

The smaller count6 base case is attempted with unchanged original P4/DG-P3
forms, exact condensation, Cholesky/METIS, zero reconstruction cache and original
residual/conservation/force/resource requirements. It completes assembly in
9.49 s with owned peak RSS 1484.8 MiB, local elimination residual 9.05e-13 and
Schur asymmetry 1.97e-14. The supervisor stops it during factor setup after
11.64 s at 2001.9 MiB observed RSS. No factor-ready/solve phase, accepted field,
raw force or physical result exists. The resource receipt and frozen sources,
C shim, binary/build identity and progress log are retained.

The current constructor allocates 798475776 bytes of all-entry COO scratch for
this base case, before CSR conversion, geometry, fields and factoring. Its finer
normal case would allocate 1002168576 bytes. Those are exact allocation-size
calculations from 103 retained variables per macro, not measured total RSS or
proof that removing the scratch alone will admit the solve. The failure establishes
a concrete memory target for backend work instead of just proposing more refinement.

`make test-cfd-reference3d-corner` runs the focused proofs.
`make audit-cfd-3d-corner` verifies signed attribution, full geometry survey evidence,
the terminal resource failure, frozen source/binary/build records and preserved
predecessor fields/workers. The result is `build/c3d-corner/checkpoint-audit.json`.
The attempted solver uses `scripts/run_cfd_reference3d_corner.py`; its numerical
probe remains the unchanged domain probe. This does not alter the supported public
headless quickstart. No native/shared API, dependency, version, commit, package,
install, release or deployment changed. Generic FE/factor/job reuse stays deferred.

The [next declared implementation slice](cfd_3d_bounded_assembly_goal.md) is bounded
sparse construction directly on free retained variables, followed if needed by an
exact symmetric single-triangle operator representation. Prove original action,
reconstruction and matched body4 full fields/forces first; measure total memory and
time, then retry this exact count6 base under the same caps. If admitted, compare
it with the matched count4 base and independently observe stress before expanding
to normal refinement or L8. All separate 1% force/scalar and raw/reaction gates
remain. Native correction/authored stationary objects and subsequent transient,
outlet, inertial wake and further geometry/material work remain open.
