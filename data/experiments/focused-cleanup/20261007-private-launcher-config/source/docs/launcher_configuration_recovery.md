# Packaged launcher configuration and recovery

Source implementation adopted in canonical on 2026-10-07. This does not refresh
an installed app or establish GUI acceptance on macOS or Linux.

Both launchers answer `--print-config` before runtime initialization. The printed
paths are requested paths, not a writability or application-startup proof.
Ordinary startup initializes private writable configuration under the selected
runtime directory. Package configuration is a read-only input.

## First initialization and reruns

The shell helper `Resources/runtime-config.sh` on macOS, or
`resources/runtime-config.sh` on Linux, takes an initialization directory lock.
It retains a fresh attempt under `<runtime>/.config-initialization-attempts/`,
copies configuration to that attempt's candidate, and compares complete file
hash/directory inventories before publication. Source configuration is checked
again after copying. Inventories admit at most 10,000 entries and 64 MiB of file
contents, and refuse links, special files and control characters in names.
The helper requires `sha256sum` or `shasum`; it does not require Python.

A completion marker travels inside the verified candidate when it is renamed
to `<runtime>/config`. Failed copies do not create an active configuration.
Failed candidates and inventories remain available; a later ordinary attempt
can initialize afresh when no final configuration or held lock exists. Existing
marked configuration is reused so application saves and user edits survive.
The initial hashes are publication evidence, not an instruction to restore user
configuration to factory bytes on every launch.

A legacy macOS link that names this package's configuration is migrated to a
private copy. The original link is moved into the retained attempt, and the
package bytes remain unchanged. A foreign configuration link is refused.

## Existing unmarked configuration

An old directory without a completion marker could be legitimate user state or
an interrupted copy. Startup preserves it and refuses to guess. Inspect its
contents and the package configuration before deliberately adopting it:

```sh
PHYSICS_SIM_RUNTIME_DIR="/absolute/path/to/runtime" \
  "/absolute/path/to/package/launcher" --adopt-existing-config
```

Use the actual packaged launcher path (`Contents/MacOS/physics-sim-launcher` or
`bin/physics-sim-launcher`). Adoption requires the package's full path inventory,
preserves edited contents and extra regular files, records the observed inventory
and creates the marker without starting the app. Missing paths, linked/special
files and size/count violations hold adoption. Do not use adoption merely to
silence a warning about a directory you have not reviewed. Keep any partial
directory as evidence while preparing a separate complete configuration.

## Abrupt termination and held locks

Ordinary failures release only the invocation's empty lock directory. An abrupt
initializer death can leave `<runtime>/.config-initialization-lock`; the next
attempt holds rather than accepting a partial candidate or deleting a guessed
stale lock. Attempts and the original configuration remain retained.

Recovery of that hold is operator-supervised: first establish that no launcher
initializer is active for this runtime, then inspect its retained request,
inventories, candidate and any `legacy-config-link`. Preserve the lock and failed
attempt before releasing the hold, and retain the existing configuration rather
than recursively resetting the runtime. If ownership is uncertain, leave the
hold in place. There is no automatic stale-PID recovery or power-loss guarantee.

## Evidence and limits

`make test-package-launcher-lifecycle` runs 37 methods without compiler setup.
Disposable packages exercise the real shell launchers and helper, native macOS
plist tools, stub application saves, normal reruns, legacy adoption/migration,
failed and incomplete copies, failed publication and abrupt initializer death.
Both actual package-copy recipe lines are exercised in disposable roots.

This proves selected source behavior. Full package builds, signing, installed
profile migration, real GUI startup and Linux desktop-session acceptance remain
separate. Shared/shader resource initialization, aggregate history retirement,
hostile concurrent path replacement and full power-loss recovery are outside this
configuration repair. Main Edit retains its independent state and needs deliberate
reconciliation before it can supersede this canonical launcher implementation.
