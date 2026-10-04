# Calibrated empty ducts isolate tunnel-length resistance

The four predeclared original P4/DG-P3 empty-duct fields pass strict reconstructed
full1e-10, retained1e-11, flux/divergence1e-8, energy.03 and unchanged
1800MiB/180s/3000iteration/50000tet caps. Five independent support tests pass.
This is reference-lane diagnostic progress; cube physical qualification remains open.

| Case | Tetrahedra | Iterations | Whole seconds | Owned MiB | Pressure error % | Dissipation error % | Pressure-field mass error % |
|---|---:|---:|---:|---:|---:|---:|---:|
| L4 n4 |3072|50|6.019|278.781|.0011074|.0013554|.0595003|
| L8 n4 |6144|64|10.386|421.391|.0011113|.0012353|.0298604|
| L4 n6 |10368|54|17.640|561.562|.0002200|.0002676|.0265198|
| L8 n6 |20736|64|41.697|1082.219|.0002205|.0002443|.0132892|

Every n6 error improves versus n4 and passes the separately declared.1% analytical
gate. Reconstructed full residuals range8.84e-13–6.81e-12. Natural inlet pressure,
zero outlet traction, no-slip side walls, mu.1Pa s and Q.008m3/s match the original
cube reference. Body force/traction/consistency lifts are explicitly not_applicable.
No empty-box body-force certificate is inferred. All complete DG pressure modes
remain; analytical pressure-field comparison includes amplitude and constant offset.
An independent constant-offset control fails the field gate even with exact Pin/D.

The existing rectangular-duct Fourier formula gives conductance.56230805982071m4.
Its512odd-term positive mathematical tail bound is1.910e-13m4. An independent
384x384double-sine sum, scale/swap checks and tail controls pass. Float64 roundoff
is explicitly not certified; this is bounded analytical calibration, not universal
forward-error proof. The auditor reconstructs the pressure basis from each saved
field and recomputes its mass-norm analytical comparison exactly.

## Diagnostic obstacle excess, with original gates preserved

Only after the n6 baseline fields passed, subtract their pressure/dissipation at
the same length from the accepted second-normal cube fields:

| Quantity | L4 raw cube | L4 calibrated empty | L4 diagnostic excess | L8 raw cube | L8 calibrated empty | L8 diagnostic excess |
|---|---:|---:|---:|---:|---:|---:|
| Pressure Pa |.02234746690|.005690843275|.01665662362|.02807089410|.01138168661|.01668920749|
| Dissipation W |.0001789791153|.00004552676785|.0001334523475|.0002245723301|.00009105351453|.0001335188156|

Diagnostic excess pressure changes approximately.196%, excess dissipation.050%.
These are additional diagnostic definitions. Raw cube total pressure/dissipation
still change25.611%/25.474%, and the original combined1% physical gate still fails.
Raw surface/reaction force mismatch remains1.770% at L4 and1.757% at L8. No force
component is substituted, original gate rewritten, or cube certification inferred.

The preceding sixteen signed-edge candidates all failed local quality before
factorization. Their rejections remain sealed. Follow a fresh quality-controlled
fixed-domain body-resolution investigation, then resource admission and actual
force comparison only for acceptable geometry. Default restart60, native solver,
public desktop package, source versions and native binaries are unchanged.

Evidence: build/c3d-empty-baseline/runs/98e8c604ec26920614e418fcb45f7728facd347d985f2cad73e2e2323cce2e42,
support-test-receipt.json, source-transform-control.json, comparisons.json and
checkpoint-audit.json. No commit, package or installation occurred. The broad
Stage1/native/general-object qualification remains active.
