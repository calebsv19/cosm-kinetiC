# PhysicsSim Persistent Main Edit Worktree

Last updated: 2026-09-03

## Lane identities

- Canonical source: `<workspace>/physics_sim` on `main`
- Persistent Main Edit source: `<workspace>/_worktrees/physics_sim_main_edit`
  on `codex/physics-sim-main-edit`
- Canonical package: `kinetiC.app`, bundle `com.cosm.kinetic`
- Main Edit package: `kinetiC Main Edit.app`, bundle
  `com.cosm.kinetic.main-edit`
- Canonical runtime/log namespaces: `PhysicsSim` and `PhysicsSim`
- Main Edit runtime/log namespaces: `PhysicsSim-Main-Edit` and
  `PhysicsSim-Main-Edit`

The Main Edit application is a local development artifact. It is not the
public desktop package, a worker package, release candidate, Registry record,
publication, deployment, or version decision.

## Start gate

Read both lanes and every registered worktree before editing:

```sh
git -C <workspace>/physics_sim status --short --branch
git -C <workspace>/_worktrees/physics_sim_main_edit status --short --branch
git -C <workspace>/physics_sim worktree list --porcelain
git -C <workspace>/physics_sim rev-list --left-right --count \
  main...codex/physics-sim-main-edit
```

Only one writer may own the Main Edit checkout. Existing RC7C, release,
temporary, detached, prunable-registration, or other specialist lanes remain
separate and must not be reset, cleaned, repurposed, pruned, or removed to
simplify topology.

## Checkpoint gate

Run focused checks before the broad source/package ladder:

```sh
git -C <workspace>/_worktrees/physics_sim_main_edit diff --check
make -C <workspace>/_worktrees/physics_sim_main_edit \
  main-edit-package-contract-checks
make -C <workspace>/_worktrees/physics_sim_main_edit clean
make -C <workspace>/_worktrees/physics_sim_main_edit clang-build
make -C <workspace>/_worktrees/physics_sim_main_edit test-fast
make -C <workspace>/_worktrees/physics_sim_main_edit run-headless-smoke
make -C <workspace>/_worktrees/physics_sim_main_edit package-desktop-self-test
make -C <workspace>/_worktrees/physics_sim_main_edit \
  package-desktop-main-edit-self-test
```

Use `test-stable` when the active feature crosses runtime-scene, editor,
Water/Wind, runtime-mesh, export, or broad integration surfaces. Package
self-tests prove local package identity and isolation; they do not establish a
public release or worker-runtime result.

## Package targets

```sh
make -C <workspace>/_worktrees/physics_sim_main_edit package-desktop-main-edit
make -C <workspace>/_worktrees/physics_sim_main_edit \
  package-desktop-main-edit-self-test
make -C <workspace>/_worktrees/physics_sim_main_edit \
  package-desktop-main-edit-refresh
```

The refresh target is a separate Desktop mutation. It refuses the canonical
Desktop destination and refuses to replace a running Main Edit application.
Building or self-testing the package does not authorize refresh or launch.

Canonical Desktop copy/refresh remains restricted to the canonical checkout,
clean `main`, exactly one registered PhysicsSim worktree, and no running
canonical `kinetiC.app`. While retained specialist registrations and the
persistent Main Edit lane exist, use the isolated Main Edit package for
development review instead of weakening that guard.

## Integration gate

Before canonical adoption:

1. classify any canonical-only commits and their ownership;
2. merge expected canonical drift into Main Edit;
3. rerun focused, broad source, and package-identity gates;
4. adopt with a fast-forward when canonical is an ancestor, otherwise use a
   reviewed merge;
5. independently read back both commits and cleanliness.

Source adoption does not authorize a `VERSION` edit, public desktop or worker
package release, signing/notarization, Registry mutation, publication,
deployment, remote execution, activation, or push.

## Retain and recycle gate

Retain the clean persistent Main Edit lane after adoption by default. Recycle
only after proving it is clean, all commits are reachable, no user-owned
untracked or ignored evidence needs retention, no process owns the checkout or
development application, and every specialist registration remains
unaffected. Never force-remove, prune, or destructively reset a lane as part of
ordinary Main Edit work.

## Main Edit icon packaging

The existing product icon is retained at
`tools/packaging/macos/local_app_icon/AppIcon.icns`; this file is deliberately
excluded from the local-icon ignore rule so future source checkpoints and
worktrees retain it. Main Edit packaging requires an icon input. Its self-test
requires the bundled icon, the matching `CFBundleIconFile`, and byte equality
with the selected `.icns` input. Missing icons must fail rather than silently
produce a generic Desktop icon. Other local icon experiments remain ignored.
