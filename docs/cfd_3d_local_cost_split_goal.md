# Local action/sweep cost split on exact matched cube

Separate diagnostic entrypoint appends timing to the byte-exact sealed packed C
source. Existing entrypoints unchanged. Same physical/encoded coefficients, factor,
CG8 recurrence, coordinate map and caller-owned7scratch+1output remain. Three native
timers measure packed/permuted solves, physical products and total; two counters
verify step counts. Statistics buffer is caller-owned and disjoint from all inputs,
output and scratch. Reject nonfinite clock/statistics, aliases and original guards.

Exact matched28416tet uses the same seven loads/two repeats as prior profile,
bitwise original-rhs/response proof, fixed pressure10/P3/Float3, original residual
contract, both VZ and all original work reservations. Diagnostic adds1MiB for timer
metadata atop original8*8*n+2MiB reserve; no heap/field/increase in numerical caps.
One capped1800MiB/180s run; all captured owners retire. Timings do not earn a field
or relax matched106.642085s/300MiB usefulness. Choose next distinct kernel or PSD
local-factor candidate from dominant cost, then original small/matched full gates.
