# Local velocity improvement: small field useful, matched cube still blocked

This batch adds24passing support controls, six sealed diagnostic/field checkpoints
and one independently accepted small-cube field. Packed local triangular sweeps
make the existing low-memory reference recipe useful on the original4992tet cube.
The exact28416tet matched L4 trial fails both complete-runtime and factor/work
savings gates. No finer43008tet L4 field or resumed finer force comparison is
claimed. Original physical equations, residual/pressure modes, resource caps and
raw force/energy gaps remain authoritative; native/default/package state unchanged.

| Candidate | Measurement | Decision |
| --- | --- | --- |
| Original-path importance-ranked fill | Same graph cap; compensation11.0%lower, fixed pressure errors improve<1%. | Diagnostic improvement gate fails; no full trial. |
| Factor-informed streamed fill | Compensation38.4%lower; CG8 column errors5.8–8.4%better; pressure errors3.7–3.9%better. | Prospective10%pressure improvement gate fails; no full trial. |
| One native original inner8 call | Same action within6.1e-13; complete action2.32%faster. | Prospective5%cost gate fails. |
| Packed original block sweeps | Complete action9.07%faster; full small field113iterations/22.1806s/fullFE8.9834e-12. | Small whole<=23.5018s gate passes; exact matched trial earned. |
| Exact matched packed recipe | Solve completes163.121s; supervisor hits180.163s during independent full FE verification. Complete factor/work savings280.930MiB. | No accepted field; whole<=106.642085s and>=300MiB savings both fail. |
| Exact matched timing diagnostic |46.6364s; local inner61.2%, coarse corrections28.7%, outer physical actions9.1%, other1.0%. | Measures next optimization target; no flow qualification. |

The useful small field has original physical matrix/RHS/free-DOF/Float identity
and separate pressure force/raw symmetric viscous force/reaction/inlet pressure/
physical dissipation agreement within2.82e-10 relative to qualified control.
All original strict full residual, flux/divergence, energy, resource, atomic field
publication and completed-owner retirement gates pass. Owned peak438.094MiB exceeds
the complete-factor baseline359.906MiB; no whole-memory benefit is inferred.

The exact matched velocity local and coarse factors together occupy498235168bytes;
including80385536bytes of declared distributed work gives578620704bytes. Compared
with873198472bytes, savings294577768bytes=280.930MiB,19.069MiB short of300MiB.
Factor-only savings do pass300MiB, but do not satisfy the declared complete-work
gate. An early progress statement counted factors too soon; complete readback and
sealed checkpoint correct it. Numeric memory admission and both measured memory
caps pass: owned1351.141MiB and sampled1234.422MiB. Runtime is the larger gap.

At100iterations matched retained momentum1.065086e-6 dominates continuity2.384e-9;
at200, momentum2.042891e-11 dominates continuity4.104e-14. Completed buffers retire
before reconstruction, but full-FE verification does not finish within180s. No
accepted result JSON or field snapshot is produced. Retained residuals cannot
substitute for independent reconstructed full equations or force acceptance.

The separate timing control runs seven declared loads twice, preserving unchanged
original-rhs and deterministic responses and captured unique-owner retirement.
It includes the original full Arnoldi V/Z, pressure and velocity reservations plus
additional diagnostic work in every fresh symbolic/numeric admission. Its1541.547
MiB owned peak stays below1800MiB. The61.2%local cost includes physical velocity
products and packed triangular sweeps; those subcosts are not yet separated.
Coarse total includes its Float solves and Double corrections, so nested timings
must not be added again. Synthetic load mixtures are not stalled-vector or complete
spectral certificates. Earlier sampled58-direction pressure/mesh diagnostics remain
valid with their limitations; more pressure directions previously worsened flow.

## Next bounded implementation path

1. Split the measured dominant local inner cost into original physical matrix
   actions and triangular sweeps on this exact matched matrix. Measure whole
   actions with deterministic original inputs and fixed work bounds, not isolated
   microbenchmark speed alone. Keep the sealed180s failure intact.
2. Build one distinct local improvement based on that split: mathematically
   identical packed physical-action/workspace kernels if execution dominates,
   or a safely stronger PSD omitted-fill model if excessive compensation dominates.
   Prove physical bit/action identity, positive work, coordinate/buffer/lifetime
   guards and the full concurrent scratch bound. Do not merely increase fill or
   inner iterations, or shrink reservations without proving live storage.
3. A candidate must earn a strict useful small field at fullFE1e-10/retained1e-11
   and whole<=23.5018023327s, then an exact matched field at whole<=106.642085s and
   >=300MiB complete factor/work savings. Both fresh1800MiB admissions and actual
   peaks remain required, along with original force/scalar equivalence and atomic
   publication. Freeze candidate gates before measurement; preserve rejections.
4. Only useful matched evidence admits saved quality-passed43008tet finerL4.
   Resume separate pressure/viscous/reaction convergence, flux/divergence and
   inlet-pressure/dissipation comparison; then the49920tet pairedL8 domain trial
   if L4 remains useful. Raw1%force and3%energy gates stay unchanged.

Existing qualified matched raw surface/reaction mismatch remains1.7702%; held-first-
strip sensitivity worsens it to1.8339%. Existing L8 inlet-pressure/dissipation domain
changes remain25.61%/25.47%. Those are physical failures distinct from tiny linear
residuals. The broad CFD goal stays active; no native/default qualification, package,
installed, release or physical-accuracy completion claim is made.

## Evidence and documentation scope

Main Edit HEAD~1..HEAD e623d10..4e322da covers earlier session/UI work; it does not
prove this uncommitted numerical batch. Live worktree inspection, frozen new source,
support logs and immutable receipts govern these six checkpoints. All protected
native hashes and older sealed sources/artifacts are preserved. Only existing
current_truth.md and README.md are edited; future_intent.md has no candidate-adoption
claim to change. Outside-writer private history stays untouched. Bounded read-only
physics_sim MemDB retrieval supplied history context; no memory write is authorized.
C builds and focused support/diagnostic/full-field checks already ran. Documentation
closeout verifies touched links, source/artifact/native hashes and preexisting drift,
without rerunning numerical experiments. The first timing auditor guard included
its own source before a premeasurement bound-method fix; failed auditor and correction
receipt are retained, and a separate readback proves that exact transition.

- [Importance-ranked fill rejection](cfd_3d_priority_fill1_checkpoint.md)
- [Factor-informed fill rejection](cfd_3d_seed_fill1_checkpoint.md)
- [Native inner-action result](cfd_3d_native_inner8_checkpoint.md)
- [Useful packed small field](cfd_3d_packed_inner8_checkpoint.md)
- [Exact matched failure and complete memory accounting](cfd_3d_packed_matched_checkpoint.md)
- [Exact matched action timing](cfd_3d_packed_matched_profile_checkpoint.md)
