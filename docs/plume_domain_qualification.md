# Explicit tall-domain qualification and physical source support

The private native bridge accepts `--sparse-domain-qualification` separately
from ordinary, eight-second qualification and full-burn movie modes. Python
`validate`, `write_schedule`, `run_sparse` and `read_sample` require the explicit
`domain_qualification=True` selection. It cannot be combined with movie mode.

Domain qualification permits up to 64×64×128 cells (524,288 total), with only
the vertical axis extended beyond 64. It retains qualification limits of 8 s,
3200 steps, 3 samples, one-billion scalar updates, 64 MiB configuration/forcing/
sample packets and a two-hour maximum wall allowance. Numerical allocation
requires an explicit request budget, capped at 512 MiB. A full-resolution
0.2-second 64×64×128 run peaks at 325,609,104 numerical bytes.

The internal atmosphere allocation envelope increases to 524,288 cells; the
ordinary and movie bridge admission limits remain 64 per axis / 262,144 cells.
Numerical equations, floor, projection, transport and warm-temperature gates
are unchanged. Domain receipts use `physics_sim_sparse_domain_receipt/v1`.
Compact accepted fields use `physics_sim_domain_sample_fields/v1`, pass the
same independent SI/source/clock/state/temperature/flux/divergence/conservation
gates, and do not claim ordinary checkpoint restart.

The surface receiver's geometry admission permits 524,288 cells. Its optional
receiver fields `injection_depth_m` and `injection_fluid_mask` select a uniform
physical slab beginning at the declared source layer's lower face. The mask
contains X-fast XY planes for every intersected vertical layer; its first plane
must equal the existing `fluid_mask`. Positive layer overlap divided by physical
depth weights the existing horizontal area intersections. Last-entry remainders
conserve integrated amounts. Depth outside the domain, mask inconsistency,
solid overlap and unresolved precision reject admission. Policies without these
fields retain the original single-layer mapping and digest meaning.

GrowthSim's authored candidate uses 0.0625 m depth, representing one layer at
32³, a fractional second layer at 48³, and two layers at 64³. This is a consistent
source-support candidate, not calibrated flame geometry or spatial convergence.
Three new domain tests cover independent fractional weights, masks, resolution
support, explicit-mode admission and a native 128-cell-tall packet. Existing
movie, receiver, atmosphere, floor and coupled checks pass. GrowthSim additionally
proves full 64×64×128 admission, original VF3D output and exact reproduction of
the accepted movie's 0.2-second control packet. Full-duration domain sensitivity,
open side boundaries, long-domain/full-larger-fire capacity and Linux capability
qualification remain next; no worker package or version changes accompany this.

## Local tall-domain movie envelope

The separate `--sparse-domain-movie` selection and Python `domain_movie=True`
admit a fresh-start local run up to 40 s, 8000 steps and 200 samples on the
same 64×64×128 maximum grid. Exactly one sparse experiment mode may be selected.
The request must explicitly budget numerical allocation (still capped at
512 MiB) and scalar work (capped at eight billion cell updates). Only this mode
admits a forcing file up to 256 MiB and a six-hour wall allowance. Configuration
and sample packet bounds remain unchanged. Previous modes retain their limits.

Receipts use `physics_sim_sparse_domain_movie_receipt/v1`; compact fields use
`physics_sim_domain_movie_sample_fields/v1`. Every decoded sample passes the
existing independent physical acceptance gates. This is an execution envelope,
not a calibrated hot-combustion model or checkpoint-resume API.

The dedicated local tests prove byte-identical short tall-domain states across
the old and new modes, a 40-second small-grid conservative run, rejection by the
old profiles, and rejection beyond the new duration/step limits. Existing domain,
qualification, atmosphere and native transport checks also pass. The larger
center-source 40-second run is recorded separately by GrowthSim; this envelope's
unit and regression checks alone do not claim that workload has completed.
