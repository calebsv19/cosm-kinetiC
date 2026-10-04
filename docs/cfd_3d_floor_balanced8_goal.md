# One combined minimum-width fine surface and balanced far-slab mesh

After floor-grid rejectionb6ab7937be491631b87e8a7a13b20606ef490ec177af1f698d820d3dfd417415,
declare ONE new mesh: original minimum body strip h=.5*(1-cos(pi/6)), body nodes
[0,h,.15,.30,.50,.70,.85,1-h,1], max.20 interval,768triangles. Remove intermediate
normal .09375m plane, retain closest .046875m/far anchor .1875m; two equal far
streamwise slabs in L4 and three in L8. Outside y/z planes unchanged.43008/49920tet.
This combines independently helpful changes in one prospective candidate, not a
retry of any failed earlier mesh. Original body/box/volume/areas/symmetry and held
nearbody matching remain. Nonnested geometry, not original tetrahedron partition.

All original prospective global/edge.025/.05/.1/body.05/.1/.2 worst intrinsic shape,
volume-weighted mean and max Jacobian condition nonworse allowance1e-8; edge.05
weighted shape>=1percent improvement; maximum surface edge decreases, triangles
increase; positive exact volume/boundary planes/areas/reflections/y-z and50000cap.
One capped dual-domain geometry survey after four support checks. No relaxation.
If dual-domain pass, selected geometry only enters ONE L4 capped full P4/DG-P3
solve with sealed complement10 engine, original Float64 physical/mode/quadrature,
full1e-10/retained1e-11/flux/div1e-8/energy.03/1800MiB/180s/3000iter/fresh symbolic
factor/bothbases/work/coarse/scratch/currentRSS+32MiB admission/atomic publication.
No admission or time-cap retries. Require>=10percent raw surface/reaction mismatch
decrease for usefulness; original1percent consistency gate separately unchanged.
Only useful strict L4 field permits one L8 physical candidate. No altered forces,
empty-reference gate substitution, native/default/general adoption or certification.
All prior geometry/solver failures remain sealed. Broad goal remains active.
