# Exact-cube pressure columns measured against complete Double inverse

Five independent controls pass; one original4992tet diagnostic bundle
6db4a1cdab561936a6280b62df2fa315321c7182497bacd3f31956acb2c70e2e completes12.6283s,
owned756.421875MiB/sampled742.046875MiB. FreshcombinedDouble stage956.87MiB isbelow
1800 with allcoexistingfactors/currentinputs/actualrestart30bothbases/diagnosticwork/
32reserve. Explicit Floatblock3/create7, Floatcoarseblock1/create7/action6 and
physicalDoubleblock1/create6 separated. No flowfield published; all diagnosticfactor
handles anduniqueinputs retired. Originalmesh/completeblockmatrix/RHS/free/Float
predictor hashes match qualifiedsmallcontrol; allpressuremodes/constantdirection kept.

FixedP3triple pressurecolumns: originalvelocityresidual1.06035..3.65961, maximum
solutionerror.217179, SchurWerror.192569/projectedKerror.175094 vs Double. CG8:
Werror.0846853/Kerror.0730251/skew.000818; CG16:Werror.0484878/Kerror.0392245/
skew.000285. Both nonlinearcolumnskews fail existing1e-5 pressureconstruction gate,
so no feeding/averaging these columns. QualifiedcompleteFloat maxvelocityresidual
.00278471/Werror.00019722/Kerror.00013859; completeDouble max6.1651e-12.
Positive constantdirectionenergy andactualcompleteSchur action observed; these do
not certify a full pressure nullspace oruniforminf-sup bound.

58 prescribed sampledpressure directions retainrank. OriginalDoubleprojectedSchur
samplecondition4.67494; balancedtenfixedproxy condition5.28741 atscale10, qualified
Floatproxy4.23958. Scale20 improvesfixedproxyto4.24267 butqualifiedstays4.23958,
so neither20nor30 passes prospective20percent BOTHproxyselection. No scalarcandidate
admitted from thissweep. Lowest sampledmodeonly3.339percent massenergy in ten
polynomials,55.85percent atends/64.00percent atwalls; outside-span residual.64660
islarge: no full eigenvalue/spectrum/stalledmode certificate. Geometryunchanged:
Jacobianmedian19.368/max215.373; nearbodymedian23.216/max82.325.

Next distinct candidate: three fixed physical residual corrections of the already
balancedP3triple action for pressureconstruction only. Its exactlinearSPD model is
3G-3GAG+GAGAG; Floatapproximationstillneeds originalpressure sym/positive/reproduction
and sampledconstructiondiagnostics, fullFGMRES/fullFE remainsauthority. Require
pairedprojectedK andSchurW errorsboth<=.75ofmeasuredfixedbaseline before one full
CG8field. Samewhole23.5018s andalloriginalresource/physicalgates; no arbitrary
nonlinear symmetrization, native/default adoption or broadgoal completion.
