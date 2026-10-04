# Matched normal-domain result: numerical acceptance, physical force gate open

The accepted count6 normal cube can now be tested in both 4m and 8m tunnels.
The new longer field passes complete numerical/publication gates and the requested
1e-10 independent full residual target. Stage 1 physical qualification remains open.

## Matched geometry and unchanged mathematical authority

L8 `held_l4` preserves the 23616-tet connectivity, Y/Z coordinates, cross section,
cube dimensions and inner X planes (translation +2m); only the two end slabs
lengthen. Q=.008 m3/s and mu=.1 Pa s match. P4/DG-P3, exact local condensation,
complete mixed operator, all pressure modes and reconstructed full FE residual
remain authoritative. The numerical probe and exact vector factor sources are
unchanged from the accepted catalog slice; new runners preserve independent frozen
sources/receipts. FE catalogs/metadata and full loads restore bitwise before full
FE validation. Stage diagnostic reports no inverse or field.

Five mesh/boundary/volume and fitting/over-budget admission controls passed. Prior
fields, observers, receipts, sealed audits and protected native workers are retained.
The original L4 field is the immutable comparison anchor, avoiding a redundant run.

## Completed numerical controls

| Case | Iterations | Whole run | Peak RSS | Full original FE residual |
| --- | ---: | ---: | ---: | ---: |
| L4 normal, accepted anchor | 170 | 97.77s | 1555.25 MiB | 4.8055e-11 |
| L8 held normal | 400 | 126.34s | 1560.38 MiB | 8.7403e-11 |

Both use chunk512 and meet requested 1e-10. L8 flux error 4.2813e-12,
max divergence 2.8468e-11 /s and energy imbalance 4.6569e-12 pass all numerical
gates. L8 momentum relative residual is 8.7363e-11, continuity 2.6355e-12.
The 400-iteration run converges; this residual split does not by itself prove a
specific Schur eigenmode stalls. No new complete global pressure spectrum is claimed.

Stage estimate 1736.011 MiB and repeated numerical live estimate 1772.137 MiB
include exact 1336344064-byte factor, 49771768-byte scratch and 32 MiB reserve.
Both admit allocation under unchanged 1800 MiB. Full run remains below 180s,
3000 iterations and 50000 tet. API-reported released bytes remain zero; sampled
current RSS and completed-run owned high-water measurements keep distinct authority.

## Force and physical gate

| Drag component | L4 force N | L8 force N | Relative change |
| --- | ---: | ---: | ---: |
| Pressure | .02480032134 | .02528463496 | 1.9529% |
| Raw symmetric viscous | .01417046279 | .01425305244 | .5828% |
| Reaction | .03980996486 | .04037455031 | 1.4182% |

Raw surface/reaction mismatch is 2.1080% at L4 and 2.0727% at L8. Pressure,
reaction and raw/reaction exceed the unchanged separate 1% physical gates. Inlet
pressure/dissipation change 26.46%/26.26%; these length-dependent scalars are
recorded but excluded from a flat domain force criterion. Neither field certifies
native/desktop CFD or physical force convergence.

## Signed stress and conditioning diagnosis

The read-only L8 observer passes in 69.17s / 470.09 MiB. Signed pressure/viscous
identities and weak reaction agreement hold to about 1e-14 N. Raw minus reaction
drag is -0.000839181 N at L4 and -0.000836863 N at L8. The local raw traction gap
therefore remains almost unchanged despite the measurable domain force shift.

For both .25m and .4m lifts, the .05-.10m centroid bucket contributes about
-0.00040 N to the signed total force gap, while other buckets contain substantial
cancellation. The closest <.025m bucket is small in the signed total. Bucket
rankings depend on the lift; they are not clipped spatial regions or proven error
bounds. Blind corner-only refinement or replacing raw traction with reaction
cannot close this evidence gap.

The 4800 end-slab tetrahedra have Jacobian condition max/median 215.37/44.66 at
L4 and 543.30/112.42 at L8. The held 18816 inner tetrahedra preserve max/median
69.78/15.91. All volumes/Jacobians remain valid. Global strong-equilibrium/jump
norms increase from .16594/.01988 to .40364/.04250; this does not contradict the
nearly unchanged signed local gap. End-cell elongation needs an independent
quality control before interpreting length sensitivity as a domain plateau.

## Recommended continuation and evidence

1. Screen a matched L8 end-slab subdivision with the same inner planes, body and
   flow. Check exact symbolic/live budget first. This separates elongated-cell
   discretization from domain-length sensitivity; reject allocation if over cap.
2. Add signed force-specific cell/face attribution around the .025-.10m edge
   neighborhoods and surrounding .10-.40m cells. Test a small conforming,
   quality-aware refinement; require actual component/scalar/raw gates and full
   numerical/resource/publication checks. Keep both lifts and cancellations.
3. If further mesh controls exceed the exact factor budget, use these admitted
   fields as anchors for a fixed SPD velocity/coarse or global pressure correction.
   Preserve all modes and complete full FE residual. Requested target and resource
   caps remain fixed; a fitting stage alone does not admit a field.
4. Native stationary-box qualification follows independent reference physical
   qualification, then authored boxes and transient/outlet/wake cases separately.

Goal/preconditions: `docs/cfd_3d_normal_domain_goal.md`.
Generated evidence: `build/c3d-normal-domain/checkpoint-audit.json`,
`support-test-receipt.json`, `force-localization.json`, frozen `runs/`,
`stage-runs/` and `observer-runs/`. Audit also verifies predecessor evidence and
native hashes. Native/shared APIs, versions, commits, packages and installed app
are unchanged by this slice. Broad goal and Stage 1 remain open.
