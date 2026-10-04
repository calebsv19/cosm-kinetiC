# Fused original inner recurrence: equivalent, cost gate rejected

Four support controls pass for original/encoded actions, actual anisotropic FE,
fixed pressure equivalence, alias/permutation/nonfinite/curvature/lifetime guards
and source/work bounds. The initial accumulation-buffer bug and strict legacy
BLAS declaration failure were corrected before freezing; diagnostics are retained.
SDK-recommended modern ILP64 compile flags preserve warnings-as-errors and the
byte-exact original C prefix. Seven scratch plus one output vectors remain within
the unchanged distributed-work reservation. No local graph/factor/pressure change.

The prescribed original4992tet paired benchmark completes16.9217s, sampled425.406MiB.
Three alternating20-load batches give median native/original time ratio.9767765,
only2.32% faster, missing the prospectively frozen5% cost gate. Maximum relative
complete-action difference6.0713e-13 passes1e-10; minimum positive work.0531513.
Original physical inputs/Float factor hashes and all completed factor owners are
preserved/retired. No full field is launched, no larger/finer/native adoption.

Next distinct cost candidate: keep the same recurrence and graph, but implement
its local block triangular solves with explicit3x3 register operations. The original
kernel may reload aliased source components inside the nested off-block loops.
Independent local-factor inverse/action comparison, exact physical bits, buffer
bounds and the same prospective paired/full cost and physical gates remain required.
This is an execution-cost hypothesis, not a change to equations or PC depth.
Broader goal remains active.
