# Sixteen-step velocity correction: strict success, worse usefulness

Three new independent controls pass; same original physical/Galerkin/Float/native
factor inputs and pressureproxy, actual bothVZ/work and40nv+24nc reservation, exact
literal inner8to16 change. Reused pressure/CG8/P3/native-action controls remain.
Original4992tet bundle6874a50bad87ff044560237be492bf18f7e84cee1d8e84a070d1bdbea1457087,
L4-body2-original-p3-cg16-scalar-receipt.json:105outer/29.9927219581s,
owned399.3125MiB/sampled399.3125MiB/full8.20654647990e-12.
Fullmom8.189059e-12/continuity5.354576e-13, originalphysicalidentity and separate
forces/Pin/D<=1e-7 equivalence, pressurecoarse gates pass, allcompletedowners
released and originalFE/field/conservation/physicalenergy/caps pass.

Assembly4.22835/setup2.32727/solve17.00622/full3.63453s. Doublinginnersteps yields
only7.080percent outer reduction from113to105, but whole26.8966percent slower
than scalarCG8/23.63558s; unchanged23.5018023327s usefulness fails. Preserve strict
numerical field only; no large/finer trial/native/default/physicalcertification.

The ten-mode approximate pressurecoarse extrema remain.0340023575/.0905205678,
versus qualifiedfullFloatvelocity.0395102324/.1014045350 on same originalmesh/
polynomialmassbasis. Extrema alone do not certify pressure operator error or a
spectrum. Full reconstructed P2 failure wasmomentum-dominated, but strongerlocal
CG gains now flatten: pressureproxy and modecoverage need independent diagnostic
before more velocitywork. No exact pressure nullity/conditioning certificate inferred.
[Next bounded pressure-column and coverage diagnostic](cfd_3d_p3_cg16_scalar_next_goal.md).
Broadusergoalactive; accepted exactcoupled pressure-complement10 recipe unchanged.
