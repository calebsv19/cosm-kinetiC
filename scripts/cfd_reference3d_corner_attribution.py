"""Signed component force-gap tracing from immutable accepted observations."""
import argparse
import json
from pathlib import Path
import numpy as np
from audit_cfd_3d_spatial import verify_receipt
from audit_cfd_3d_graded import sha
ROOT=Path(__file__).resolve().parents[1]


def trace_lift(lift,reaction):
    pressure=lift['pressure'];viscous=lift['viscous'];parts={}
    for name,part in (('pressure',pressure),('viscous',viscous)):
        jump=np.array(part['interior_jump_centroid_buckets_n']);volume=np.array(part['volume_divergence_centroid_buckets_n'])
        assert jump.shape==volume.shape==(6,3)
        signed=jump-volume
        np.testing.assert_allclose(signed.sum(axis=0),part['raw_minus_weak_n'],rtol=0,atol=1e-9)
        parts[name]=signed
    total=parts['pressure']+parts['viscous']
    raw=np.array(pressure['raw_surface_load_n'])+viscous['raw_surface_load_n']
    weak=np.array(pressure['weak_load_n'])+viscous['weak_load_n']
    np.testing.assert_allclose(weak,reaction,rtol=0,atol=1e-9)
    np.testing.assert_allclose(total.sum(axis=0),raw-reaction,rtol=0,atol=1e-9)
    return dict(shell_m=lift['shell_m'],pressure_signed_centroid_buckets_n=parts['pressure'].tolist(),
        viscous_signed_centroid_buckets_n=parts['viscous'].tolist(),total_signed_centroid_buckets_n=total.tolist(),
        net_raw_minus_reaction_n=(raw-reaction).tolist(),nearest_centroid_bucket_to_net_x_ratio=float(total[0,0]/(raw[0]-reaction[0])),
        scope='exact signed jump-minus-volume decomposition; centroid bands are not clipped physical bands; component weak splits depend on lift')


def observe(path):
    path=path.resolve();receipt_path=path.with_name(path.stem+'-receipt.json');receipt=json.loads(receipt_path.read_text())
    assert receipt['returncode']==0 and receipt['stop_reason'] is None and receipt['diagnostic_failure'] is None
    assert receipt['artifact_sha256'][str(path)]==sha(path)
    for p,h in receipt['artifact_sha256'].items():assert sha(Path(p))==h
    for n,h in receipt['source_sha256'].items():assert sha(receipt_path.parent/'source'/n)==h
    row=json.loads(path.read_text());assert row['diagnostic_accepted'] and row['input_complete_numerical_gates_passed']
    source=Path(row['input_receipt']);assert sha(source)==row['input_receipt_sha256']
    original=verify_receipt(source)[1];assert original['numerically_accepted']
    traced=[trace_lift(lift,np.array(original['reaction_force_n'])) for lift in row['lifts']]
    return dict(observer_receipt=str(receipt_path),observer_receipt_sha256=sha(receipt_path),observer_sha256=sha(path),
        input_receipt_sha256=sha(source),input_snapshot_sha256=row['input_snapshot_sha256'],
        centroid_bucket_upper_distances_m=row['centroid_bucket_upper_distances_m'],lifts=traced,
        numerical_input_accepted=True,physical_accuracy_certified=False)


def run():
    records={}
    for family,name in (('cholesky','body4-normal-stress'),('domain','L8-held-normal-stress'),('selective','L8-held-outer-select2-stress')):
        path=next((ROOT/f'build/c3d-{family}/observer-runs').glob('*/'+name+'.json'))
        records[name]=observe(path)
    return dict(schema='physics_sim_c3d_corner_attribution_v1',physical_accuracy_certified=False,source_sha256=sha(Path(__file__)),records=records,
        interpretation='nearest .025-m centroid edge-distance bucket dominates the signed net gap; far weighted indicators alone did not guide useful force refinement',
        force_replaced=False,scope='accepted-field postprocessing; not a force-error bound or new solved field')


if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--output',type=Path,required=True);a=ap.parse_args()
    assert not a.output.exists();a.output.write_text(json.dumps(run(),indent=2)+'\n')
