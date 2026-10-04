# CG8 flexible velocity and fixed pressure Schur proxy

Predecessor f292238d38ef625fe2d9a8de1bd8ff57d95c1407db690091d6f4bc008584b099
preserves CG8 nonlinear pressure symmetry rejection before outer solve. New distinct
candidate only changes pressureten coarse column construction to the already
strictly validated P3/local triple action. Same factor handles/Z/Ac; extra balanced
owner references common inputs with no duplicate factors/matrices/bases. Pressure
W and mass complement10 unchanged formula, with fixed validated velocity PC model.
Nonlinear CG8 used ONLY through flexible outer velocity action. W is a PC proxy,
not a modified physical pressure matrix; original all pressure modes/coupling stay.
Do not relax existing symmetry<1e-5/reproduction<1e-5/positivecoarse gates.

All frozen CG8/P3 assembly/FloatABI/filledpattern/resource/8CG controls reused.
Support adds pressure W equality to validated fixed P3, constructed nonlinear
symmetry rejection independently, fixed proxy symmetry/SPD and ownership release.
No additional dense Nv*Nc or factor;40nv+24nc workspace and fresh combinedstage
BEFORE bothnumerics unchanged. Validate fresh allocation/actualbothVZ/work and
all completed owners retire before independent original FE reconstruction.
Small original4992tet first, full1e-10/retained1e-11, originalmatrix/RHS/Floatidentity,
separate force/Pin/D within1e-7, whole<=23.5018023327s, same flux/div1e-8/energy.03/
atomicfield/1800MiB/180s/3000iter/50000tet. Only useful small permits one matched
28416tet L4 whole<=106.642085s and>=300MiB complete newvelocityfactor/work saving
vs873198472bytes. Only useful large permits savedquality43008tet finerL4. No
cap/gate relaxation, unchanged rejected retry, native/default/physicalcertification.
Broad usergoalactive. This goal and support frozen BEFORE numerical measurement.
