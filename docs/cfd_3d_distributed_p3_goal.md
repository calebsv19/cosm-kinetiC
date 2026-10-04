# Distributed macro P3 with fixed three-step physical residual corrections

Predecessor a1d24dfa96a6cee80bdd2ed60cdf1c0dd867c4d1d52cb0d9926825c854918c5b:
distributed P2/local fill1 reaches full9.966e-7 at3000/64.873s, rejects strict/cost.
Distinct next candidate uses original macro P3 sparse interpolation and bounded
local Galerkin, cheap controlled-fill local factor, Float coarse factor with
three fixed corrections against the original Double coarse operator. Local inverse
also has three fixed corrections against original Float64 physical velocity.
In exact arithmetic for SPD A,G, triple inverse3G-3GAG+GAGAG has SPD polynomial
T^2-3T+3I>=.75I in G^.5 coordinates. This is a model claim, not a spectrum or
forward-error certificate for quantized Float solves; flexible outer and complete
original FE residual remain authority. No changing physical matrices/quadrature,
loads, pressure modes, native/default, caps or force acceptance.

Sparse macro P3 vertex/edge/face interpolation already has exact sparse evaluation
left inverse and original essential projection. No global dense Nv-by-Nc or A@Z.
Local <=102x60 projection uses physical upper orientation and roundoff-only coarse
symmetry; batch64<=1830entries/macro, projected all-live hierarchy<=256MiB,
local temporary<=2MiB. Fresh interpolation reserve1024*nt+256*tet+64*nv+64MiB;
actualRSS+reserve+32MiB<=1800. Before each merge freshRSS+2*predictedCSR+32MiB
admission and every64macro original owned highwater/time check.

Separate coarse library is exact byte copy of encoded Float workspace C, scalar
block_size1 explicitly seven-argument ABI. Original Double Ac remains authority,
Float predictor input separately hashed, no shift. Coarse symbolic first; combined
fresh numeric admission BEFORE either numeric factor includes local72*pattern+1024,
Float coarse storage, max(32*nv,coarse numeric scratch), measured actualRSS of ALL
inputs/predictors, restart30 actual both V/Z/work, pressureten complement10 reserve,
work8*(32*nv+24*nc)+2MiB and32MiB. Extra Float RHS conversion and residual/correction
arrays included in work. All inputs/factors/bases retire before full FE reconstruction.

Support controls: original full physical CSR bits/load/reconstruction, independent
Galerkin, nested P3 projected left inverse, dense triple/balanced polynomial SPD
(including eigenvalues above2), actual Float corrected inverse accuracy/approximate
linearity/symmetry, repeated/partial ownership/budget rejection and exact Float C/ABI.
Three fixed coarse action/reproduction vectors require<=1e-10/1e-8 before full solve;
sampled diagnostics never spectrum certification. One original4992tet control,
full1e-10/retained1e-11, flux/div1e-8, raw force gates,1800MiB/180s/3000iter/50000tet.
Small useful whole<=23.5018023327s; only pass permits matched28416tet L4,
whole<=106.642085s and>=300MiB complete velocityfactor/work saving vs873198472bytes.
Only useful matchedL4 permits saved43008tet finer geometry. No unchanged retries;
all rejected failures retained, no field on failed strict residual. Broader goal active.
