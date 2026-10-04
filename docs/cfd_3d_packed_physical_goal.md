# Packed physical block action and proved concurrent velocity work

Original3x3 physical coefficients and arithmetic dependencies remain; a new native
entrypoint holds component values in registers, decodes original Double bits and
stores each block's outputs once. Original exported actions and sealed packed C
prefix stay byte-exact. Only inner CG8 uses the equivalent new action. Factor graph,
compensation, Float predictor, fixed pressure10/complement10/P3/Float3 and cap8 unchanged.

Concurrent dynamic work bound:8*(24*nv+24*nc)+4MiB. This is a new proved recipe,
not a postmeasurement relaxation of the previous candidate. Derivation covers:
- Mixed and balanced input/output/difference/product vectors, including evaluation
  of coarse+local before the final correction: at most8full velocity equivalents.
- Native CG8 owns7scratch+1output; input aliases the above caller vector. Finite
  guards and temporary component permutations have up to4additional equivalents.
  Fixed linear-pressure triple peak also fits within24velocity equivalents. Validation
  coarse reproduction probes fit the same bound; persistent inputs remain in actualRSS.
- Coarse restriction, three corrections, Double products, differences, old/new results,
  C Double output and two Float vectors fit16coarse equivalents. Native SparseSolve
  scratch is separately bounded by8*nc+2MiB, explicitly checked before any coarse solve.
  The remaining2MiB covers allocation headers/small temporaries. Reject on excess.
- Both flexible V/Z and original outer work, pressure reservations, symbolic/factor/
  numeric scratch and32MiB remain separate. Constructor uses fresh actualRSS with
  all new work; every numeric admission and actual whole peak keeps1800MiB/180s.
No factor/operator storage is silently removed. Final owner/atomic full-FE proof stays.

Expected exactmatched factor/work savings302.356MiB are eligibility prediction only.
Prospective action benchmark compares against sealed PACKED CG8 baseline on the same
factor/coarse inputs (not the slower Python recurrence),20loads/3alternatingbatches.
Require>=5%complete action cost improvement, relative action error<=1e-10 and positive
work. Then original strict small fullFE1e-10/retained1e-11/whole<=23.5018023327s,
original force/Pin/D equivalence1e-7/flux/div/energy/owner/publication gates. Only useful
small evidence earns exactmatched full trial: whole<=106.642085s and>=300MiB complete
factor/work saving. Only usefulmatched admits finerL4. Freeze before measurement;
keep every failure, no native/default/physical adoption inferred from microbenchmarks.
