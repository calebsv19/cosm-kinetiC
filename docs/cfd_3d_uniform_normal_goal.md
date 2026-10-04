# Uniform body-spacing and balanced-normal physical control

The constrained bisection optimization and six balanced-cosine normal controls
remain rejected by their unchanged intrinsic quality gates. Select a distinct
conforming family: uniform body-plane spacing with count6/count8, common first
normal distance1/16m in all axes and two X end layers. Physical cube/domain/flow/
BCs and original P4/DG-P3 forms, all pressure modes, original full residual and
resource/force gates remain unchanged. This changes the FE mesh, not equations.
Uniform body spacing is coarser at the very first edge interval than cosine
spacing and finer along the middle of edges; better shape is not proof of better
force accuracy. Retain that tradeoff explicitly.

Compare with the original corresponding cosine base geometry. Require positive
mapping, volume15m3, exact physical body/domain/boundary areas and mirror symmetry,
no false wall, global intrinsic worst shape and Jacobian maximum not worse, and
below.025m centroid-region worst/mean intrinsic shape improved. Centroid sets
change; this is not a same-parent or clipped-region error bound. Keep rejected
candidates and prior accepted fields. Verify original FE action/nonzero-load
reconstruction on the already declared bounded anisotropic control.

Stage-screen count8 first, keeping the unchanged Float coupled preconditioner,
restart60 flexible iteration, chunk512 diagnostics and all factor/scratch/current/
basis/work/reserve accounting. If count8 exceeds1800MiB, preserve the exact
stage and screen count6. Solve an admitted candidate under180s/3000iterations/
50000tet/full1e-10, with complete independent flux/divergence/energy/publication
checks. Compare all raw pressure/viscous/reaction/traction/scalar quantities to the
accepted cosine anchor; no physical adoption without actual measured improvement
and existing force requirements. If useful, inspect stress and matched L4/L8
controls; if inaccurate retain the field/failure and do not expand this family.
No native/API/version/package/install/commit change. The broad object/wind-tunnel
CFD goal and native physical qualification remain open.
