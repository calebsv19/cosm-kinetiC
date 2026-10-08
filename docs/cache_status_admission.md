# Scene-cache status manifest admission

Main Edit cache status no longer searches JSON text for field-name substrings or
accepts manifest-controlled absolute/alternate paths. It reuses the app's strict
bounded object parser used by configuration and jobs. Reads originally used a fixed 16 KiB buffer; the subsequent inventory slice
admits positive regular single-link metadata up to 1 MiB in bounded allocation,
check exact size/EOF and named identity before parsing, and refuse malformed UTF-8,
duplicate decoded keys, invalid/trailing JSON, NUL and structural overflow. The
reused parser limits depth to 64 and values to 100,000; cache metadata has its own 1 MiB byte admission. Entry witnesses now compare nanosecond timestamps.

The known v1 schema, run identifier, four integer fields and three fixed relative
active paths are required. Negative counters, nonpositive stride, integer type/
representation overflow and invalid retained-index type/count/sequence hold.
When both active and compatibility manifests exist they must be semantically
identical; status does not silently ignore a conflicting compatibility record.
Legacy records without optional retained indices/root/runtime metadata remain
supported. Optional root/runtime fields, when present, must match known layout.

Project source inputs, both manifests, selected active directories and bundle
paths use nofollow admission. Active trees preflight under the existing 10,000
entry/depth-16/8-GiB-per-file/32-GiB-combined metadata-size budgets and hold links,
special and hardlinked leaves. Status borrows the existing shared nonblocking
publication lock, holds on pending publication, and verifies owner identity or
continued lock absence after observation. Any failure clears output status so a
reused output structure cannot retain a previous ready result. Zero-frame metadata
cannot report ready. Existing escaped shell-command construction remains reused;
command formatting failure now also holds status.

Twenty-six status/native methods plus twenty-five recovery/native regressions
pass (thirty-five distinct methods; sixteen methods overlap). Ten new status
methods cover native and compatibility-only success, zero/no-cache output,
malformed/duplicate/decoy/NUL input, UTF-8/depth/byte bounds, required/schema/type/
range fields, fixed-path constraints, conflicting records, retained indices and
escaped valid keys, linked manifests/artifacts/source inputs, special and hardlinked
active entries. Failed status output is explicitly tested after initial ready=true.
The reused JSON dependency is wired into both native test harnesses and the actual
status Make contract; pkg-config probes use local json-c 0.19 while Make selects
its existing json-c 0.18. No dependency upgrade or shared API/version occurred. The rebuilt actual status
Make contract and supported headless scene-project cache-output fixture pass.

Retrospective proof compiles the sealed previous reader under the same fixture:
both old and new report the valid native cache ready. Previous code also reports
ready for trailing invalid JSON and absolute external bundle redirection; current
code refuses both and clears readiness/frame output. Baseline SHA-256:
ad225f00c2a129ad4f9b686c4daeeda578a7a0de97e56dda847ef424a052e4a3.

## Remaining requirements

Status now also checks the declared VF3D inventory, bundle linkage and raw header/
payload lengths through `cache_inventory_admission.md`. Full payload/source
authentication, pack contents and CFD numerical accuracy remain unqualified. Cooperative reader locking covers this status
entrypoint, not direct downstream asset readers. Immutable-generation consumers,
forward recovery, full hostile path-swap confinement, aggregate quotas/hard I/O
deadlines, archive-backed retirement and canonical adoption remain incomplete.

Evidence: data/experiments/lifecycle-validation/20261007-cache-status-admission.
