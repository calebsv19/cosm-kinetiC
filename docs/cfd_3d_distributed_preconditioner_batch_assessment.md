# Distributed cube preconditioner batch assessment

The distributed cubic correction establishes strict full reconstructed convergence
on the original4992tet filled cube with unchanged physical equations. Performance
is not yet useful under the prospectively declared23.5018023327s whole limit, so
no new28416tet matched or43008tet finer trial was admitted. Native/default and
qualified exactcoupled pressure-complement10 recipe remain unchanged. Broadgoalactive.

| Candidate | Outer iterations | Full FE residual | Whole seconds | Result |
| --- | ---: | ---: | ---: | --- |
| Distributed P2/local fill | 3000 | 9.9656e-7 | 64.873 | strict residual and cost fail |
| Distributed P3/local triple | 498 | 1.0610e-11 | 43.358 | strict pass, cost fail |
| P3/CG8 with fixed pressure proxy | 113 | 8.9354e-12 | 24.321 | strict pass, cost fail |
| P3/CG8 with native scalar coarse action | 113 | 9.0141e-12 | 23.636 | strict pass, cost fail |
| P3/CG16 with same scalar action | 105 | 8.2065e-12 | 29.993 | strict pass, worse cost |

Direct nonlinear CG8 use in pressureten coarse construction was separately rejected
by the existing symmetry gate before outer iteration. Fixed previously validated
P3/triple action for pressurecolumns resolves compatibility; nonlinear inner CG is
confined to flexible outer velocityapplications. Original pressuremodes and constant
direction remain.35 new independent controls pass; partialownership/resourcefailure/
inputmatrix/Galerkin/FloatABI/source-transform/refinedcoarse/SPDmodel/independentCG/
nativeaction controls and all sealed predecessors retained. All full fields preserve
separatepressure/rawviscous/reaction/Pin/D within1e-7 of qualified originalfield;
full conservation/energy/resource/publication and completedowner retirement pass.

The native coarse action median cost is39.5775percent lower with1.131e-16 action
error. Combinedlocal/cubicfactor64902436bytes versus oldfullfactor94037704bytes
is30.9843percent smaller on thissmallcase. This is a factorstorage measurement only:
wholepeak394.953125MiB for scalarCG8 versus359.90625MiB qualifiedsmall,35.046875MiB
HIGHER because construction/coarse/basis/work overhead remains. No completeworking-
memory gain or large/finer memoryqualification is asserted. Finerexactfactor
freshadmission remains2087.527MiB against1800, gap287.527MiB.

CG16 gains only8 outeriterations at substantialtimecost. Pressurecoarse extremal
metadata differs from qualifiedinverse, and prior pressuremodecoverage was limited;
these are clues, not pressurebottleneck/operator-error/spectrumcertificates. The next
bounded pressurecolumn/coverage diagnostic must distinguish pressureproxy error from
velocity cost. [Prospective next steps](cfd_3d_p3_cg16_scalar_next_goal.md) require
one usefulsmall candidate before matchedL4 then savedfinerL4. Existing matchedL4/L8
and heldfloor6 fields support current force sensitivity tests; rawforce mismatch
1.7702to1.8339percent and originaldomainPin/D failures remain. Furtherresolution
forceconvergence still needs a useful finerfield; no physicalcertification yet.
