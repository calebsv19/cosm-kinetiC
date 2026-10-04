# Local resolution improved; global weighted shape still rejects both meshes

Four support tests and the one7.532s/sample286.594MiB dual-domain screen pass as
diagnostics. Both first-strip widths(original.0669873 and.07m) pass every local
edge/body worst/weighted/conditioning rule and global worst/max condition. Edge.05
weighted shape improves4.71830→3.57944/3.51829 (24.1/25.4percent). Outer body.2max
condition improves71.638→70.712. But global weighted shape worsens: L4baseline7.67159
becomes9.33501/9.33835; L8baseline10.91308 becomes13.61475/13.62006. More transverse
surface grid intervals propagated through original long streamwise slabs create
this separate global regression. Neither is selected or numerically factored.

Next distinct combined hypothesis keeps the original minimum body spacing and
finer.20max surface grid, with the previously screened two/three equal outer slabs.
The earlier thin-strip balanced slabs improved global weighted shape but failed
local body.2; the minimum-width grid passes all local bands but its long outer
slabs fail global weighted shape. Combine the geometric changes only in one new
prospectively declared mesh, retaining all existing strict gates and resource caps.
No original failed mesh is retried and no physical success is inferred before full
numerical force checks. Broad goal remains active.
