# Pack, dataset and trace CLI object identity

Main Edit vf2d_pack_tool, vf2d_dataset_tool and physics_trace_tool now compile
sources into tool-private object namespaces under the selected build profile,
then perform object-only final links. Names preserve source-relative paths rather
than flattening basenames. The three namespaces intentionally preserve each
legacy tool's distinct compile flags, including dataset-only definitions and
include paths, rather than borrowing application objects with different flags.
The effective tool CFLAGS also participate in build configuration identity.
The existing final command prefixes and libraries are retained; this does not
add cross-architecture flags the previous commands did not use.

All tool objects use the existing admitted dependency-producing atomic compiler
path with -MMD/-MP. The declared Clang dependency/configuration object set includes
all three families. Their Darwin links opt into actual linker input manifests and
receipt-bound graph admission. Other platforms gain the declared object graph but
retain separate final-link provenance and execution qualification requirements.
No shared source/API or version changes were made.

Frozen pre-repair Make fragments yield nine failing subcases across five methods:
three real stale-header cases, three missing object-declaration subcases and three
effective tool-flag cases (the newly introduced flag selection is unused by the
old recipe). Current tests pass preserved-time header and source rebuilds, matching
no-ops, dependency/link manifests, per-family flag invalidation and declaration
coverage. The current gate passes 42 distinct methods: 5 CLI object, 3 shape,
3 object coverage, 13 linker input, 13 dependency/input, 4 environment and 1 build
identity. This is behavioral fixture coverage, not all-platform acceptance.

A fresh real cli-object-identity-20261007 profile builds the three executables,
29 objects/dependencies and 3 final linker manifests. The per-tool object counts
are 8 pack, 12 dataset and 9 trace. Matching repeat leaves all 35 object/binary/
manifest outputs unchanged. Read-only cleanup preview admits 66 owned files;
no cleanup was applied. Full default declarations now have 369 objects and 369
unique matching dependencies, adding those 29 isolated objects to the previous
340-entry declaration.

Three real integration fixtures pass: dataset JSON export, manifest-to-trace export,
and pack/dataset parity. Two inspector-consuming fixtures now accept an explicit
PHYSICS_SIM_PACK_CLI_BIN and refuse a provided non-executable path before allocating
fixture work. With an override, they do not invoke the shared core_pack Make build.
This proof compiles a private native inspector from existing shared sources into
fresh retained temporary storage. Both invalid-inspector refusals pass. Default
fixture behavior still builds the shared inspector; this optional injection does
not establish lifecycle safety for the shared build itself. All three complete
small fixture roots, logs, inspector and generation artifacts are retained.

Limits: dependency observations are not immutable inputs; -MMD omits system-header
content closure. Full toolchain/resource identity, non-Darwin and cross-architecture
qualification, arbitrary flag side effects and coherent binary/manifest/receipt
recovery remain open. Runtime scene emitter diagnostics, direct contract commands
and additional worker/platform producer variants still need complete compile/link
closure. Export output path/publication/retirement policy and CFD physical acceptance
are separate. No installed or public release claim follows from these proofs.

Evidence: data/experiments/lifecycle-validation/20261007-cli-object-identity.
No canonical adoption, commit, installation, release, old-profile rebuild, user
cleanup or independent backup coverage changed. Evidence lies outside the frozen
seventy-packet backup batch. The complete TL01-TL13 goal remains active/incomplete.
