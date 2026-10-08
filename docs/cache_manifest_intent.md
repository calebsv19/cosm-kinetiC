# Generated cache manifest intent and selection bounds

A native fclose fault changed the writer's output before witness capture. The
preceding publisher accepted all 16 tested combinations: wrong run ID, timestamp,
frame count or unexpected field in each generated slot or all three slots. A
stable witness did not prove that the writer initially emitted requested metadata.

Every generated manifest now passes the existing bounded strict JSON reader and
cache manifest contract after its witness is captured, before publication. The
writer's exact 13-field shape is required; project/runtime paths and retained
indices must be present. Run ID, frame count, export start/stride/max and created
stamp must match the request. Existing contract checks bind schema, asset paths,
integer representation/ranges and the complete exact retained-index sequence.
Whole-set/per-slot witness rechecks bind this readback to the subsequently
observed generated files. No source format or shared API/version changed.

Before source admission or attempt creation, the publisher calculates the
selection count in int64 arithmetic and requires it to equal frame_count, in
1..10,000. Source count/start remain ordinary nonnegative int values, stride
positive, max nonnegative. This makes the existing index writer bounded by the
requested admitted selection instead of potentially iterating INT_MAX entries
before any readback. A large source domain remains valid when max or stride keeps
the selected count bounded. The writer does not silently repair inconsistent
requests or replace frame_count with an inferred count.

Three new native methods cover 16 wrong-intent writer cases, eight inconsistent
or unbounded requests that retain all predecessors without an attempt, and three
large-domain/sparse valid selections whose three generated manifests retain the
correct complete sequence and export fields. Existing source/staged consistency,
copy/payload, interrupted rollback and compiler-profile checks also run, together
with the actual status/headless producer integration.

Reuse: strict JSON, cache semantic contract, named witnesses and retained
publication remain the existing app boundaries. No generic shared parser,
cryptographic primitive or new release surface was introduced.

This verifies generated publication metadata against request intent. It does not
prove CFD accuracy, authenticate the source's provenance, create durable digest-
bound candidate inventories, eliminate check-to-rename races or provide atomic
seven-slot reader visibility. Generated schema changes need an explicit matching
writer/readback change; tolerant status handling of older optional fields is
unchanged. Forward recovery, immutable generations, hard workflow/I/O quotas,
archive-backed retirement, canonical adoption and installed/downstream gates
remain open. The newer packet has not been independently archived.

Evidence: data/experiments/lifecycle-validation/20261007-cache-manifest-intent.
