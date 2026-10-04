# Force sensitivity testing resumed; held-strip redistribution not promoted

Four independent mesh controls and paired geometry survey pass. Both domains retain
positive original fluid volume, exact cube/domain areas and planes, reflection/YZ
symmetry and all strict global/local worst/weighted shape/Jacobian checks. Original
first edge strip.0669872981 is held; four central intervals.2165063509 reduce maximum
body triangle edge from.3535533906 to.3061862178m (13.397percent). Surface432triangles
and28416/33216tet counts remain. Edge.05 weighted shape improves4.718301 to4.582130
(2.887percent). This is non-nested redistribution, not uniform resolution proof.

One L4 field passes every original numerical/resource/publication check:
78iterations, supervisor82.626627s, owned1296.140625MiB,
full original FE residual8.9048769234e-12. Fresh numerical-stage projection
1394.774138MiB passes1800; factor873198472bytes and scratch33556391 include full
physical operator, both actual Arnoldi bases/work, pressurecoarse and32MiB reserve.
Full-load/catalog restoration and completed workspace retirement prove original
authority survives reconstruction. Flux1.605e-14/maxdiv1.416e-10/energy2.142e-11.

| Original streamwise quantity | Accepted baseline L4 | Redistribution L4 | Change |
| --- | ---: | ---: | ---: |
| Pressure force N | .024838232793 | .024750242698 | -.354253percent |
| Raw symmetric viscous force N | .014217810488 | .014202444342 | -.108077percent |
| Reaction force N | .039759879391 | .039680388632 | -.199927percent |
| Inlet pressure Pa | .022347466898 | .022313935545 | -.150045percent |
| Dissipation W | .000178979115 | .000178675423 | -.169680percent |
| Raw surface/reaction mismatch | 1.770216902percent | 1.833907421percent | worsened |

The full field provides actual force-sensitivity evidence. It does not improve the
original1percent raw force consistency gate: decline mesh promotion and the
conditional matching L8 solve. Original reference recipe remains qualified for
bounded force testing; numerical convergence does not certify physical forces.
No corrected/reaction-only/subtracted-energy replacement. No native/default/
package/install adoption. Broader goal remains active.

Evidence: build/c3d-heldfloor6/checkpoint-audit.json, paired-heldfloor6 geometry,
L4-selected-heldfloor6 immutable receipt/field, comparisons.json. Next bounded
work is documented in cfd_3d_heldfloor6_next_goal.md.
