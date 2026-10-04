# Same-factor packed block solves inside original CG8

The fused recurrence matched actions but failed its5% cost gate. This distinct
candidate retains that recurrence and changes only its native local triangular
sweep implementation: explicitly load three source components per off-diagonal
3x3 block, and keep each backward-sweep accumulator in local registers. Preserve
the original per-component multiplication/subtraction and triangular dependency
order. Factor coefficients, graph, permutation, physical action, fixed pressure
proxy, P3/Float3, eight-step cap, work reservations and all target/caps remain fixed.
Original legacy solve/complete-factor/operator C prefix stays byte exact. No extra
heap storage; existing seven scratch plus output vectors suffice. Both original
and packed local actions are independently compared before the full CG8 action.

Test reconstructed factor inverse on sparse graphs, original and encoded physical
CG8, actual anisotropic FE/fixed pressure, alias/nonfinite/curvature/permutation/
short-buffer guards, original inputs, all owners and exact-source transformations.
Then one original4992tet paired20-load/three-alternating-batch balanced-action
benchmark, using the already-declared reservation and same timing protocol. Require
local-factor relative action difference<=1e-12, complete-action difference<=1e-10,
positive work and median candidate/original complete-action time<=.95. No threshold,
benchmark size, recurrence, pressure or physical gate changes after measurement.

Only useful control permits one full small field at original FE1e-10/retained1e-11,
force/Pin/D equivalence1e-7 and whole<=23.5018023327s. Original flux/div1e-8/energy.03/
1800MiB/180s/3000/50000/atomic publication stay. Then exactmatched28416tetL4 mustpass
whole<=106.642085s and>=300MiB complete velocity factor/work saving vs873198472bytes.
Only useful matched permits saved quality-passed finerL4 and force convergence.
No native/default/package/deployment adoption or commit; goal remains active.
