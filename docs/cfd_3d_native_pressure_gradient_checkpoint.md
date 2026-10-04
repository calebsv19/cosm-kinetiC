# Native pressure-gradient known-answer baseline

2026-10-04, PhysicsSim Main Edit. The separate C11 probe includes unchanged native
mixed-obstacle implementation only to exercise its existing pressure transpose.
No solver or default changes. Actual cube grids32x16x16,64x32x32 and16x8x8 retain
native adjoint/SPD controls. Each run evaluates all20 cubic pressure monomials from
exact physical cell averages against independently integrated pressure gradients on
staggered dual control volumes, including all components and interior fluid faces.

Degree<=2 errors stay below1.5e-14. For the pure cubic in the differentiated axis,
the measured gradient discrepancy is h^2/4:0.015625,0.00390625,0.0009765625 as h
halves from0.25 to0.125 to0.0625. The predicted signed error matches within3.8e-14.
Thus this pressure action is second order on the cubic physical-average control.
This is a truncation diagnostic, not a decomposition or full error norm of the
solved cube field, and it does not assign the archived force error to this term.

A constant pressure gives exactly zero interior pressure action and the appropriate
opposite open-end traction loads. Open ends therefore do not have a null global
constant-pressure mode; no gauge projection is added on that assumption.
Address/undefined sanitizer runs atn8/n16 pass. Allocation-owned peaks are0.824,
7.046 and58.355MiB, below the512MiB native probe budget. All source/header inputs,
compiler identity, binaries and outputs are sealed in
`build/c3d-native-pressure-gradient/checkpoint-audit.json`.

Next use this baseline to assess a consistent pressure-gradient/divergence pair
and boundary treatment if the operator is changed. Preserve adjoint conservation,
physical pressure loads, complete momentum/continuity residuals and force/energy
qualification; increasing reconstruction order alone is insufficient. No new
native PDE solve or native physical certification is claimed here.
