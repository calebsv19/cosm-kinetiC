# Obstacle surface-pressure correction and CFD stopping point

## What changed

The old obstacle pressure diagnostic summed pressure at adjacent cell centers as
though those samples were on the solid face. That was the correct reaction for
the existing discrete projection operator, but not an accurate surface-pressure
integral. In the blocked channel it missed the pressure across two half-cell
distances and understated the known wall load.

`cfd_mac2d_surface_pressure` now reconstructs pressure onto each X-normal solid
face: `p_wall = 1.5*p_near - .5*p_next`. The nearest two fluid samples must exist;
a missing stencil returns failure rather than silently falling back. This is
exact for linear pressure and second-order for smooth pressure. It integrates
periodic pressure correction, not the affine mean-pressure term in the separate
unobstructed channel visualization convention.

The following quantities remain distinct:

- `obstacle_pressure_force_x_n`: last projection's discrete pressure reaction;
- `obstacle_mean_pressure_force_x_n`: its substep-integrated tick average;
- `obstacle_surface_pressure_force_x_n`: reconstructed surface integral at the
  last projection;
- `obstacle_mean_surface_pressure_force_x_n`: reconstructed tick average;
- `continuum_drive_force_x_n`: G times geometric fluid volume;
- `unresolved_drive_force_x_n`: geometric drive minus active-velocity-volume drive;
- `surface_pressure_valid`: reconstruction availability.

The existing momentum residual still uses actual discrete update reactions. We
have not added the same correction to both sides to manufacture a smaller
residual. Velocity evolution and the boundary-volume operator are unchanged.
The reconstructed force and discrete impulse need not agree for general flow;
this disagreement is now inspectable at the core API instead of being hidden.

## Known-answer verification

`make test-cfd-mac2d-boundary-pressure` runs linear-pressure exactness, pressure
constant-offset invariance, smooth-pressure refinement, and the solved full-height
baffle at 8/16/32/64 grids. Forward/reverse cases also vary density and timestep.

For G=.1 Pa/m, fluid length 1.5 m, height 1 m and width .5 m, the periodic
body-forced reference load is G*fluid_volume=.075 N. The reconstructed force
matches this within 1e-9 N at every tested grid (and -.075 N when reversed).

| Grid | Old discrete reaction N | Reconstructed surface N | Explicit unresolved drive N |
|---|---:|---:|---:|
| 8² | .0625 | .075 | .0125 |
| 16² | .06875 | .075 | .00625 |
| 32² | .071875 | .075 | .003125 |
| 64² | .0734375 | .075 | .0015625 |

An independent cosine-pressure surface integral has errors .0766407, .0175734,
.00404757 and .000960301 N across these grids: ratios about 4.2–4.4, consistent
with second-order reconstruction. This is a pressure-quadrature test, not a
convergence result for a complete obstacle flow. Baffle velocity and the original
momentum residual retain their existing limits. A linear-pressure fixture checks
exactness and gauge invariance without solving the same discrete equations.

Evidence: `build/s3-boundary-pressure-correction/`. Core channel, obstacle
projection, bounded obstacle-step and agent-channel regressions are run separately.
The reconstruction/baffle fixture is also checked under address/undefined-behavior
sanitizers. No desktop package, commit or public model promotion is performed.

## Where the CFD lane stands

The new CFD path is **2D incompressible, constant-density, laminar** with invariant
Z width. The older graphics-oriented 3D Wind path remains a separate solver and
has not inherited these proofs or been converted into verified 3D CFD.

Established numerically:

- SI density/viscosity/pressure channel baseline, wall shear and flux checks;
- matched staggered pressure projection and conservation diagnostics;
- known-answer nonuniform transient and transport-error isolation;
- a verification-only lower-diffusion transport candidate;
- core-only stationary rectangular obstacles with complete timesteps, no normal
  leakage, no-slip ghost treatment, discrete reactions and unforced decay tests;
- corrected surface-pressure measurement for the controlled baffle and smooth
  supplied-pressure cases.

Still unqualified: general body pressure/viscous drag accuracy, boundary/corner
volume weighting, open inlet/outlet behavior, curved geometry, arbitrary STL
bodies, moving objects, 3D CFD, turbulence, AMR and water/free-surface physics.

Existing unobstructed channel scene/session/MCP tools provide asynchronous
controls, snapshots, physical pressure/shear samples, flux/residual diagnostics
and result provenance. Rectangular obstacle setup and the new force reconstruction
are currently C-core/test APIs, not agent-authorable scenes. The desktop app is
not refreshed by this work. Avoid describing core verification as user-facing
availability.

## Bounded next work and useful stopping point

This is a verified research baseline for simple 2D problems, not yet a predictive
wind tunnel. The smallest useful continuation is to compare reconstructed
pressure plus independently reconstructed viscous traction against a surrounding
control-volume balance, repairing boundary/corner volume terms where necessary.
Use one fixed geometry and a known flat-wall/shear reference before more shapes.

For a useful wind-tunnel baseline, then implement actual inlet/outlet boundaries
and run a channel recovery, disturbance-exit, backflow and domain-length campaign.
Complete one matched low-Re 2D obstacle reference (including its confinement and
reference uncertainty) before a qualified drag coefficient. A 3D sphere reference
cannot certify a 2D block/cylinder.

A bounded CFD-lab agent surface can then expose one or two verified presets with
explicit method, geometry, resolution, physical parameters, time history,
conservation residuals, force components and qualification status. Keep momentum
residual, reconstructed-surface mismatch and reference error as separate fields.
Unqualified runs may be useful exploratory outputs, but must say so.

There is no need to pursue turbulence or AMR before returning effort to the
application shell, live inspection, preset authoring and existing 3D usability.
Those improvements can proceed independently while this numerical baseline stays
stable. General 3D CFD should be a later port with its own verification suite,
not an implicit promise attached to these 2D results.

Ownership remains app-local numerical policy, reusing existing session/scene
infrastructure; no shared module/API/version changes are needed.
