# Positive omitted-fill compensation in the fixed-pattern block factor

Predecessor7247f191fb37a1a504d20728efb8021e0bfa83ca26536fd681b63f6f6a097209 rejected
late-pivot-only IC0 before coarse reproduction. Do not retry it unchanged.
Distinct PC: every omitted off-diagonal Schur update U adds ||U||_F I to BOTH
remaining diagonal blocks. The pairwise difference [norm I,U^T;U,norm I] is PSD;
this controls fill omission before unstable pivots arise. All stored original
blocks remain in the predictor; physical Float64 matrix remains exactly unchanged.
Keep one PC-only Gershgorin fallback for unexpected roundoff; record its use and
all fill-compensation count/sum/max. Same fixed memory graph, Double arithmetic,
component mapping, physical C prefix, balanced quadratic velocity30 and pressure
complement10, flexible right iteration and both actual Arnoldi bases/work.

Independent gates include dense full-pattern exactness, sparse global-SPD fixture
factor-model minus rounded predictor PSD, bounded inverse growth (fixture inverse
norm <= 10 times dense physical inverse norm), balanced coarse reproduction,
anisotropic original FE/full RHS/pressure preservation, invalid/rank/lifecycle and
fresh memory admission. Factor report must print BEFORE coarse construction so
setup rejection retains numerical diagnostics. No solution-dependent correction.

Same predeclared caps1800MiB/180s/3000iter/50000tet/full1e-10/retained1e-11 and original
force/flux/divergence/energy/publication rules. Keep all prior failures and300MiB
large-control memory-saving requirement. One original4992tet small full control,
whole <= 2*11.750901166s, strict original full residual/field equivalence. Only a
useful small control allows one exact28416tet matched L4 control, whole <= 1.25*
85.313668042s plus original identity/forces/scalars. Only that allows one saved
43008tet finer L4 run; do not relax any gate or retry a failed unchanged case.
No native/default adoption or physical-accuracy claim. Persistent goal active.
