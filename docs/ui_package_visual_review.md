## 2026-10-13 — UI source checkpoint adopted; Main Edit ready

The completed active UI rollout and retained Fire–Fluid source are checkpointed
at `336a741d94458e0aebfd7fa58bbb0a5a5dcd975f` and fast-forward adopted into canonical `main`. The persistent
Main Edit checkout remains the development owner. This supersedes earlier
uncommitted/adoption-pending status entries below.

Fresh Clang build, ten focused UI/theme/package targets, all 16 fast and 62 stable
targets, headless CLI and thirteen retained coupling native targets pass. The
existing local package/visual qualification remains documented separately.
Coupling source and frozen workers/evidence are preserved; this checkpoint does
not close the open numerical accuracy work. Installed/user/platform acceptance
and legacy structural retirement remain separate. Program/worker versions stay
0.5.0/0.4.0; source adoption does not publish a release. PhysicsSim functional
edits can continue in the retained Main Edit lane.

# PhysicsSim local UI package and visual qualification

Date: 2026-10-13. Retained Main Edit, source VERSION 0.5.0. This is local macOS
arm64 development qualification; canonical adoption, installed refresh and public
release remain separate effects.

The active shared UI rollout covers menu/settings, preset metadata/actions, the
precision editor, simulation window/HUD and F8 live inspection. Shared lifecycle,
matched controls, measured text and theme/font roles retain app-owned drafts,
saves, canvases, simulation and async session policy. Scientific colors and
rectangular control/HUD geometry remain intentional. The imported shared pin and
minimum versions are unchanged.

Final visual refinements use a stronger themed HUD backdrop (alpha 224) over bright
fields and event-owned keyboard modifiers for reliable quick Cmd/Ctrl text-size
shortcuts. The latter preserves the modifier at key press rather than sampling it
after a rapid release. A regression covers press/release, unmodified keys and repeats.

Real packaging found and repaired two launcher defects: unique MoltenVK driver
manifest generations now contain a .json filename accepted by the loader, and new
TimerHUD output defaults to the runtime directory instead of the signed app bundle.
Explicit driver and timing-root overrides are retained. The Main Edit self-test
fixture copies its three resource lanes, so its scratch inventory stays contained;
production resource links and strict bundle/link validation are unchanged.

Fresh Clang application build, focused UI/theme/package contracts, the rapid
shortcut regression, all 16 fast targets, 38 actual launcher lifecycle tests and
14 package-proof tests pass. The preceding integration checkpoint retains all 62
stable targets, 9 real-worker tests and 80 native lifecycle stages; numerical code
is unchanged in this visual/package slice. The corrected packaged Vulkan launcher
passes startup, resize and renderer restart on Apple M2 at 2x drawable scale, with
validation requested/enabled and zero warnings/errors.

Hands-on review binds the exact packaged process path before computer use. Review
used disposable runtime, preset, session and log roots. It confirms settings draft
and revert, dimension input/save, precision cancel/return, F8 worker creation,
Continue/Pause/Step/Cancel, vectors/probing/text size, and interactive Wind in 2D
and the controlled 3D lane. Quick runtime zoom/reset persists correctly. Captures
show contained labels and readable rectangular HUDs. The package signature verifies
after normal use and exit. The real F8 review run is terminal at cancelled tick 118;
its values establish UI/control transport, not solver accuracy.

Local active UI integration and packaged visual qualification are complete. User
acceptance and installed Main Edit refresh remain next. Existing TimerHUD settings
may contain an absolute log_filepath into an older app/build; that user override
must be checked during installed-refresh preflight rather than silently rewritten.
Other platforms/monitors and glyph/platform IME coverage are not inferred from this
Mac. Structural remains legacy code/routes, excluded from active UI priorities;
retirement is a separate bounded product cleanup. No commit, canonical adoption,
Desktop replacement, VERSION edit, publication or Registry change is included.
