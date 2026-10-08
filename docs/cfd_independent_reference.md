# Independent confined-flow reference

Current continuation: [component-force and obstacle-energy assessment](cfd_component_energy.md)
and [automated run acceptance](cfd_run_acceptance.md) supersede earlier statements
that masked energy is unsupported or that separate force errors are only 3–5%.
The component 2% gate remains open; 3D work has not started.

The verification-only reference uses P2 vector velocity / P1 pressure finite
elements for steady Stokes flow, separate from PhysicsSim's staggered grid and
projection. Its formulation follows the documented scikit-fem Stokes examples:
https://scikit-fem.readthedocs.io/en/latest/listofexamples.html
and the inlet/outlet example at
https://github.com/kinnala/scikit-fem/blob/12.0.2/docs/examples/ex24.py.

Geometry: 4 x 2 m fluid rectangle minus [1.5,2.5] x [.75,1.25] m obstacle;
extrusion width .5 m. Prescribed inlet is a parabola of mean .002 m/s, all solid
boundaries are no-slip, dynamic viscosity is .1 Pa s. FEM uses a natural zero
vector-Laplacian traction outlet. MAC uses pressure-outlet momentum evolution.
Their formal boundary equivalence is not assumed; a doubled-domain check bounds
outlet sensitivity in this case. Density 1 kg/m³ gives MAC body-height Re=.01;
halving inlet speed checks proximity to the creeping-flow limit.

The FEM reference first reproduces exact Poiseuille velocity and pressure. Its
obstacle result reports variational reaction and directly integrated physical
pressure/shear separately, free-equation residual and inlet/outlet flux. Mesh
refinement 16/32/64/128 resolves the same polygon without stair-step movement.
Total reaction converges faster than the individual traction components.

The MAC harness records one-second maximum velocity changes and force histories,
requiring two consecutive windows below 1e-6 of inlet speed after at least 5 s.
Force observations use the reusable open-model API, with independent surrounding
momentum quadrature and retained time derivative. Quadratic pressure and cubic
wall-velocity reconstruction are calibrated against supplied exact fields. No
periodic flow evolution is used by this observation adapter.

## Reproduce

The reference environment is local verification tooling, not a runtime or public
first-start dependency. The recorded run used Python 3.14.6. From Main Edit:

```sh
python3 -m venv data/tools/cfd-reference-venv
data/tools/cfd-reference-venv/bin/python -m pip install -r scripts/requirements-cfd-reference.txt
data/tools/cfd-reference-venv/bin/python scripts/cfd_fem_reference.py --output build/s3-independent-reference/stokes.json
data/tools/cfd-reference-venv/bin/python scripts/cfd_fem_reference.py --grids 128 --output build/s3-independent-reference/stokes-128.json
data/tools/cfd-reference-venv/bin/python scripts/cfd_fem_reference.py --grids 64 --length 8 --output build/s3-independent-reference/stokes-long.json
make test-cfd-open2d-force-check test-cfd-open2d-obstacle
make test-cfd-open2d-reference > build/s3-independent-reference/corrected-mac.log
build/cfd_open2d_reference_test 64 .001 > build/s3-independent-reference/half-re.log
build/cfd_open2d_reference_test 32 .002 8 > build/s3-independent-reference/long-mac.log
python3 scripts/assess_cfd_reference.py build/s3-independent-reference
```

The assessor requires exact reference calibration, refined reaction/surface
agreement, steady MAC windows, decreasing MAC reference error, mass/divergence,
low-Re sensitivity and domain sensitivity. Finest MAC surface and CV total forces
must each lie within 2% of the finest reference. Last FEM total-drag refinement
must change less than .5%; domain sensitivity must remain below .1%.

The current bounded screen passes: reference .005973307 N, surface .005890113 N,
CV .005906724 N. Individual pressure and viscous forces are not qualified to 2%:
their differences from current reference components are about 4.85% and 3.45%.
This numerical reference does not certify other shapes, higher Re, moving bodies,
3D flow, turbulence, experimental drag, or masked energy accounting.
