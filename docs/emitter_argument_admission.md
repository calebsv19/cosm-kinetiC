# Emitter CLI argument admission and dependent-link rebuilds

Main Edit emitter diagnostics now validates the optional emitter index before
configuration/default-preset initialization, scene loading or backend setup.
Syntax is 1-32 ASCII decimal digits, with conversion overflow rejection and range
0..MAX_FLUID_EMITTERS-1 (currently 0..31). Signed, whitespace, fractional, prefixed,
trailing-junk and overlong/overflow strings are refused with bounded diagnostics.
Omitted index, explicit zero and leading-zero decimal inputs remain supported.
An in-capacity index still checks the projected scene's actual emitter count.

The previous native binary used atoi: malformed values could become zero and run
a diagnostic. Its argument test gate reports 19 failed subcases and one timeout
error across eight methods. These are not twenty equivalent bad-acceptance cases:
some strings were refused only after scene loading or with the old scene-count
message. The FIFO probe demonstrates attempted scene opening before invalid-index
refusal; the timeout killed/reaped the direct blocked test process. Invalid strings
such as abc/0tail and wrapping integer input actually selected emitter zero.
The retained baseline binary was not rebuilt or changed.

Eight final native methods pass against the explicitly selected completed build:
malformed syntax, overflow/capacity/length bounds, refusal before missing/FIFO
scene reads, default/zero real diagnostic runs, leading zeros, actual scene count,
and usage arity. Test binary and scene digests are stable across the gate. The
control-only test target requires an explicit binary selection and cannot silently
report an all-skipped success; missing selection refusal was checked. The selected
path is read from the environment, not interpolated into shell code.

A broader regression gate exposed a separate stale executable despite the object
being content-forced to rebuild. Whole-second Make timestamps can keep the old
link; a newer owned binary makes the same failure deterministic. Two before-code
probes fail for preserved-time header content and changed effective compile flags.
The Make graph now propagates content-forced object rebuilds to their declared
executable dependents, with ordinary application/FisiCs final links, shape tools,
headless/runner/session worker and the four migrated CLI object families mapped
explicitly. New configuration selections also force the tracked object links.
Unrelated requested links do not gain a content-change dependency unless their
object list intersects the dirty objects. Matching configurations remain no-ops.
This fixes declared graph propagation, not incomplete source graphs or all-platform
runtime qualification.

Final validation totals 50 distinct methods: 8 real native argument, 9 CLI object
(including both new deterministic link cases), 7 compiler boundary, 12 atomic
output, 13 dependency/input and 1 build identity. The first broad regression failed
its dot-component stale-value check; that log and the deterministic counterproofs
are retained rather than hidden by sleeps or timestamp-sensitive assertions.
The final 42-method regression gate passes, plus all 8 final native methods.
The terminal final native build succeeds. Matching repeat preserves 104 objects,
binary and manifest (106 outputs). Read-only cleanup preview admits 214 owned
files, including the additional retained configuration history after Make fixes.
No cleanup applied. Existing source scene and the baseline binary remain unchanged.

The new emitter-argument-identity-20261007 profile was refreshed only for this
turn's source/Make corrections; no older user profile was rebuilt. Intermediate
build/test logs are retained, but only qualified final logs support acceptance.
This is CLI admission/build freshness evidence, not physical CFD accuracy,
immutable inputs, sandboxing of arbitrary scene paths, hard resource quotas,
coherent multi-file recovery, installed/public freshness or full lifecycle proof.
Contract/worker/platform closure, complete toolchain identity, recovery, retirement
and canonical adoption remain open.

Evidence: data/experiments/lifecycle-validation/20261007-emitter-argument-admission.
No commit, canonical adoption, installation, release, user evidence cleanup or
independent backup coverage changed. This packet is outside the frozen seventy-
packet backup batch. The complete TL01-TL13 goal remains active and incomplete.
