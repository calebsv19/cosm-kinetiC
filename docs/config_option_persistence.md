# Configuration option persistence and numeric roundtrip

Main Edit saving now includes the nine previously readable but omitted options:
fixed timestep and maximum steps per frame; stroke sample rate and spacing;
emitter density, velocity and sink multipliers; volume-frame and render-frame
export flags. The existing canonical JSON keys are used. Missing input fields
still load defaults; legacy aliases still load and save through canonical keys.
Collider logs persist through the canonical debug section. No file/schema version,
shared module API or dependency upgrade is introduced.

Numeric serialization uses nine significant digits for stored float values and
seventeen for double values, replacing six fixed decimal places for floats and
nine decimal places for timing doubles. This preserves small nonzero values and
native representations across load/save/load in the tested platform contract.
Previously valid values such as a small diffusion coefficient could save as zero;
higher precision inflow or timing values could silently change. Existing staged
JSON validation and predecessor-preserving publication remain in force, including
refusal of nonfinite/invalid candidates and unconfirmed post-rename semantics.

Four compiled native persistence methods pass. An independently specified
nondefault configuration exercises all 52 distinct AppConfig members consumed by
the current file contract, comparing scalar and string values across actual
load/save/load. Additional cases cover small nonzero float/double values, legacy
aliases/defaults and false/mixed export choices. The known parser has 59 field
spellings because aliases and collider/debug log spellings share members. This
52-member claim does not cover additional AppConfig members outside the readable
file contract or arbitrary domain-invalid values that existing logic clamps.

Forty existing configuration JSON/read/save/native persistence methods also pass
(forty-four distinct methods total). The actual Make configuration roundtrip passes
in `build/profiles/config-options-20261007`. Retrospective native probes compile the
retained prior JSON-contract writer under the same test harness. It fails the
omission case on physics_fixed_dt and the precision case on tunnel_inflow_speed;
the current writer passes both. The baseline source SHA-256 is
f97e53ef2714789b3d5a7b5917c36ea0a17fa0c26af5498b39a57578a40e123e.
This demonstrates that the regression probes detect the repaired behavior rather
than merely accepting both implementations. All inputs and output files are
disposable; no user's runtime settings were rewritten.

Physical ranges, cross-field consistency, unsupported AppConfig option coverage,
locale-independent successful numeric formatting, allocation-failure campaigns,
full worker/resource/recovery/retirement control and canonical adoption remain
open. Invalid locale-generated numeric JSON holds at staged validation rather
than publishing, but locale behavior is not newly qualified here. Native tests
do not establish installed package, cross-host or CFD physical acceptance.

This is app-owned configuration serialization, reusing existing strict JSON and
persistence adapters. No shared API/version or adoption minimum changed. No
commit, canonical adoption, cleanup, pruning, package, installation or release
occurred. Evidence is local and outside the frozen backup scopes. The overall
lifecycle goal remains incomplete.

Evidence: `data/experiments/lifecycle-validation/20261007-config-option-persistence`.
