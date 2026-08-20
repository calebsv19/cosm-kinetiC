# PhysicsSim Agent Guide

This repository builds the PhysicsSim source program and the packaged desktop
product `kinetiC`.

Use this guide first when operating the repo as a fresh human or AI agent.
The supported external-agent workflow is local source-checkout headless
operation. It is not a desktop package workflow, not a website download
workflow, and not remote worker submission.

## Read First

1. `docs/AGENT_CONTROL.md`
2. `docs/AGENT_DEMO_PACK.md`
3. `docs/headless_cli.md`
4. `docs/current_truth.md`
5. `docs/README.md`

Use `docs/headless_cli.md` as the detailed CLI reference. Use this file as the
short operational contract.

## Supported First Workflow

If this repository was cloned by itself from GitHub, run these from the repo
root:

```bash
make physics_sim_headless
make test-physics-sim-headless-water-mode
make test-physics-sim-headless-scene-project-cache-output
```

If you are inside the larger CodeWork workspace parent, use the equivalent
workspace form:

```bash
make -C physics_sim physics_sim_headless
make -C physics_sim test-physics-sim-headless-water-mode
make -C physics_sim test-physics-sim-headless-scene-project-cache-output
```

Interpretation:

- `physics_sim_headless` proves the local source checkout can build the
  supported headless entrypoint.
- The Water smoke is the smallest deterministic public source-checkout proof.
- The scene-project cache-output fixture is the first scene-project-shaped
  proof and confirms PhysicsSim-owned cache output without mutating
  `scene_authoring.json`.

Expected first-proof outputs are listed in `docs/AGENT_DEMO_PACK.md`.

## Second-Tier Local Supervision

After the direct headless workflow is clear, a local agent may use:

```bash
make physics-sim-job-runner
make test-physics-sim-job-runner-bundle-smoke
```

From the CodeWork workspace parent, use:

```bash
make -C physics_sim physics-sim-job-runner
make -C physics_sim test-physics-sim-job-runner-bundle-smoke
```

`physics_sim_job_runner` is trusted-local supervision over
`physics_sim_headless`. It is not a public upload endpoint and not a remote
worker submission path.

## Routine Local Commands

These are safe local source-checkout commands for agent operation:

```bash
make physics_sim_headless
make test-physics-sim-headless-water-mode
make test-physics-sim-headless-scene-project-cache-output
make test-physics-sim-headless-cli
make physics-sim-job-runner
make test-physics-sim-job-runner-bundle-smoke
```

Equivalent commands from the CodeWork workspace parent:

```bash
make -C physics_sim physics_sim_headless
make -C physics_sim test-physics-sim-headless-water-mode
make -C physics_sim test-physics-sim-headless-scene-project-cache-output
make -C physics_sim test-physics-sim-headless-cli
make -C physics_sim physics-sim-job-runner
make -C physics_sim test-physics-sim-job-runner-bundle-smoke
```

Use these only with normal local filesystem write access to `tmp/` in a
standalone clone, `physics_sim/tmp/` from a workspace parent, or to the
generated output roots named by the fixture scripts.

## Do Not Treat These As First-Start Commands

Do not put these in a first external-agent quickstart:

```bash
make -C physics_sim run
make -C physics_sim package-desktop-refresh
make -C physics_sim package-desktop-self-test
make -C physics_sim release-distribute
make -C physics_sim package-linux-worker
make -C physics_sim package-linux-worker-dry-run
make -C physics_sim test-physics-sim-headless-wind-long-tunnel-video
make -C physics_sim test-physics-sim-headless-dragonwind-orientation-probe
```

Reasons:

- desktop app commands are GUI, local packaging, or human app workflows
- release commands can create, sign, notarize, stage, or distribute artifacts
- worker-package commands create package roots but do not install or publish
  them
- long Wind/video probes are proof and development lanes, not first-start
  checks
- DragonWind/local-system probes may depend on local private artifacts

## Version And Surface Split

- Public desktop current: `kinetiC 0.2.0` for macOS arm64.
- Source checkout and worker-package line: `physics_sim 0.3.2`.
- Local headless proof uses the source checkout, not the public desktop ZIP.
- Worker-package evidence is separate from public desktop package evidence.
- Worker packages are internal/fleet package-root artifacts unless a future
  public release explicitly publishes them as downloads.
- There is no public remote submission API in this contract.

Do not describe `physics_sim 0.3.2` as the public desktop current unless a
future approved desktop release updates website metadata, production-registry
state, and public readback.

## Mutation Boundaries

The first agent docs must not claim support for:

- remote worker submission
- report-inbox loops
- VPS, home-server, Linux PC, or Raspberry Pi direct control
- website deploys
- production-registry mutation
- release artifact builds, signing, notarization, upload, or promotion
- desktop app freshness beyond the recorded public desktop current
- public untrusted upload or sandboxing of arbitrary scene/project paths
- public worker-package downloads
- public remote job submission

For cross-host, worker-fleet, website, registry, package, or release-control
work, stop and use the workspace-level CodeWork routing docs and skills first.

## Development Notes

- C source uses C11, 4-space indentation, and `snake_case`.
- Public APIs live under `include/`; implementation lives under `src/`.
- Generated build outputs live under `build/`, `tmp/`, and selected fixture
  output roots.
- Do not make commits unless the user explicitly asks for a specific commit.
