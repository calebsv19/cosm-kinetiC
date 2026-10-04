# Explicit collection control rejected

Five controls plus sealed vector/load proofs establish actual unreachable cycle
collection with weakrefs, live symbolic owners and immutable inputs/action/full
FE preservation, unchanged global GC policy, owned peak monotonicity and partial/
fitting/over-budget cleanup. Original/base accepted fields match predecessors at
150/160 iterations, 18.898/141.285 s and 377.719/1132.563 MiB. Both meet requested
1e-10 full target and numerical gates. Base is 1.171% more memory and 15.601% more
measured time than the lossless-load control; these costs do not justify adoption.

Normal symbolic/live stage estimates 1898017528 bytes (1810.091 MiB), above 1800,
so numerical launch is withheld. Its collection reports 26 objects and no immediate
current-RSS reduction. Do not attribute large allocator residency differences to
reclaimed cycles, or object count to released bytes. Symbolic handle is cleaned
up; no normal field/force exists. Reject this collection path for adoption.

Evidence: `build/c3d-collect-pressure/checkpoint-audit.json`, five tests, two
accepted control fields and stage-only normal rejection. Original equations,
pressure/mixed/RHS identities, older evidence and native workers remain intact.
No native/shared API/version/commit/package/install/deploy change. Stage 1/full
goal remain open. Next suspend only deterministic FE coordinate/index/mapping
metadata unused by the assembled exact solve, restore bitwise after factor cleanup,
and remeasure normal stage plus numerical live guard before force testing.
