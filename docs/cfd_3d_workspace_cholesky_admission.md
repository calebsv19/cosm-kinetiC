# Exact stopped-normal stage admission control

The count6 base completes with caller-owned exact Cholesky, owned peak about
1353 MiB. The normal factor needs 1325505336 bytes, about 411 MiB more than the
base; extra mesh/input/symbolic residency makes the remaining headroom uncertain.
Measure the exact stopped normal through symbolic and free-page-control stages
without numerical factor allocation. Preserve the same full mesh/free/RHS/mixed
identities, SDK/reference ownership and owned high-water. No field is published.

Define a conservative stage budget: observed current RSS after the control plus
exact symbolic factor storage, exact numeric scratch and a predeclared 32-MiB
reserve. This is an admission estimate, not measured numeric RSS or a relaxed
resource cap. If above 1800 MiB, reject numerical launch. Both supervisor/owned
peak and time caps remain 1800 MiB / 180 s. A fitting stage estimate still needs
a full accepted numerical run before any field/force claim. No CPU/mesh/residual/
force equation or acceptance threshold changes.
