# Balanced finer surface improves most shape metrics but fails outer body band

Four support tests and one8.373s/sample314.047MiB dual-domain survey pass as diagnostics.
Neither .05 nor .055 strip profile meets ALL declared geometric gates. Global
conditioning and all edge/inner-body bands improve, but body-distance.2m worst
shape/condition worsen. Baseline worst14.36357/condition71.63798; .05 gives17.28089/
89.98450 and .055 gives16.26042/81.86103. Edge.05 weighted shape falls4.71830→3.91190/
3.79843, but that improvement does not excuse the outer body-band failure. No
profile is selected or numerically factored; no field or force accuracy is claimed.

Next distinct physical-resolution hypothesis preserves the old minimum body strip
(or modestly enlarges it) while adding resolution between that strip and the center.
Maximum surface spacing still decreases.25→.20 and triangle count432→768. Avoiding
smaller tensor cells should remove the aspect-ratio regression and permit the
original far planes, with fewer total tetrahedra. Exact profile and prospective
ALL-band gates must be declared before its screen. No failed profile is retried,
no quality/cap/force metric is relaxed. Broad goal remains active.
