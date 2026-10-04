# Pre-3D 2D resolution completion audit

The original five-step implementation is complete in Main Edit source. This is
readiness for a bounded 3D implementation, not evidence that 3D is implemented
or that arbitrary CFD is physically validated. No commit or Desktop refresh is
part of this completion.

## Requirement-by-requirement evidence

| Original requirement | Implementation and inspected evidence | Outcome |
|---|---|---|
| Locate force error and establish fair cost | Independent reference sampled through reconstruction; corner/surface bands; X-only/Y-only comparisons; separate pressure/viscous outlet extension; optimized serial cost and phase profiling | Complete for the stated 2D reference cases |
| Efficient solver scaling | Cached pressure and velocity multilevel preconditioners, optimized worker, removed diagnostic copies, grid-sized monitor storage, bounded numerical allocations and setup peaks; true residual preserved | Complete; measured scaling and memory limitations disclosed |
| Conservative fixed local refinement | Sparse balanced shared-face topology; volume transfer and affine/operator tests; evolving channel and two-interface transport; body in local fine mesh; uniform endpoint/spacing controls | Complete; fixed hierarchy, no dynamic remeshing |
| Verified implicit viscosity and synchronized timesteps | Implicit diffusion/mixed momentum, continuous nonuniform manufactured flow, spatial and temporal refinement, CFL and timestep cost diagnostics | Complete; constant timestep within each run |
| Physical and efficiency acceptance with agents | Independent separate component/total/energy limits, three-grid body refinement, matched-accuracy transient time/RSS, real scene/control/sample/assessment/comparison/artifact workflow | Complete within the declared physical scope |

The original plan did not require a uniform obstacle mesh to pass before moving
on. Its uniform controls are retained as failed physical evidence, while the
matched-accuracy cost requirement is demonstrated on the known-answer transient.
This distinction preserves the original gates without inventing a local-mesh
speedup or requiring an unbounded uniform-grid campaign.

## Final numerical results

The fixed confined steady Stokes rectangle uses domain 4x2x.5 m, body
[1.5,.75,2.5,1.25], density 1 kg/m3, dynamic viscosity .1 Pa s, mean inlet
.002 m/s and the documented natural vector-Laplacian traction outlet.

| Fluid leaves | Pressure error | Viscous error | Total error | Physical energy imbalance |
|---:|---:|---:|---:|---:|
| 7296 | 3.739% | .209% | 2.353% | 2.306% |
| 18240 | 1.755% | .970% | .685% | .657% |
| 58560 | .979% | .982% | .209% | .194% |

The coarse case correctly fails; both finer cases pass the separate unchanged
2% limits. Last-pair changes are pressure .784%, viscous .012%, total .477%.
Viscous reference error is not monotonic; no Richardson uncertainty estimate is
claimed. The final report requires explicit domain/spacing and finite true
linear residual evidence. Outlet-distance sensitivity is separately below 1%
for each force component; it does not qualify transient backflow.

Current regression checks include shared-flux cancellation, balanced topology,
transfer, affine diffusion, conservative disturbance crossing, evolving channel,
exact Couette dissipation, evolving-body momentum, multigrid algebra, memory
admission and failure cleanup, invalid reference evidence, and real agent runs.
The finest evolving channel has about .276% velocity and .279% pressure error.
Manufactured spatial ratios are 3.35/3.88; temporal ratios are approximately
4.07/4.03/4.02. The evolving body's closed momentum residual is 7.65e-12 N.

## Efficiency and usable quality choices

- Sharp rectangle: 18240 local leaves pass in about 10 seconds in the recorded
  serial cost screen. Uniform 122880 leaves take 77 seconds and fail separate
  components. This is cost/accuracy dominance, not a matched-passing speedup.
- Smooth transient: uniform 2048 and local 6656 leaves both meet identical
  velocity/pressure L2 limits of 1e-4 with measured divergence <1e-8. Three-repeat
  medians are 1.365 versus 22.455 seconds and 17.65 versus 69.62 MB process RSS.
  Coarser controls fail. The smooth channel therefore defaults to uniform cells.
- Multilevel preconditioning improved the earlier matched discrete case 4.27x.
  Periodic true-residual candidate checks subsequently reduce the measured
  manufactured transient median from 7.413 to 5.554 seconds without changing
  acceptance tolerance. These are distinct benchmark scopes, not multiplied
  into a claimed overall speedup.
- The qualifying body runs under a 192 MiB numerical allocation cap, including
  setup peaks. Numerical requested bytes are distinct from total process RSS.
- Implicit viscosity avoids the explicit small-cell diffusion restriction;
  temporal refinement remains necessary. The demonstrated smooth case reaches
  a spatial-error floor, so smaller dt can waste substantial work.

## Agent surface

`incompressible_refined2d_v1` is available through the existing local scene and
session service. Agents can author fixed regions and SI fluids, choose base
resolution and memory/cell budgets, control background runs, sample physical
pressure without advancing, compare refinements, inspect separate reference
acceptance and retrieve digest-bound leaf/face fields. Mesh levels, actual
minimum spacing, CFL/observed dt bound, iterations, numerical memory, CPU phases
and publication/export wall time are exposed. Low resolution is allowed and
reported honestly; completion never substitutes for physical acceptance.

## Evidence and limits

Machine-readable evidence inventory and source hashes:
`build/s4-resolution/completion-audit.json`. Detailed development history and
commands: [resolution checkpoint](cfd_pre3d_resolution_goal.md). Agent contract:
[agent session](agent_session.md).

This is stationary aligned-rectangle/laminar channel verification, not arbitrary
STL, moving obstacles, turbulence, liquid free surfaces or general transient
force certification. The refined energy observer does not fabricate a transient
energy derivative. The current numerical allocator cap excludes JSON and total
process overhead. Controls are serviced at solver-step boundaries. Dynamic AMR,
subcycling and GPU execution were not required and are not introduced.

The next 3D work should start with a bounded channel and a manufactured/transient
case varying in all three directions, retaining these agent and acceptance
contracts. An extrusion of the 2D solution is only a regression control.
