# Runtime configuration save and shutdown result handling

Main Edit `config_loader_save` now serializes to a retained pending stream under
the existing native persistence adapter. It no longer opens its destination with
truncation. The adapter holds cooperative parent ownership, verifies admitted
predecessor/parent/lock identities, checks flush/close/sync and publishes a checked
candidate. Missing parent, linked/special/hardlinked destination, linked parent,
protected checkout source roots, competing or changed owners hold. The existing
16 MiB publication bound and failure-after-rename unconfirmed semantics apply.
See `native_persistence.md` for the exact contract and its limitations.

Both normal and legacy-headless shutdown paths now check preset and configuration
save results individually, report which save is unconfirmed and return failure if
either is unconfirmed. Interactive shutdown also requests an SDL error dialog
before SDL teardown. Both saves are attempted even if one fails; this is not a
transaction spanning both files. The dialog result is ignored and stderr remains
the diagnostic fallback. Successful shutdown still returns success. Existing
menu save-result checks consume the now stronger configuration writer without
callsite changes. Configuration field serialization/schema is unchanged.

Six actual compiled configuration methods passed: native save/load and Python
JSON parsing of ordinary default-derived output, flush/close/rename faults with
predecessor and pending-attempt retention, failed first publication, linked
file/parent refusal, source-root refusal and FIFO/nonempty-lock refusal. Fourteen
native persistence regression methods also passed. The actual Make configuration
contract passes in `build/profiles/config-save-20261007`; its old fixed global
temporary filename now uses a unique create-only fixture file to prevent
concurrent test interference. `src/main.c` compiles in that isolated profile under
its actual Make flags and atomic compiler wrapper. No installed app was launched;
interactive dialog behavior and full shutdown runtime failure observation remain
unqualified. Compilation is not runtime acceptance.

## Remaining source findings

The older configuration reader has unbounded whole-file reads and a hand-written
substring/object/string parser. The serializer interpolates path strings without
JSON escaping and does not reject nonfinite floating-point values. These remain
open semantic/admission risks; ordinary JSON roundtrip evidence does not qualify
arbitrary strings or malformed inputs. Fix them with reader/writer compatibility
proof before claiming a complete configuration contract.

`physics_sim_ensure_runtime_dirs` in `src/app/data_paths.c:215` still performs
path-based mkdir for data/runtime/scenes and data/snapshots, treating EEXIST as
success without nofollow/type/identity validation. It can follow an existing
linked data/runtime ancestor before the safer save adapter is reached. The safer
preference directory adapter does not qualify that older startup helper. Its
callers and thirteen selected Make source lists need migration/proof. General
text saves, binary snapshots and scene-cache overwrite also remain open.

No shared API/version changed: this slice reuses the app-owned adapter after the
prior core_io/core_data/core_pack reuse review. No canonical adoption, commit,
cleanup, pruning, package, installation or release occurred. The broader lifecycle
goal remains incomplete. This packet is local, outside the original independent
archive and later frozen prepared backup.

Evidence: `data/experiments/lifecycle-validation/20261007-config-save-lifecycle`.
