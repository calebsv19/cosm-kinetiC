# Execute coupled float32 preconditioner / float64 flexible iteration

Follow cfd_3d_mixed_precision_goal.md. Use fixed restart60 flexible right-preconditioned
Arnoldi, two-pass modified Gram-Schmidt and separately stored preconditioned basis.
Approximate upper triangular mixed preconditioner uses float velocity factor and
negative exact macro-constant pressure mass inverse; original mixed operator is
unchanged float64. Require true retained residual checks at least every10iterations
and at each restart/termination, then independent fullFE authority after restoration.
Projected Arnoldi residual alone cannot publish a field or imply full-target success.

Reserve both flexible basis arrays and small dense/work arrays in live numerical
admission on top of original currentRSS+Float factor/scratch+32MiB. Record actual
basis allocations separately. All original equations/modes/gates/limits stay fixed.
Eleven focused support controls cover Float owner/input/accuracy/fullFE/partial100
cleanup, fitting/rejected symbolic-only admission and flexible indefinite/varying/nonfinite/rank-deficient/cap behavior.
Original L4 first; no larger extension unless it meets requested full1e-10 and
useful measured cost. Freeze immutable source/supervisor/compiler/SDK receipts.


## Declared follow-up after first base resource rejection

The first base chunk128 attempt is terminal at180.12s: metadata restored38.99s,
fullFE phase102.92s, then diagnostics still incomplete at cap; no field/result.
Retain it unchanged. Test the existing supported chunk512 option for complete
FE/physical batches, unchanged quadrature/formulas/target/caps. Match an exact
float64 catalog chunk512 control for cost attribution; do not compare different
chunks as precision-only cost proof. Same-geometry field/force equivalence against
immutable accepted base anchors remains required. This is a distinct batch-cost
experiment, not a same-parameter retry or resource/tolerance relaxation.
