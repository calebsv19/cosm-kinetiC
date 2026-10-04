"""Signed force-local residual accounting; diagnostic scores are not error bounds."""
import numpy as np
from cfd_reference3d_mesh import edge_distance
BANDS=np.array([.025,.05,.1,.2,.4,np.inf])


def summarize_signed(volume,faces,adjacency,cell_centers,face_centers,lo,hi,shells):
    """Allocate half each interior jump to both cells; retain original face data."""
    volume=np.asarray(volume,dtype=float);faces=np.asarray(faces,dtype=float);adjacency=np.asarray(adjacency)
    n=cell_centers.shape[1];nf=face_centers.shape[1];nl=len(shells)
    if volume.shape!=(nl,2,3,n) or faces.shape!=(nl,2,3,nf) or adjacency.shape!=(2,nf):
        raise ValueError('invalid signed force dimensions')
    if not np.all(np.isfinite(volume)) or not np.all(np.isfinite(faces)) or not np.all(np.isfinite(cell_centers)) or not np.all(np.isfinite(face_centers)):
        raise ValueError('nonfinite signed force input')
    if not np.issubdtype(adjacency.dtype,np.integer) or np.any(adjacency<0) or np.any(adjacency>=n):
        raise ValueError('invalid interior adjacency')
    cell_net=-volume.copy()
    for lift in range(nl):
        for part in range(2):
            for axis in range(3):
                for side in range(2):np.add.at(cell_net[lift,part,axis],adjacency[side],faces[lift,part,axis]/2)
    expected=faces.sum(axis=-1)-volume.sum(axis=-1)
    np.testing.assert_allclose(cell_net.sum(axis=-1),expected,rtol=1e-11,atol=1e-12)
    total=cell_net.sum(axis=1)
    score=np.max(np.abs(total[:,0]),axis=0)
    distances=edge_distance(cell_centers,lo,hi)
    cell_bands=np.searchsorted(BANDS,distances,side='right')
    face_bands=np.searchsorted(BANDS,edge_distance(face_centers,lo,hi),side='right')
    rows=[]
    for lift,shell in enumerate(shells):
        bands=[]
        for k,upper in enumerate(BANDS):
            ci=cell_bands==k;fi=face_bands==k
            signed=cell_net[lift][:,:,ci]
            bands.append(dict(lower_distance_m=0. if k==0 else float(BANDS[k-1]),upper_distance_m=float(upper) if np.isfinite(upper) else None,
                cell_count=int(ci.sum()),face_count=int(fi.sum()),signed_pressure_n=signed[0].sum(axis=-1).tolist(),signed_viscous_n=signed[1].sum(axis=-1).tolist(),
                signed_total_n=signed.sum(axis=0).sum(axis=-1).tolist(),absolute_cell_total_n=np.abs(total[lift][:,ci]).sum(axis=-1).tolist(),
                absolute_cell_parts_n=np.abs(signed).sum(axis=(0,2)).tolist(),
                weighted_volume_pressure_n=volume[lift,0][:,ci].sum(axis=-1).tolist(),weighted_volume_viscous_n=volume[lift,1][:,ci].sum(axis=-1).tolist(),
                face_jump_pressure_n=faces[lift,0][:,fi].sum(axis=-1).tolist(),face_jump_viscous_n=faces[lift,1][:,fi].sum(axis=-1).tolist()))
        rows.append(dict(shell_m=float(shell),signed_parts_n=expected[lift].tolist(),signed_total_gap_n=expected[lift].sum(axis=0).tolist(),
            absolute_cell_total_n=np.abs(total[lift]).sum(axis=-1).tolist(),absolute_cell_parts_n=np.abs(cell_net[lift]).sum(axis=(0,2)).tolist(),centroid_bands=bands))
    top=np.lexsort((np.arange(n),-score))[:min(24,n)]
    summary=dict(schema='physics_sim_c3d_signed_force_attribution_v1',lifts=rows,
        drag_axis=0,score_sum_n=float(score.sum()),maximum_score_n=float(score.max()) if n else 0.,
        top_cells=[dict(cell=int(i),center_m=cell_centers[:,i].tolist(),edge_distance_m=float(distances[i]),score_n=float(score[i]),signed_total_n=total[:,:,i].tolist()) for i in top],
        allocation='negative weighted element stress divergence plus half each signed interior jump on each adjacent cell; original separate face/volume data retained',
        score_scope='maximum absolute signed drag contribution across both lifts after pressure/viscous sum; diagnostic ranking, not a force-error bound',
        band_scope='centroid bands; half-face cell allocation does not equal a geometrically clipped band identity',physical_accuracy_certified=False)
    return summary,cell_net,score
