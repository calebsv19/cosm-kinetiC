# Small restart checkpoint: matched memory admission recovered, time cap preserved

Four independent mixed-solution, strict full-target, parameter and iteration/basis
controls pass. New isolated probes accept only restart8/4 and explicitly require
requested reconstructed full1e-10 before publication. Their literal source
transforms bind the unchanged previous complete Float-factor reference and all
physical equations/pressure modes. Default60 and historical probes remain unchanged.

| Control | Iterations | Whole seconds | Owned peak MiB | Full residual | Outcome |
|---|---:|---:|---:|---:|---|
| Original,8 |150|12.806|365.406|7.375e-11|complete gates pass|
| Original,4 |310|13.759|397.594|8.735e-11|complete gates pass|
| Normal,8 |176|80.479|1042.406|8.198e-11|divergence fails; no field|
| Normal,4 |250|99.865|1103.750|7.501e-11|complete gates pass|

Normal8 maximum divergence1.4224e-8/s exceeds unchanged1e-8. Normal4 is6.8818e-9/s.
Reject8 for this control.4 reduces normal owned peak11.3% against prior60 but costs
15.0%more whole time. It is an optional investigation setting, not a faster default.
Normal4 force/scalar changes versus60 are below7.2e-10 relative; raw surface/reaction
mismatch stays2.108%. Pressure coefficient relative difference8.82e-7, mass relative
1.29e-7, global mean difference2.72e-12Pa are measured readbacks, not a forward-error
or full pressure-spectrum certificate. All physical pressure modes remain present.

The exact same-surface second-normal held-L4 L8 tensor case remains33216tet.
Restart4 basis reservation31,703,720bytes includes both flexible bases and all
reserved work. The actual stage currentRSS619,380,736bytes + factor1,125,998,956 +
scratch38,745,361 + reserve33,554,432 + basis estimates1,849,383,205bytes
(1763.709MiB), below1800. This recovers admission without dropping coefficients.
Full identity matches prior restart12 withheld stage and factor/scratch are unchanged.
This is the previously declared non-nested tensor remesh, not nested tetrahedra.

The numerical run passes its own fresh admission and reaches solve completion at
106.732s, full-residual verification phase145.476s; remaining physical observations
hit the supervisor180.047s stop. Sampled peak1665.625MiB. The stopped run has no final
result or field; a progress phase cannot establish its residual or numerical
acceptance. Iteration400 retained residual8.196e-10 still exceeds1e-10. Momentum and
continuity must remain independently audited after reconstruction in a completed run.
Continuity dominates iteration300; this run does not prove a terminal residual stall.
No force comparison or signed observation of this unaccepted field is allowed.

Evidence: `build/c3d-restart-small/` frozen supervisors/sources, four calibration
receipts, exact stage, preserved terminal time stop, source transform/control test
receipt, calibration pressure/force readback and once-only checkpoint audit.
No native/API/desktop default, commit, package or install changes. Stage1 and all
independent1%force, raw/reaction, mesh/domain gates remain open. Next is a distinct
restart6 calibration to recover completion time while retaining actual budget.
