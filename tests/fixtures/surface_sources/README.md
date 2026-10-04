# Source fixtures

Prescribed/zero frames are copied from GrowthSim's contract known answers at
`1e24d710f23d0532b964799c1567923069b41b04`. They intentionally pin zero producer hashes.

`native-bundle.json` is the first three-tick native interval from GrowthSim's
`run_physics_surface_contract.py` general-atmosphere acceptance run. The bundle
contains its worker/adapter provenance, sealed configuration and native checkpoint.
It is a retained regression input, not evidence of authenticated remote input or
qualified combustion physics. The receiver test rejects a resealed bundle with a
corrupt native checkpoint hash, and confirms bundle/frame replay equivalence.
