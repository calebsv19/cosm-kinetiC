# Disposable compiler output lifecycle

Ordinary object, dependency and binary outputs use `scripts/atomic_output.py`.
Successful writers register exact byte and file identities. Existing destinations
must match those receipts before the compiler starts. Unknown or changed files
hold the build; preserve them and select a fresh build root. A successful build
never serves as implicit adoption of a pre-existing file.

The wrapper compiles into a fresh sibling staging directory, verifies success,
flushes outputs, and rechecks every destination's prior identity before replacing
it. A destination that appeared or changed during compilation holds publication.
Dependency files receive the same admission before either dependency or object
publication. Failure and catchable interruption preserve the admitted predecessor.
Forced termination can leave unclassified staging files, which hold cleanup.

Make holds cooperative ownership for its full lifetime. These checks are not a
sandbox against malicious compiler code or an uncooperative filesystem writer;
there remains a narrow recheck/publication race outside that ownership contract.
Dependency and object replacement are separate filesystem operations, rather
than a multi-file crash transaction. Installed acceptance and power-loss recovery
are separate qualifications.

Twenty-four additional ordinary session/solver and 2D CFD contract targets now
use this wrapper: channel, MAC2D, surface force, obstacle/step/boundary/force,
open2D/exit/corner/obstacle/budget/centered/donor/reference/force-check, momentum
flux, masked energy/transient, pressure guess/scaling/multigrid, plus session
observation and solver qualification. These contain 26 compiler commands.
Historical campaign and qualification scripts that write numerical evidence
alongside their binaries still require separate retained-lifetime migration.

```sh
make test-atomic-output
make test-contract-writer-lifecycle
make BUILD_DIR=build/my-contract-profile test-cfd-channel
make BUILD_DIR=build/my-contract-profile clean-plan
```

Validation covers all 24 actual recipe bodies with controlled compiler behavior,
failed rebuild preservation, refusal before invocation for changed predecessors,
unknown dependency holds, and mutation during compilation. Five native targets
(channel, MAC2D, surface force, momentum flux and pressure multigrid) passed in
a fresh root. Their binaries were copied and hash-checked into retained proof
before guarded clean removed only that fresh registered root. Historical build
and numerical evidence were untouched. These checks qualify lifecycle behavior
and the named native contracts, not general 3D CFD accuracy.

## Refined CFD contract writers

Twelve additional refined mesh/diffusion/projection/transient/transport/channel,
split-channel, channel-units, energy, obstacle-evolution and stdout obstacle
probe recipes now compile through the same wrapper and run from `BUILD_DIR`.
Their numerical arguments, flags and assertions are preserved. The mixed
coupling and mixed-result recipes still write fixed-path JSON evidence; those
require retained-run migration rather than disposable-output registration.

The recipe behavior fixture now executes 36 migrated targets and verifies 38
registered outputs. Failed compiler and unknown predecessor checks cover both
the ordinary channel and refined mesh families. This routing proof uses a
controlled compiler; native runtime checks are recorded separately. Remaining
historical writers and standalone worker consumers still need audit.

## Pressure/material and atmosphere contract writers

Twelve pressure-trace API, anisotropic trace, material-scaling and passive,
evolving and open atmosphere normal/sanitizer recipes now honor `BUILD_DIR`
and use guarded compiler publication. Their test programs produce stdout and
no persistent run files; numerical source, flags and assertions are unchanged.
The controlled recipe fixture exercises 48 migrated targets and verifies 50
registered outputs, with failure and changed-predecessor admission checks for
ordinary channel, refined mesh, pressure API, passive sanitizer and open
atmosphere outputs. Real native outcomes are recorded in the corresponding
retained validation packet.

The worker builder/consumer migration is recorded below. Fixed-path periodic
JSONL and atmosphere convergence reports remain retained-run migration work.
These gaps prevent whole-family completion.

## Atmosphere worker build and consumer selection

The three worker file targets now belong to `BUILD_DIR` and use guarded atomic
publication. Make exports absolute `PHYSICS_SIM_PASSIVE_WORKER`,
`PHYSICS_SIM_ATMOSPHERE_WORKER` and `PHYSICS_SIM_OPEN_ATMOSPHERE_WORKER` paths.
The three CLI defaults and six direct/coupled test consumers use the same
app-local selection helper in `passive_atmosphere.py`; convergence imports the
selected open-atmosphere worker from its existing test module. A missing selected
binary never causes fallback to another profile. Empty or relative environment
overrides are refused. Outside Make, the default remains the default build root;
a custom build can be selected through the environment or explicit CLI `--worker`.

