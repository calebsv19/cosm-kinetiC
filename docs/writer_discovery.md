# PhysicsSim writer discovery and next repair priorities

The bounded first-party inventory inspected 1,750 selected files (13,451,244
source bytes): 1,034 Python, 666 native C/header, 23 shell and 27 Make files.
It recorded 5,357 static signals across 1,029 files and 418 explicit script
references in shell/Make. No admission/parse gaps were reported in this selected
scan. These are review candidates, not distinct unsafe writers or behavioral
acceptance. Tests and declarations can produce false positives; aliases, macros,
external libraries and dynamically resolved commands can hide writers.

## Concrete inspected candidates

| Surface | Trigger and observed behavior | Required repair/proof |
| --- | --- | --- |
| Canonical cleanup | The ordinary checkout retains broad recursive removal recipes; Main Edit guards have not been adopted. | Reconcile and adopt the bounded reviewed guard, then run headless, Water and scene-cache proof on the adopted tree. |
| `src/app/preset_io_save.c:10` | Saving presets opens the destination with `w` before serialization; `fprintf` and final close failures are not checked. | Stage a complete checked candidate, preserve the predecessor on failure, validate slot bounds and demonstrate failed-write recovery. |
| `src/app/menu/shared_theme_font_adapter.c:333` and `:406` | Saving theme/font preferences truncates their persistence files; write/close failures can still return true. | Checked predecessor-preserving publication and failure injection for both preference saves. |
| `src/app/physics_sim_file_helpers.c:121` | General text save opens the destination with `wb`; a failed write can leave a partial destination and close failure is ignored. | Audit callers and output classes before introducing a reusable admitted atomic-save contract. |
| `src/app/sim_runtime_backend_2d.c:708` | Snapshot export opens the final path with `wb`; checked binary writes can fail after truncation and final close is unchecked. | Retain failed attempts and publish a complete snapshot only after checked completion; establish caller ownership/root policy. |
| `src/app/scene_project_cache_output.c:724` | With `allow_overwrite`, publication removes VF3D and Physics run/active directories before four copy passes and final manifest publication. A later error can leave predecessor caches gone or mixed outputs. | Admit run identity and all roots, coordinate ownership, stage immutable run outputs, then commit a coherent active reference with recoverable interrupted states. Inspect existing downstream contracts before changing this layout. |

These observations establish source-level failure exposure. No destructive fault
was injected into user data, no native implementation was changed in this slice,
and no public/package numerical behavior was qualified. The recursive cache
helper uses `lstat` for its current leaf; complete ancestor-swap confinement and
whole-operation ownership have not been established by this review.

## Using the discovery tool

Run `python3 -B scripts/writer_inventory.py --repo /absolute/checkout` for JSON on
stdout, or `make test-writer-inventory` for its five focused behavioral tests.
The inventory creates no records or generated roots. It scans scripts, tests,
src, include, make and root Make files; generated/build/third-party/external trees
are outside scope. Per-file input is bounded at 4 MiB, total input at 128 MiB,
entries at 20,000 and depth at 16. Its 120-second budget is checked between
entries, not a hard interruption deadline for filesystem I/O or parsing.

Python uses AST discovery; native, shell and Make use lexical discovery. An
observed ownership adapter in a file does not prove every write uses it. Every
file reports behavioral coverage false; deletion authorization is false. The
native mode parser was repaired after a focused test exposed a missed write
beside a subsequent read-only open; the failed log is retained with the passing
log. Five tests cover language signals/edges, links/special files/parse gaps,
bounds and read-only CLI behavior. The final full scan passed selected admission.

## Remaining top-level work

After canonical cleanup protection, prioritize native user-data preservation and
coherent scene-cache replacement. Continue mapping supported commands to output
root, owner, class, replacement policy, interruption recovery and retirement
eligibility. Finish worker lifetime/log bounds, transitive build provenance,
aggregate workflow/storage budgets and exact archive-backed guarded pruning.
A passing inventory does not close these requirements. Keep clean, evidence
retirement, environment retirement and package retirement distinct.

The original 303 MiB backup has independent archive and restore receipts. The
later seventy-packet batch remains locally prepared after automatic approval
review rejected its transfer; no upload began. This new packet is outside both
frozen backup scopes. No cleanup, pruning, commit, canonical adoption, install or
release occurred.

Evidence: `data/experiments/lifecycle-validation/20261007-writer-discovery`.


## Entrypoint scope correction and frame deletion hold

Writer discovery now includes tools and bounded extensionless shell/Python
entrypoints plus Make helper-variable bindings. The current scan selects 1,839
files, 6,308 static signals and 746 edges with no selected admission gaps; ten
previously omitted entrypoints are now visible. Legacy rm_frames now previews
retention for its own checkout and refuses blind deletion. Twenty-two distinct
tests and real control-only Make repeats pass without a compilation profile.
See entrypoint_cleanup_audit.md for the newly identified Wind probe recursive
reset, bundler temporary cleanup and packaged-launcher/installer publication gaps.
Behavioral completeness, guarded retirement, canonical adoption and newer backup
remain open; the expanded inventory is discovery, not acceptance.
