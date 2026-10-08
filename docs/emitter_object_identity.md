# Emitter diagnostic object identity and source closure

Main Edit runtime_scene_emitter_diag_tool now compiles its sources into tracked
private objects, using the existing atomic compiler/dependency admission and
content-based reuse path. The final Darwin object-only link uses linker input
manifests and receipt binding. Its effective compile flags participate in global
build configuration selection. The prior compile prefix, include paths and
JSON/math link libraries are retained for the existing sources.

The real before-code target fails to link: it omits implementations for
core_scene_compile_dependency_manifest_inspect, core_scene_compile_sha256 and
physics_sim_runtime_mesh_preview_resolve_migrated_path. The source list now uses
the existing complete CORE_SCENE_COMPILE_SRCS family plus the existing mesh-path
resolver. No solver/shared source, API or version changes were made. The newly
included scene bundle source needs Darwin declarations; the first migrated native
build exposed this compile error. The existing -D_DARWIN_C_SOURCE object rule is
extended to the emitter's bundle object, rather than changing the shared source.
Both failure logs are retained. Final native build succeeds.

Source-relative object names normalize dot components before becoming Make target
names, matching normalized receipt and content-rebuild targets. This matters for
the historical cJSON source spelling through core_mesh_preview/../../shape.
A native dot-component fixture proves preserved-time header rebuild and matching
no-op. Default vendored source roots are qualified here; external/custom source
roots and cross-host layouts still need separate qualification.

Frozen previous Make fragments produce three failing counterprobe subcases: the
emitter's direct C source misses a preserved-time header change; its object list
is missing; and the newly introduced per-tool flag override is unused by the old
recipe. The flag case is a new selection acceptance check, not proof of a defect
in the old supported global CFLAGS selection. Current tests also prove source
changes, effective flags and matching no-ops across all four migrated CLI families.

Validation: 41 distinct methods (7 CLI object including the separate dot-component
method, 3 object coverage, 13 linker input, 13 dependency/input, 4 environment and
1 build identity). The main gate runs 40 methods; the additional mapping method
runs separately on final code. A fresh emitter-object-identity-20261007 profile
contains 104 owned objects/dependencies and the binary/link manifest. Matching
repeat leaves all 106 object/binary/manifest outputs unchanged. Read-only cleanup
preview admits 213 files, including retained configuration history after the
Darwin compile-rule repair; no cleanup applied. The real default declaration now
has 473 objects and 473 unique matching dependencies.

The final native binary refuses missing arguments and a missing scene with exit 1.
It successfully runs retained tests/fixtures/scene_runtime_launch_projection.json,
selects attached emitter 0, creates the 249x208x135 diagnostic domain and completes
its diagnostic emitter step. Logs and the unchanged input digest are retained.
This proves the selected executable/bridge/backend path operates; it does not
qualify physical CFD accuracy, arbitrary scene resource safety or human acceptance.

Limits: compiler/linker observations do not make inputs immutable; system-header
closure under -MMD, complete toolchain resources, platform/custom-root variants,
coherent multi-file recovery, hard runtime/aggregate resource budgets and export/
retirement policy remain open. Remaining direct native contract and worker/platform
producer variants need compile/link closure audit. Canonical still requires separate
adoption; this work does not protect its old clean command.

Evidence: data/experiments/lifecycle-validation/20261007-emitter-object-identity.
No canonical adoption, commit, installation, release, old user profile refresh,
user cleanup or independent backup coverage changed. This evidence is outside the
frozen seventy-packet backup batch. The complete TL01-TL13 goal remains incomplete.