All three real workers built in a fresh selected root. Six actual Make consumer
suites passed 39 tests plus three native contract prerequisites. Three selection
checks passed via control-only Make, and finite readback requests verified each
result worker SHA-256 against its selected binary. Reference/package/public state
is unaffected. This app-local orchestration reuses the existing admission helper
and compiler lifecycle rather than introducing a new shared module.

Standalone execution ownership is recorded below. Full transitive external
JSON-C/library identity remains broader audit work. Bounded capture follows.
The selected-root Make suites retain cooperative build ownership for their
lifetime; these results do not establish an untrusted worker sandbox.

## Live atmosphere worker output bounds

Passive/evolving/open atmosphere wrappers now reuse the `tool_probe.py` capture
engine. Native output is read as raw bytes with live separate limits: 64 MiB
stdout by default, 64 KiB stderr, and 120 seconds of execution. Open requests
retain their existing declared stdout allowance up to 256 MiB. Capturing stops
at the bound rather than materializing arbitrary output and checking afterward.
The limit bounds captured payload bytes, not native memory, JSON parse memory or
overall adapter RSS. Probe callers retain their smaller original combined cap.

Capture owns a process group, forwards catchable interruption, terminates owned
group descendants on all outcomes, and bounds the final child wait to five
seconds. Signalling/refused teardown produces an unverified result and native
adapters refuse acceptance. Escaped process groups and forced helper termination
are outside this trusted-local contract. Inherited cooperative Make ownership
descriptors pass into workers. Standalone wrapper calls now acquire execution ownership as described below. Worker hash and numerical/state acceptance checks remain.

The behavioral suite covers raw bytes, live independent stdout/stderr overflow,
wall timeout after closed pipes, background descendants, inherited descriptors,
invalid bounds, adapter refusal, SIGTERM and controlled teardown refusal.

## Atmosphere worker configuration prerequisites

All three worker file targets now depend on the selected configuration stamp and
force a rebuild when a new stamp is first selected, including whole-second Make
timestamp ties. Compiler identity, selected worker paths and configured
`JSON_CFLAGS`/`JSON_LIBS` participate in the digest. Worker recipes consume those
configured JSON flags directly; they no longer independently query pkg-config.
Output admission validates all three destinations inside the selected root.

An actual-recipe fixture compiles three finite C stand-ins with real clang. It
verifies changed compile flags, link flags and compiler invocation rebuild every
worker, matching settings leave every binary untouched, and every worker output
override into source storage holds before mutation. A changed predecessor also
holds publication. This demonstrates recipe/configuration behavior, not numerical
source acceptance or external library binary/header provenance.

## Standalone atmosphere execution ownership

All three public Python `run` adapters hold cooperative ownership from before
worker hashing through native execution and final state/physics acceptance.
Direct calls acquire the worker directory subtree with the existing ancestor
and global cleanup hierarchy. Same/parent builds and cleanup hold; independent
sibling roots remain available. Make callers borrow their existing owner without
reacquiring and pass descriptors through native capture. A subprocess CLI launcher
inside an owned Make run must forward `worker_subprocess_descriptors()` explicitly;
closing those descriptors causes an honest hold rather than implicit borrowing.

Inherited admission verifies exact descriptor/file identity, no symlinks, kernel
lock witnesses and reaffirmation through the claimed open descriptions. Unlocked
metadata cannot borrow another owner's active lock. The context releases only
its own descriptors after return or exception. Descriptors passed into surviving
workers keep kernel exclusion if a supervisor is killed. Forced termination and
escaped descendants are still outside complete recovery qualification.

Supported workers must be regular, non-symlink files under this checkout's
`build/`. External binaries and workers from other checkouts require their own
owning lifecycle and are held, rather than being executed without coordinated
ownership. Explicit CLI `--worker` still selects custom roots inside this build
namespace. The low-level byte-capture helper itself is not a public ownership
contract; callers should use the decorated atmosphere adapters. Output JSON
publication occurs after accepted adapter return and retains its prior contract.
