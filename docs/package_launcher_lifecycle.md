# Packaged launcher inspection and selected initialization admission

Both packaged shell launchers now resolve configuration defaults and answer
--print-config before touching logs, runtime roots, resource lanes or Vulkan
configuration. Key-value output uses printf, preserving literal backslashes.
CONFIG_INSPECTION_READ_ONLY=1 and
CONFIG_PATH_SCOPE=requested_without_writability_probe state the query scope.
The reported destinations are requested paths, not a writability or startup
acceptance claim. macOS reports the planned VK_ICD_GENERATION_ROOT and current
operator driver overrides; inspection does not allocate or select a new ICD
file. The existing release audit only needs those driver keys to be present.
Main Edit profile/runtime/log/build-label output remains compatible with its
existing consumer checks. The macOS plist namespace storage components reject
path separators and other invalid component characters before any initialization.

## Selected initialization safeguards

Mutation modes preflight runtime, runtime/data/runtime, runtime/data/snapshots,
log directory and log file before their first initialization write. Selected
macOS vk paths are checked only when an ICD generation is needed. The shell
checks require absolute unambiguous bounded paths, reject control characters,
linked destination components, nondirectory ancestors and special/linked log
leaves, and refuse runtime/log locations inside the package. Only the established
macOS /tmp and /var aliases to /private/tmp and /private/var are allowed as linked
components. This is cooperative path admission, not confinement against a
concurrent hostile path swap or hard-link aliases.

Failed configured roots now fail explicitly. The launchers no longer switch to
shared predictable TMPDIR runtime/log paths. Checked mkdir/touch failures stop
startup. --self-test remains an initializing operation, and normal launches still
hand exact argument boundaries to the app in the selected runtime directory.
The macOS --agent-mcp handoff remains separate and its stdout is not contaminated
by initialization-tool output.

macOS now creates a fresh mktemp ICD generation below runtime/vk for each selected
startup. It builds and lints an XML plist using native plutil, then performs
checked native JSON serialization before exporting the generation. This avoids
manual JSON interpolation and preserves old fixed/previous generation files,
including a legacy fixed symlink. Driver overrides are preserved independently;
when both exist, no unused ICD is created. Tool failure leaves its fresh candidate
retained and prevents app handoff. All plutil diagnostics go to stderr.
This Mac's native plutil failed inserting into JSON and rejected nested JSON in
-lint, so validation occurs on the constructed plist before JSON conversion;
fixture Python JSON parsing independently verifies the produced JSON and exact
library path, including quotes/backslashes. These checks do not execute MoltenVK
or qualify a real Vulkan loader/device.

## Verification

Twenty-five actual-launcher fixture methods pass on this Mac, including Linux
and macOS inspection from absent/default/explicit roots; repeated bytes/mode/
mtime/inode preservation; linked and blocked requested paths; source namespaces;
FIFO/linked logs; relative/ambiguous/control-character/package-overlap roots;
self-test and stub application handoff; real native plist generation and preserved
ICD history; explicit driver overrides; failed-tool candidates; MCP stub stdout;
and JSON escaping. Linux shell fixtures run locally; Linux native GUI/session
behavior is not inferred from them. Mac-only cases require the native plist tools
and are explicitly skipped on hosts where they are unavailable.

Eight release-audit methods and fourteen package-proof regressions pass:
47 distinct affected methods total. The real control-only Make entrypoint repeats
the 25 launcher methods with compiler/pkg-config unavailable and creates no
requested build profile. The release-audit suite now includes the real repaired
launcher and proves the actual audit recipe passes without creating an absent
disposable home. Its fixture was refreshed to include the existing anchored
supervisor dependency and packaging control directory; initial missing-dependency
failures are retained. No production release recipe was changed.

The first 21-method launcher run had seven failing platform subcases: the new
control-character grep missed embedded newlines, and native plist-tool format
limitations prevented macOS startup. A following run retained five native
validation failures before correcting the XML-lint/JSON-conversion order.
Those logs and both failed reproductions remain. The corrected 21 methods and
expanded 25 methods pass. All newly started direct command/session handles have
observed terminal results; no historical process hold was reaped.

Persistent selected fixtures retain inspection, ICD history and failed-tool
behavior. Safe old-script controls show that --print-config previously created
runtime/log state on both platforms. Fixture copies preserve link targets as
observed-links.json metadata rather than recreating live links; relocated replay
is not claimed. Shell syntax and git diff whitespace checks pass. No GUI,
simulation, signed package, installed app or real user profile was exercised.

## Explicit next findings

This slice does not close resource-lane lifecycle ownership. Two synthetic
observations are retained and remain open:

1. macOS still creates runtime resource symlinks. A stub save through runtime/
   config/app.json changed the fixture's package Resources/config/app.json.
   Private writable config publication and predecessor-preserving migration of
   existing links are required before claiming immutable-package behavior.
2. Linux still copies missing resource lanes directly to their final names and
   accepts existing lanes. A synthetic cp exit 17 left partial config; the next
   ordinary stub launch accepted that directory. Fresh staged publication,
   complete inventories and interrupted-attempt reconciliation are required.

Runtime/log ownership, competing initializers, hard-link/concurrent-swap behavior,
aggregate disk/log/RSS limits, power-loss recovery, generation retirement, full
child/descendant supervision, installed/released platform acceptance and canonical
adoption also remain incomplete. No automatic cleanup was added. Canonical remains
unchanged and still has the legacy broad clean/launcher behavior. The pending
archive was not retried; this packet is outside the frozen archive cutoff.

Evidence: data/experiments/lifecycle-validation/20261007-package-launcher-inspection.
