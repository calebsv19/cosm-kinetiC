# Offline coupling in the Linux worker package

The Linux worker recipe includes the native session, passive, evolving and open
atmosphere workers in addition to the existing headless and job-runner binaries.
`bin/physics_sim_coupling` carries the explicit Python adapter closure and checks
`coupling_payload.json` before importing it. Python 3.11+, json-c and the declared
native runtime libraries are required. All six native binaries are checked for
ELF architecture and the declared GLIBC ceiling by the package validator.

The offline CLI capabilities are separate from coordinator job capabilities.
The existing `trio_headless_stage` job protocol is unchanged. A native session
binary is included; this does not add a packaged MCP session service.

```sh
/path/to/package/bin/physics_sim_coupling capabilities
/path/to/package/bin/physics_sim_coupling --runtime /absolute/external/runtime \
  periodic --config /absolute/config.json --store /absolute/external/receiver init
/path/to/package/bin/physics_sim_coupling --runtime /absolute/external/runtime \
  periodic --config /absolute/config.json --store /absolute/external/receiver \
  admit /absolute/source-bundle.json
/path/to/package/bin/physics_sim_coupling --runtime /absolute/external/runtime \
  periodic --config /absolute/config.json --store /absolute/external/receiver \
  step --revision 0 --operation-id first --dt 0.05
```

`periodic`, `open` and `passive` select the existing coupled adapters. Their
config contracts remain unchanged. The configuration `worker` must name the
matching binary inside this exact package; arbitrary external workers are
refused. Experiment evidence and receiving stores must stay outside the package.
The package execution lifecycle holds inherited descriptors through native
capture and checks bytes again before acceptance. Source-checkout build ownership
continues to use its existing lifecycle.

For package-only coupling, conservation, exact replay/restart, refusal, native
parent-death recovery and VF3D scene qualification:

```sh
python3 -B tests/qualify_fire_coupling_packages.py \
  --growth-package /absolute/extracted/change-fire-source \
  --physics-package /absolute/extracted/physics-worker \
  --output /absolute/new-proof-directory
```

The proof requires process visibility for its SIGTERM/SIGKILL child-teardown
checks, and uses only the selected packages for simulation and scene operations.
The demonstration conserves authored energy/smoke but uses synthetic calibration;
physical Fire accuracy and bidirectional feedback are not qualified.

The Linux build must use the sanctioned native/portable executor and the exact
release packet. Mac-assembled development payloads are not Linux release
artifacts. Authentication, Registry promotion, installation and activation remain
the separate release stages.
