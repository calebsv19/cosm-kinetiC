# Controlled domain and mesh sensitivity after force testing resumed

2026-10-03. Persistent Main Edit, optional local reference path. Seven bounded
solver receipts and two independent stress observers separate tunnel length from
near-body spacing. Six focused geometry/action/resource tests pass. The exact
P4/DG-P3 equations, full reconstructed residuals, raw component loads, separate
1% raw/reaction criterion and original resource caps remain unchanged. Physical
qualification and the broader object/wind-tunnel goal remain open.

The old outer grading changed the first-normal spacing when extending L4 to L8.
A diagnostic `held_l4` mesh preserves all internal L4 X planes translated to the
L8 cube, all Y/Z planes, original connectivity and macro centers. The longer outer
cells remain conforming. Volume, actual boundary areas, mirror symmetry and
independent original quadrature action are verified. `original` remains the default;
held planes are an experimental comparison, not an adopted physical mesh.

| L8 measurement | Original spacing | Held L4 near planes |
| --- | ---: | ---: |
| Base to normal: pressure force | 2.589% | 0.158% |
| Base to normal: viscous force | 1.731% | 0.918% |
| Base to normal: reaction | 1.125% | 0.345% |
| Base to normal: inlet pressure / dissipation | 0.630% / 0.630% | 0.193% / 0.193% |
| Raw surface / reaction mismatch, normal mesh | 3.173% | 2.102% |

The matched held L4-to-L8 normal pair still changes body pressure force by 1.954%
and reaction by 1.341%, while viscous force changes 0.485%. Both domain sensitivity
and the raw/reaction criterion remain open. The original-spacing pressure comparison
of 0.010% alone was misleading because length and near-body mesh changed together.
Total inlet pressure and dissipation change approximately 26% with tunnel length;
these are length-dependent physical responses and are not required to remain flat
when changing the physical domain. Their within-L8 mesh changes remain reported
under the unchanged scalar refinement criterion.

The four accepted initial L8 controls have 10752/13824 tetrahedra, take 45–70 s,
and use 1404–1580 MiB observed RSS. They pass original full residual, divergence,
flux and energy gates. Normal held-mesh residual is 9.73e-11; independent stress
identities close to 2.12e-14 N. Its strong volume defect is 0.27592 and interior
stress-jump norm 0.03288. More than 99% of its h-squared volume indicator lies
in the outer centroid bucket. This weighted indicator guides refinement and is
not a certified force-error bound; long cells amplify the weighting.

Global outer-plane subdivision produced 16896 tetrahedra and a passing linear
residual of 7.21e-11, but failed the original memory gate. Owned peak RSS was
1969815552 bytes (1878.6 MiB), exceeding 1800 MiB despite periodic supervisor
samples peaking at 1708.3 MiB. No accepted field was published. Its force values
are not admitted accuracy evidence. The failure receipt remains intact.

The new domain probe checks owned peak RSS/time at assembly, factor setup, solve,
reconstruction and diagnostics boundaries. A unit proof verifies immediate rejection
after an observed phase overrun. An actual retry stopped in 10.57 s at 2008.3 MiB,
by the supervisor during factor setup; it does not establish the owned phase guard
as that retry's stop authority. An accepted normal control with the added checks
passes in 70.24 s / 1580.9 MiB and preserves mesh/operator/RHS identity, with maximum
velocity/pressure coefficient differences 4.14e-13 / 8.14e-11. Force and scalar
changes are below 2.5e-12 relative. Post-serialization acceptance remains atomic.
No equation, residual, conservation, force or resource threshold was relaxed.

`make test-cfd-reference3d-domain` runs four geometry/original-action and two phase
resource tests. The initial bitwise coordinate-translation test failed on 2.22e-16
roundoff; its log is retained and the test uses a 1e-14 absolute translation check.
`make audit-cfd-3d-domain` verifies frozen sources/binaries/build records, complete
numerical gates, field equivalence, observers and preserved predecessor evidence.
The durable report is `build/c3d-domain/checkpoint-audit.json`. Entry points are
`scripts/run_cfd_reference3d_domain.py` and the corresponding equilibrium runner.
These optional research commands do not change the public headless quickstart.

Next: localize the dominant outer stress defect and test a selective conforming
macro refinement, rather than adding a plane across the entire cross-section.
Prove geometry, original algebra and resource admission before comparing raw forces.
Then resolve body/corner stress and matched L4/L8 domain sensitivity. All separate
force/scalar and raw/reaction requirements must pass before native correction and
authored stationary-object qualification. Transient/outlet/inertial wake and curved,
moving, free-surface and atmosphere lanes remain subsequent requirements. Shared
FE/factor/job extraction is reuse-deferred. No native/shared API, package, install,
commit or deployment changed.
