# Bounded local tool identity probes

Build configuration and doctor share `scripts/tool_probe.py`. Compiler version
and macOS SDK selection probes run without a shell, with a 15-second wall limit
and a combined 64-KiB capture limit. Output is truncated at the limit before the
probe is held. Nonzero exit, excessive output, timeout and launch failures cannot
produce verified build identity.

The runner terminates the process group it owns on completion or failure. This
includes a successful parent whose child closed the captured pipes and kept
running. Commands must not use identity probes to start persistent services.
These are trusted local tools, not a sandbox for hostile wrappers; escaped
process sessions and arbitrary executable side effects remain outside the
cooperative contract.

Compiler probes execute the resolved selected binary. Its byte/file fingerprint
must match before and after the probe. macOS xcrun selection has the same check;
its resolved path and hash participate in build configuration. The shared probe
helper itself also participates in configuration identity. FISICS optional file
identity remains distinct from executing a FISICS qualification probe.

```sh
make test-tool-probe
make test-build-identity
make doctor
```

Validation covers output and time bounds, invalid bounds, nonzero exit,
successful-parent descendant cleanup, executable mutation during version output,
and the actual configuration CLI refusing a noisy compiler without allocating
build or metadata roots. Doctor, configuration admission, incremental selection,
concurrent roots and reference environment setup checks passed. The supported
headless build and both first-proof fixtures passed with the final helper.
Reference-profile doctor ran read-only without creating its selected build root.

Full transitive SDK/library provenance, signatures, installed product freshness,
numerical acceptance and independent backup recovery are separate evidence.
