# Physics Sim Visual Artifact

`make -C physics_sim visual-artifact` is the R6 source-run visual proof route.
It builds `physics_sim_headless`, runs a checked-in Wind runtime-scene fixture,
writes a Wind projection BMP, validates that the BMP is nonblank, and prints
the final artifact path.

Expected success line:

```text
physics_sim visual artifact: <path>/physics_sim_wind_projection_first_frame.bmp
```

Default artifact root:

```text
physics_sim/visual_artifacts/source_first_frame/
```

Generated files in that root include:

- `physics_sim_wind_projection_first_frame.bmp`
- `visual_artifact_report.json`
- `run/run_summary.json`
- `run/wind_shot_manifest.json`
- `run/wind_analysis_timeseries.jsonl`

The `visual_artifacts/` root is ignored and should not be bundled into
`kinetiC.app`. This proof does not use desktop capture, package launch, remote
workers, or private machine scene paths.

`visual-harness` remains build-only readiness. Use `visual-artifact` when an
actual inspectable image artifact is required.
