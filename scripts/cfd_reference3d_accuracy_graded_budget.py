"""Accuracy-first bounded local contract; original physical gates preserved."""
import resource,time
import numpy as np
from cfd_reference3d_domain_budget import PhaseResourceStopped
from cfd_reference3d_residency_owners import fresh_admission as original_admission
RSS_CAP=8192*2**20
WALL_CAP=1800.
def enforce_phase(phase,peak_rss_bytes,wall_s):
    if not np.isfinite(wall_s) or wall_s<0 or not isinstance(peak_rss_bytes,(int,np.integer)) or peak_rss_bytes<0:raise ValueError('invalid phase resource observation')
    if peak_rss_bytes>=RSS_CAP or wall_s>=WALL_CAP:raise PhaseResourceStopped(dict(phase=phase,peak_rss_bytes=int(peak_rss_bytes),wall_s=float(wall_s),rss_cap_bytes=RSS_CAP,wall_cap_s=WALL_CAP))
def fresh_admission(factor,outer_bytes,coarse_bytes,rss_reader=None):
    kwargs={} if rss_reader is None else {'rss_reader':rss_reader}
    row=original_admission(factor,outer_bytes,coarse_bytes,**kwargs)
    row['numeric_stage_admitted']=row['estimated_numeric_stage_bytes']<=RSS_CAP
    row['rss_cap_bytes']=RSS_CAP;row['wall_cap_s']=WALL_CAP
    return row

def numerical_failure_reasons(row):
    checks={
        'linear_residual': row['info']==0 and np.isfinite(row['final_residual']['true_residual']) and row['final_residual']['true_residual']<row['target'],
        'flux': np.isfinite(row['flux_error']) and row['flux_error']<1e-8,
        'volume_divergence': np.isfinite(row['volume_divergence_max_s_inv']) and row['volume_divergence_max_s_inv']<1e-8,
        'energy': np.isfinite(row['physical_energy_imbalance']) and row['physical_energy_imbalance']<.03 and row['physical_dissipation_w']>0,
        'resources': row['tetrahedra']<=120000 and row['iterations']<=3000 and row['peak_rss_bytes']<RSS_CAP and row['wall_s']<WALL_CAP}
    return [name for name,passed in checks.items() if not passed]

def publish_snapshot(snapshot,fields,row,started):
    row['numerical_failure_reasons']=numerical_failure_reasons(row)
    row['numerically_accepted']=not row['numerical_failure_reasons']
    if not row['numerically_accepted']:return False
    assert not snapshot.exists()
    pending=snapshot.with_name(snapshot.name+'.pending.npz')
    assert not pending.exists()
    np.savez_compressed(pending,**fields,numerically_accepted=True)
    row['peak_rss_bytes']=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    row['wall_s']=time.monotonic()-started
    row['numerical_failure_reasons']=numerical_failure_reasons(row)
    row['numerically_accepted']=not row['numerical_failure_reasons']
    if not row['numerically_accepted']:
        pending.unlink()
        return False
    pending.rename(snapshot)
    return True
