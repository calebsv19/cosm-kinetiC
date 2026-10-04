# Exact filled-cube pressure-column and coverage diagnostic

Predecessor0e2d65e3e09ba50c4209d8b018fa11d0a8b6d46a00e7a59d2529f970007a0f68:
CG16strict field but slow29.993s; bestCG8/23.636 narrowlyfails23.502s. Previous
turn PROGRESS: physical/cost evidence changes next action to pressure diagnostics.
One original4992tet mesh/matrix/RHS/Float predictor identity, unmodified P4/DGP3/
quadrature/condensation/body/loads/pressuremodes. No flowfield qualification.
Measure originalvelocity true residual for ten coupling*polynomialpressure RHS under
fixedP3triple, CG8, CG16, qualifiedcompleteFloat and completeDouble factor. Report
columnsolution, SchurW, projectedenergy andcoarseblockskew errors against Double.
Nonlinear columns are diagnostic ONLY, never fed to symmetricpressure construction.
Constant pressure kept, no gauge penalty/removal. Positive constant Schurenergy
checks assembledboundary system only; no full nullity/inf-sup certificate.

Reused P3/localfactors and Float/Double ABI libraries staged separately: original
Floatblock3 create7, Floatcoarseblock1 create7/scalaraction6, Doublephysicalscalar
create6. ExactDouble uses original complete scalarCSR velocitytriangle asauthority.
Freshstageguard BEFORE every numeric factor includes currentRSS of all live inputs/
previousfactors, newfactor storage/scratch,32MiB, complete restart30 bothbasis/work,
pressure reserve, P3nestedwork and diagnosticreserve8*(5*nv*10+12*nv+16*np*64+
12*64^2)+8MiB. P3factorfreshguard uses samecompletebudgets. No peak resets/cap changes.
All original1800MiB/180s/3000iter/50000tet; diagnostic phases aftereach10column/
coverage10batch andoriginal64macro assembly; terminalimmutablecapture. Numerical
input/source hashes and factorhandles/uniqueinput owners verified, allclose paths.

Fixed58-or-less mass-orthonormal prescribed coverage basis from existing pressure
coverage helper; sampled originalDoubleSchur and completeFloatSchur actions, actual
outside-span residuals andunchanged meshconditioning. No claims that sampleeigenvalues
are fullspectrum orprove which mixedmode stalls. Evaluate pressurePC complement
scalesONLY{5,10,20,30}, same tenZ/W andpositive masses. Analytic/densecontrols show
unchangedequations andSPD/reproduction; nonlinearpressure symmetry stillrejects.
Report sampledcondition for fixedP3 andqualifiedFloat proxies against SAME DoubleY.
Prospective selection: require candidateworst(condition/correspondingscale10condition)
<=.8 for bothproxies; choose smallestworst ratio, smallerpositive scale ontie. This is
only eligibility for ONE distinct fullCG8 candidate, not solver/physical adoption.
No arbitraryaveraging of nonlinearSchur, changingpressuremodes or broader sweeps.

Selectedcandidatefulloriginalsmall stillrequires1e-10/retained1e-11, samephysical/
RHS/Floatidentity, separateforcescalarequivalence1e-7 andwhole<=23.5018023327s.
Only usefulsmall permits exact28416tetL4<=106.642085s and>=300MiBcompletevelocity
factor/work savingvs873198472; onlyusefulmatchedpermits43008tet finerL4. Physical
force/energy failures preserved; native/default/qualifiedrecipe unchanged. Broadgoalactive.
