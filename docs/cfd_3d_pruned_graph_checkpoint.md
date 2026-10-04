# Compensated graph controls: numerically verified, not useful for adoption

Six independent coupled-SPD/Float/coordinate/physical-input/lifecycle controls
pass. Strength survey and six complete scientific controls establish the actual
storage/convergence tradeoff. Adopt neither compensated graph approximation nor
zero-pruning representation. Larger matched L8 numerical launch is withheld
because no useful pruning candidate exists. Preserve the previous same-surface
L4 force result (raw/reaction1.770%, still above1%) and matched L8 resource stop.
Stage1/native physical certification and the broad goal remain open. No original
scientific source/equations/pressure modes/target/resource gates or native source
are changed; no commit, package or install.

## Complete physical input and optional approximation

The original complete Float64 velocity/coupling/pressure blocks and original load
remain authoritative. Only the optional Float velocity preconditioner is pruned.
For each omitted3x3 upper block E, add ||E||_F I to both adjacent diagonal blocks.
The exact-arithmetic difference [[wI,-E],[-E^T,wI]] is PSD since w>=||E||_2.
Independent coupled matrices verify coefficients, symmetry, positivity/dominance,
coordinates, Float factor solve and cleanup. Float rounding is not an exact SPD
or forward-error certificate; nonfinite/underflow/nonpositive/factor failures
reject explicitly. Every physical coefficient/pressure mode remains in fullFE
quadrature and the actual residual. Complete V/Z/work, factor/scratch/current/
reserve admission,1800MiB/180s/3000iterations/50000tet and full1e-10 remain.

## Survey and measured controls

Accepted normal matrix has53000velocity nodes,1708108upper blocks,1655108
non-diagonal blocks and no completely zero off-diagonal block. Normalized strength
1/50/99-percentiles are0.00177188752/0.09102879104/0.69219180712. Threshold1e-3
omits7216off-diagonal blocks (~0.436%);1e-2 would omit124448 (~7.519%). The survey
builds the original operator and no symbolic/numeric factor or field. It is not a
force qualification or full physical pressure-spectrum certificate.

| Case | Iterations | Wall s | Owned peak MiB | Factor MiB | Full original residual | Outcome |
|---|---:|---:|---:|---:|---:|---|
|L4-body2-original-pruned0|120|12.928550|394.218750|89.681343|2.39615843226e-11|pass|
|L4-body2-original-pruned1e3|296|12.480581|454.328125|89.075516|5.08266701433e-11|pass|
|L4-body2-original-pruned1e2|3000|55.452015|422.046875|85.257744|1.03763728027e-10|target/iteration failure|
|L4-body6-normal-pruned0|120|68.375546|1158.390625|637.572285|7.27933250575e-11|pass|
|L4-body6-normal-pruned1e3|280|83.036404|1210.796875|623.855236|6.98247540752e-11|pass|
|L4-body6-normal-complete-fresh|120|68.339460|1157.687500|637.572285|6.3027184111e-11|pass|

Original1e-3 saves0.676%factor storage but raises iterations120→296 and peak
memory15.25%; its small wall difference is not sufficient usefulness. Original
1e-2 saves4.93%factor storage but reaches3000iterations, full1.03764e-10 and
~4.29xcontrol cost; momentum1.03748e-10 dominates continuity1.79e-12. Requested
1e-10 is missed. No numerical field is published and no normal/L8 extension of
this failed threshold is attempted.

Normal1e-3 passes full1e-10 but raises iterations120→280, time21.44% and owned
peak4.52%, with only2.15%factor saving. It preserves physical force components to
relative~8e-10 on this same mesh; its raw/reaction mismatch remains2.108%, not a
physical gate pass. Fewer graph entries do not recover the larger force test.

The initial zero-pruning normal peak~1158MiB appears better than an older1289MiB
receipt. A fresh unchanged complete Float backend is1157.6875MiB /68.33946s,
versuszero-pruning1158.390625MiB /68.37555s. The apparent improvement disappears
under matched control: cost ratio1.000528, peak ratio1.000607. Rounded Float value
hashes are identical. Preserve historical receipts; infer no universal memory
benefit from allocator/run variation. Do not adopt zero pruning.

## Pressure readback and limits of residual authority

Accepted physical fields agree closely in velocity (relative~6.05e-10original,
~9.29e-10normal) and integrated forces. Pressure coefficient relative differences
are2.84e-7original and3.63e-7normal; they are explicitly retained rather than
claimed below an unsupported1e-8 coefficient threshold. Independent pressure-mass
norm differences are1.88e-8 /5.52e-8 relative (absolute1.05e-9 /3.07e-9 in the
physical mass norm). Global physical pressure-mean differences are~1.63e-13 /
1.44e-12Pa; maximum retained macro coefficient-mean differences4.08e-9 /
1.68e-8Pa. A1e-10 residual is not a pressure forward-error bound. No full
DG/global pressure null-space certificate is asserted. The fresh complete versus
zero-pruning pair has pressure-mass relative difference1.62e-10. Input receipts,
snapshots and source hashes are rechecked after the bounded read-only readback.

## Evidence and next bounded experiment

- Six support controls: `build/c3d-pruned-graph/support-test-receipt.json`.
- Original operator distribution: `build/c3d-pruned-graph/strength-runs/`.
- Immutable full solve/failed-target receipts: `build/c3d-pruned-graph/runs/`.
- Fresh previous-backend control: `build/c3d-pruned-graph/control-runs/`.
- Actual cost/force/field decisions: `build/c3d-pruned-graph/comparisons.json`.
- Distinct coefficient/mass/global pressure observations: `build/c3d-pruned-graph/pressure-readback.json`.
- Once-only closure: `build/c3d-pruned-graph/checkpoint-audit.json`.

Next preserve the complete velocity factor and calibrate shorter flexible restart
lengths with the matching full basis reservation, numerical target and resource
limits. Only useful completed controls may recover the exact matched L8 force test.
If short restarts stall, investigate a separately declared global pressure coarse
correction rather than weakening equations or increasing caps. See
[next bounded storage/convergence gate](cfd_3d_preconditioner_correction_goal.md).
