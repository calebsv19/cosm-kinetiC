# Next: memory-bounded velocity preconditioner for the accepted finer mesh

Quality-passed finer L4 needs287.527MiB less projected stage memory. Existing exact
Float coupled Cholesky factor alone1372.059MiB dominates; no numeric factor/solve
or force field was admitted. Preserve this receipt/geometry and all earlier failures.
Do not retry unchanged factor or raise1800MiB/180s/3000iter/50000tet.

Inspect current coupled-incomplete-factor and coarse-velocity history first. Select
a genuinely distinct velocity-PC candidate using bounded symbolic fill/storage,
full coupled physical velocity entries in its residual action, and symmetric/SPD
preconditioner controls needed by the balanced pressure Schur construction. A
compensated block incomplete Cholesky is a candidate, subject to implementation and
independent SPD/Float accuracy tests; any compensation belongs solely to PC, never
physical diagonal/equations. Do not assume incomplete factors are automatically
positive or adequate. Keep successful balanced-ten pressure complement10.

Predeclare memory, setup, residual-action and full numerical usefulness gates.
Test independent dense/SPD and anisotropic full FE controls before one numerical
control on the existing accepted cube. Keep flexible right iteration and both actual
Arnoldi bases, original true Float64 retained/reconstructed FE residuals, complete
pressure modes and original force/conservation/publication gates. Only a useful
bounded strict control permits a distinct preconditioner trial on saved finer mesh;
fresh stage admission remains mandatory. No full physical coefficient dropping or
force replacement. Seek an actual finer-mesh field before claiming force convergence.

Separate optional reference proof from general/native/installed integration. Broader
user goal remains active; this checkpoint is progress, not physical completion.
