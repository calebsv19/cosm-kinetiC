"""Exact matched diagnostic splitting local physical action and packed-sweep cost."""
import argparse,json
from pathlib import Path
from unittest.mock import patch
import cfd_reference3d_packed_matched_profile as original
from cfd_reference3d_local_cost_split_action import profiled_inner8
from cfd_reference3d_shared_factor import storage_sha
from cfd_reference3d_domain_budget import PhaseResourceStopped

def run(factor_library,coarse_library,snapshot):
    saved=original.profile_actions;base_reserve=original.diagnostic_reserve
    def profile(factor,precondition,rhs,nv,check):
        expected=storage_sha(precondition(rhs));measurements=[]
        def inverse(x):
            result,stats=profiled_inner8(factor,x);measurements.append(stats);return result
        with patch.object(factor.balanced,'inverse',inverse):row=saved(factor,precondition,rhs,nv,check)
        if row['per_load'][0]['output_sha256']!=expected:raise ValueError('native timing altered original mixed response')
        assert len(measurements)==15
        samples=measurements[1:];totals={k:sum(x[k] for x in samples) for k in samples[0]};row['native_local_split']=dict(samples=samples,totals=totals,timed_calls=14,baseline_original_rhs_bitwise_match=True,scope='clock calls change timing only; physical matrix and mathematical recurrence unchanged')
        return row
    with patch.object(original,'profile_actions',profile),patch.object(original,'diagnostic_reserve',lambda n:base_reserve(n)+2**20):return original.run(factor_library,coarse_library,snapshot)
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--factor-library',type=Path,required=True);ap.add_argument('--coarse-library',type=Path,required=True);ap.add_argument('--snapshot',type=Path,required=True);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args();assert not a.output.exists() and not a.snapshot.exists()
    try:row=run(a.factor_library,a.coarse_library,a.snapshot)
    except PhaseResourceStopped as e:row=dict(diagnostic_accepted=False,numerically_accepted=False,flow_field_published=False,resource_phase_rejected=e.record)
    a.output.write_text(json.dumps(row,indent=2)+'\n');print(json.dumps({k:row.get(k) for k in ('diagnostic_accepted','resource_phase_rejected','wall_s','diagnostic_factor_owners_retired')}),flush=True);raise SystemExit(0 if row['diagnostic_accepted'] else 2)
