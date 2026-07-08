# PhysicsSim Agent Control

This document is the public repo-local control contract for agents operating
PhysicsSim from a source checkout.

Public product name: `kinetiC`.
Repository/program key: `physics_sim`.

## Supported Contract

The first supported external-agent workflow is local source-checkout headless
operation:

1. build `physics_sim_headless`
2. run the deterministic standalone Water smoke
3. run the scene-project cache-output fixture when a project-shaped proof is
   needed

This contract does not cover public desktop package installation, website
download validation, remote worker submission, or release publication.

## Read Order

1. `AGENTS.md`
2. `docs/AGENT_DEMO_PACK.md`
3. `docs/headless_cli.md`
4. `docs/current_truth.md`
5. `docs/desktop_packaging.md` only for desktop package context

## Quick Commands

Run from the CodeWork workspace root:

```bash
make -C physics_sim physics_sim_headless
make -C physics_sim test-physics-sim-headless-water-mode
make -C physics_sim test-physics-sim-headless-scene-project-cache-output
```

These commands build and exercise the direct headless path. They write only to
generated local output roots such as `physics_sim/tmp/`.

## Output Authority

For direct headless proofs, trust these generated files first:

- `run_summary.json`
- `run_progress.json`
- `volume_frames/<preset>/manifest.json`
- `volume_frames/<preset>/scene_bundle.json`
- Water sidecars such as `water_manifest_v1.json` and
  `water_surface_000000.json`
- scene-project cache manifests under `<project>/physics_sim/`

If a command fails, inspect CLI stderr and the generated summary/progress files
before changing code.

## Path Trust Boundary

`physics_sim_headless` and `physics_sim_job_runner` are trusted local operator
tools. They do not sandbox arbitrary paths.

- `--runtime-scene` is a read-only local input path.
- `--scene-project` is a trusted local project directory. PhysicsSim reads
  `scene_runtime.json`, validates `scene_authoring.json`, and writes
  PhysicsSim-owned cache/run directories and manifests under that project.
- `--output-root` is the direct run artifact root. Non-empty roots are rejected
  unless `--overwrite` is provided.
- `--summary` and `--progress` are sidecar write paths; keep them under the
  output root unless a supervising tool owns a separate job root.
- `--jobs-root` belongs to detached local supervision.

Do not expose either tool as a public upload, web, or untrusted worker request
endpoint without a separate wrapper policy.

## Detached Job Runner

The detached runner is a second-tier local supervision workflow:

```bash
make -C physics_sim physics-sim-job-runner
make -C physics_sim test-physics-sim-job-runner-bundle-smoke
```

It writes local job state under `build/agent_runs/jobs/<job_id>/`, including:

- `job_request.json`
- `job_status.json`
- `run_progress.json`
- `stdout.log`
- `stderr.log`
- `pid.txt`
- `result_summary.json`

The bundle smoke also writes a shared report under
`output/report.json`. Treat these as local evidence, not public service state.

## Version Wording

Use this wording:

- Public desktop current: `kinetiC 0.2.0` macOS arm64.
- Source checkout and worker package: `physics_sim 0.3.0`.
- Local headless proof uses the source checkout, not the public desktop ZIP.
- Worker package evidence is separate from public desktop package evidence.

Do not describe `physics_sim 0.3.0` as the public desktop current unless a
future approved release updates website metadata, production-registry state,
and public readback.

## Exclusions

These are not part of the first external-agent contract:

- remote worker submission
- report-inbox loops
- direct VPS, home-server, Linux PC, or Raspberry Pi control
- website deploys
- production-registry mutation
- release artifact builds, signing, notarization, upload, or promotion
- desktop app freshness beyond the recorded public desktop current
- public untrusted upload or sandboxing of arbitrary scene/project paths

For those lanes, use the workspace-level CodeWork routing and release-control
docs before taking action.
