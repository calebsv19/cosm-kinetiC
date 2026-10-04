# Pressure and velocity preconditioner investigation: measured results

This batch added 20 passing support controls, five immutable diagnostic/field
checkpoints, and two independently verified full small-cube fields. It has not
qualified a cheaper preconditioner for the finer force-convergence mesh. The
accepted complete-factor recipe and existing force-sensitivity fields remain
available. The new work uses the same original filled-cube physical equations;
its numerical candidate controls use the original 4,992-tetrahedron L4 mesh.
No new full 33,216-tetrahedron L8 or 43,008-tetrahedron finer L4 field is claimed.

| Investigation | Measured result | Decision |
| --- | --- | --- |
| Pressure-column/58-direction diagnostic | Fixed pressure proxy: 19.26% Schur-column error and 17.51% projected coarse error against the complete Double velocity inverse. No scalar passed the prospective two-proxy improvement gate. | Keep scale 10. |
| Three pressure-proxy corrections | Schur/projected errors fall to 13.72%/12.00%; full field takes 114 iterations, 24.909 s, residual 9.008e-12. | Strict field passes; whole-runtime usefulness fails. |
| Six added sampled pressure directions | Sampled condition improves 31–40%; full field worsens to 135 iterations, 25.961 s, residual 8.984e-12. | Strict field passes; usefulness fails. |
| Adaptive cap-eight inner action | Factor calls fall 80 to 58, but per-column energy-error ratios reach 1.448, above prospective 1.25. | Reject before full flow. |
| Tighter safe discarded-fill bound | Same graph/storage; compensation falls only 0.00084%, pressure errors improve about 0.00025%. | Reject before full flow. |

The new full fields preserve the original physical matrix/RHS/free-DOF/Float
identities and agree with the qualified control's separate pressure force, raw
symmetric-viscous force, reaction, inlet pressure and physical dissipation within
1e-7. Original full-FE residual, flux, divergence, energy, atomic field and resource
checks remain in force. Diagnostic archives do not constitute flow fields. All
completed unique factor owners retire; both flexible Arnoldi bases and all declared
work are reserved in fresh admission checks before numeric allocations. Native
worker and previous native acceptance hashes are preserved.

## What the evidence supports

The earlier distributed P2 trial reached 3,000 iterations with retained momentum
about 9.966e-7 and continuity about 1.168e-9. The fixed-pressure CG8 control at
iteration 100 had momentum about 1.078e-10 and continuity about 3.014e-12. These
are retained-row diagnostics; independent reconstructed full FE is still the
acceptance authority. The remaining distributed-factor difficulty is predominantly
momentum/velocity accuracy, with an inaccurate fixed pressure response also measured.
The tested pressure corrections do not buy useful full-solve improvement.

The prescribed 58-direction Double Schur projection has sampled condition 4.675.
Its weakest sampled vector is poorly covered by ten polynomials (3.339% mass energy),
but has a large outside-span residual (0.647). Positive constant-direction energy
and finite sampled conditioning do not establish a complete nullspace, inf-sup,
full-spectrum or stalled-mode certificate. Enlarging that sampled space improved
its projected condition but worsened the actual flexible-GMRES solve.

The unchanged mesh has Jacobian condition median 19.368 and maximum 215.373;
near-body median 23.216 and maximum 82.325. These show anisotropy, not proof that
one mesh region causes the stall. Previously quality-passed finer geometry stays
withheld: its complete-factor admission remains 2,087.527 MiB, 287.527 MiB above
the unchanged 1,800 MiB limit. No force score or resource failure is replaced.

## Next bounded implementation and gates

1. Investigate importance-based selection of level-one local fill at the same
   per-column/global graph and storage caps. Keep every original edge and the
   same coordinate bijection; independently validate the candidate ordering,
   deterministic ties, Float input and PSD omitted-pair compensation. Measure
   complete symbolic scratch and reserve it before construction. Do not merely
   enlarge fill or increase the inner iteration cap.
2. Compare the new local factor on the same original small cube, including
   velocity/momentum response and fixed pressure Schur errors against the sealed
   Double reference. A diagnostic improvement is eligibility only; it must then
   reduce complete flow cost at the unchanged full residual target. Freeze gates
   before measurement and retain any rejection.
3. If the full small field is useful (whole <=23.5018023327 s), run the exact
   matched 28,416-tet L4 once. Require whole <=106.642085 s and >=300 MiB saving
   in complete velocity factor/work versus the 873198472-byte reference, plus
   all original residual, input, force, resource and publication checks.
4. Only a useful matched result permits the saved quality-passed finer L4 field.
   Resume separate pressure/viscous/reaction convergence, flux/divergence and
   inlet-pressure/dissipation/domain comparisons. Keep original raw 1% force
   acceptance and 3% physical energy gates; no corrected-force substitution.

Physical convergence remains open. The accepted matched L4 raw surface/reaction
mismatch is about 1.7702%; the existing held-first-strip field worsens it to
1.8339%. The accepted L8 raw pressure/dissipation domain changes remain about
25.61%/25.47%. These failures are distinct from tiny linear residuals. The broader
CFD goal remains active; no native/default/package/deployment adoption occurs.

## Documentation reconciliation scope

Closeout evidence uses Main Edit HEAD~1..HEAD (e623d10..4e322da), live worktree
status, current new source and immutable receipts, and a bounded read-only MemDB
query for physics_sim. The committed diff covers earlier session/UI work and does
not prove this uncommitted CFD batch. The five sealed checkpoint audits govern
this batch; preexisting source is preserved. Update only current_truth.md and
README.md, and add this new assessment. Existing future_intent.md has no candidate
adoption claim requiring revision. The outside-worktree private history remains
untouched under the bounded writer scope. No memory write is authorized. New C
builds and support/field checks already ran; docs verification adds link, source,
artifact and native-hash readback, without repeating numerical runs.

Detailed checkpoints:
- [Complete Double pressure-column diagnostics](cfd_3d_pressure_column_proxy_checkpoint.md)
- [Pressure-proxy refinement result](cfd_3d_pressure_proxy_refine3_checkpoint.md)
- [Sampled pressure coverage result](cfd_3d_pressure_coverage16_checkpoint.md)
- [Adaptive inner accuracy rejection](cfd_3d_adaptive_cg8_checkpoint.md)
- [Tighter discarded-fill result](cfd_3d_tight_fill_bound_checkpoint.md)
