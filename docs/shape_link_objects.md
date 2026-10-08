# Shape tool object and link identity

Main Edit shape_mask_tool and shape_asset_tool no longer pass the timer_hud
external cJSON C source directly to their link command. SHAPE_SHARED_OBJS maps
that source into the already-owned timer_hud_external object namespace, reusing
the existing compile rule, generated dependency admission, content-based graph
refresh and compiler-boundary observations. No new copy or duplicate compile
namespace, shared source/API edit or shared version change was introduced.

Both final Darwin link recipes now opt into the existing linker-input manifest
contract and graph reuse admission. Other platforms reuse the tracked object
mapping but retain their separate final-link provenance limits. Their Linux or
FisiCs execution is not qualified by this macOS evidence. Reusing the normal
compile rule supplies its CFLAGS instead of the previous implicit source compile
inside a LDFLAGS-only command; the real selected conversion output parity below
qualifies this change for the tested input, not every cJSON consumer or shape.

The real native before-code shape baseline builds. Three actual Make countertests
then fail: shared link declarations contain a raw C path, a preserved-mtime change
to its header leaves both binaries stale, and a preserved-mtime change to its source
also leaves both stale. The current code passes all three, including updated native
return values, matching no-ops, generated external dependencies and Darwin final
link manifests for both tools. This is actual behavior evidence, not a static
recipe assertion alone.

The gate passes 33 distinct methods: 3 new shape methods, 3 object-dependency,
13 linker-input, 13 dependency/input admission and 1 build identity. A fresh real
shape-object-identity-20261007 profile builds both native tools with 8 owned objects
and 8 generated dependencies. Matching repeat leaves all 12 object/binary/manifest
outputs unchanged. Read-only cleanup preview admits 22 files. No cleanup applied.
The full real default declaration still has 340 objects and 340 deduplicated
matching dependencies; the cJSON object was already declared, so no new global
object count is claimed.

Real baseline and current tools convert the retained import/u_shape.json into
32x32 PGM masks and shape asset JSON in fresh proof storage. Both current outputs
match their baseline equivalents byte for byte. Input SHA-256 is unchanged. This
qualifies the selected CLI conversion flow; broader shape input correctness,
output path admission/publication, resource policies and installed acceptance
remain separate. The baseline profile was not rebuilt after the repair.

Other direct C source-to-binary recipes (pack/dataset/trace/emitter diagnostics and
contract/worker/platform variants) still need their own complete compile closure
and final-link qualification. This repair advances two ordinary CLI tools, not
full graph completeness, toolchain identity, immutable inputs or coherent
binary/manifest/receipt recovery. Retirement, canonical adoption and the broader
TL01-TL13 contract remain incomplete.

Evidence is sealed in data/experiments/lifecycle-validation/20261007-shape-link-objects.
No commit, canonical adoption, installation, release, user evidence cleanup,
old-profile refresh or independent backup coverage changed. This packet is outside
the frozen seventy-packet backup batch.
