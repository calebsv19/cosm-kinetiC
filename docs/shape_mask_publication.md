# Shape mask PGM publication

Main Edit shape_mask_tool now reuses physics_sim_persistence for PGM output.
It admits an existing parent and destination, holds cooperative parent ownership,
writes a retained exclusive candidate, checks header/body writes and consumes the
stream through checked flush/close/sync/publication. It no longer truncates the
selected predecessor before the replacement is ready. Output failure produces
exit status 1, including late buffered-write failure. A current input/output
inode alias refuses before publication. No-output diagnostic operation remains.

The file-class bound is 64 MiB mask data plus 64 bytes of header allowance. The
existing helper refuses linked ancestors, symlink/hardlink/special destinations,
protected system and Git source storage, unknown lock state and a cooperative
competitor. Git-local output must use the helper's admitted generated roots;
external trusted-local operator-selected output is still supported. Failed
candidates and lock files remain in their admitted parent. Explicit successful
replacement supersedes the prior file; this is not immutable evidence history.
False after a final rename can mean complete visible bytes with unconfirmed
durability. The new caller does not claim full hostile path-swap confinement,
input snapshot authentication, archive coverage or a durable recovery inventory.
Input-alias checks bind the current named files, not all concurrent substitutions.

The mask link reuses the existing app persistence/headless-output objects, their
normal atomic compile/dependency rules and Darwin link provenance. They join its
explicit content-forced link dependency map. The ordinary graph already owns
these app objects; no shared source/API or duplicated helper implementation was
introduced. An actual Make regression proves a preserved-time persistence header
change rebuilds only the mask executable, leaves the asset executable unchanged
and then repeats without writes. Synthetic fixture support objects exercise the
real Make graph; actual native publication tests exercise the real helper.

Old-binary countertests report seven failures and one FIFO timeout across the
initial nine methods. The real process-local RLIMIT_FSIZE probe (SIGXFSZ ignored)
leaves only 64 PGM bytes where the predecessor was, demonstrating destruction;
the old writer ignored late close failure and could report success. Separate
temporary source-alias and protected Git source probes both fail against the old
binary. The blocked FIFO direct child was killed/reaped by subprocess timeout.
No old profile or source scene was rebuilt/mutated. Countertests use temporary
copies rather than risking the retained real input.

Final validation passes 43 distinct methods: 11 actual PGM publication, seven
numeric CLI, 15 native persistence, six sidecar atomic and four actual Make/link.
Publication tests cover missing parent, linked/hardlinked output, linked ancestor,
FIFO/directory, cooperative competitor, write fault with unchanged predecessor
and a retained 64-byte candidate, input alias, protected Git source and successful
create/replace. Selected binary and source digests stay unchanged across tests.
The fresh shape-output-publication-20261007 profile builds both shape tools.
Normal 32x32 PGM and asset conversions match unchanged retained baseline output
bytes. The final matching repeat preserves fourteen object/binary/manifest outputs;
read-only clean preview admits 26 owned files. No cleanup applied.

Asset output still uses the shared path-only serializer and needs an appropriate
stream/publication seam; it has not gained PGM safeguards through this change.
Shape input file/JSON admission, remaining asset geometry, output recovery and
retirement, hard I/O/workflow bounds and the two earlier sanitizer process holds
remain open. This turn's native tests are terminal and are distinct from those
held attempts. Installed/platform/public/physical-CFD qualification and canonical
adoption remain separate. No commit, install, release, user evidence cleanup,
canonical change or independent backup coverage changed. This packet is outside
the frozen seventy-packet backup. The complete TL01-TL13 goal remains incomplete.

Evidence: data/experiments/lifecycle-validation/20261007-shape-mask-publication.
