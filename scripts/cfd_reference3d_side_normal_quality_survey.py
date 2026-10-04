"""Prospective side-normal thinning with declared bounded anisotropy and physical gates."""
import json
from pathlib import Path
import resource
import time
import numpy as np
from cfd_reference3d_directional_field import build as parent_build
from cfd_reference3d_mesh import refined_octant
from cfd_reference3d_p3 import alfeld_split
from cfd_reference3d_edge_redistribution_mesh import quality
from cfd_reference3d_translated_cells import compare_cells, inner_cells
from run_cfd_native_accuracy_regression import save, sha

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'build/c3d-side-normal-quality'
PROFILES = dict(side023=.0234375, side016=.015625)
PARENT_GEOMETRY = ROOT/'build/c3d-directional-quality/geometry.npz'
QUALITY_RATIO = 2.1
MAX_CONDITION = 400.
MAX_SHAPE = 90.


def main():
    DEST.mkdir(exist_ok=False)
    start = time.monotonic(); arrays = {}; rows = []
    names = (Path(__file__).relative_to(ROOT).as_posix(),
             'scripts/cfd_reference3d_directional_field.py',
             'scripts/cfd_reference3d_accuracy_graded_mesh.py',
             'scripts/cfd_reference3d_mesh.py', 'scripts/cfd_reference3d_p3.py',
             'scripts/cfd_reference3d_edge_redistribution_mesh.py',
             'scripts/cfd_reference3d_translated_cells.py')
    hashes = {n:sha(ROOT/n) for n in names}
    save(DEST/'contract.json', dict(source_sha256=hashes, profiles=PROFILES,
        mesh_cap=120000, rss_cap_bytes=2048*1024**2, wall_cap_s=600,
        regional_shape_condition_max_ratio=QUALITY_RATIO,
        absolute_max_condition=MAX_CONDITION,absolute_max_shape=MAX_SHAPE,
        parent_geometry=str(PARENT_GEOMETRY),parent_geometry_sha256=sha(PARENT_GEOMETRY),
        policy_reason='Prospective accuracy experiment allows moderate wall-layer anisotropy; original equations, force/numerical/resource acceptance remain unchanged. Previous rejected geometry is not reclassified.', numerical_factor_attempted=False,
        prospective_physical_field_condition='Paired geometry must pass; original L4 raw gap must improve at least 10 percent before L8 solve.'))
    for length in (4., 8.):
        parent = parent_build(PARENT_GEOMETRY,length); _, lo, hi, axes, _ = parent
        before = quality(parent[0], lo, hi, length)
        for name, spacing in PROFILES.items():
            changed = [a.copy() for a in axes]
            for axis in (1,2):
                for side, plane in enumerate((lo[axis],hi[axis])):
                    old = plane + (1 if side else -1)*.03125
                    mask = np.isclose(changed[axis],old,rtol=0,atol=1e-12)
                    assert mask.sum()==1
                    changed[axis][mask] = plane + (1 if side else -1)*spacing
            assert all(np.all(np.diff(a)>0) for a in changed)
            macro, _ = refined_octant(changed, np.array([length,2.,2.]), lo, hi, 0)
            mesh = alfeld_split(macro)
            def body(x):
                return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
            mesh = mesh.with_boundaries(dict(inlet=lambda x:np.isclose(x[0],0),
                outlet=lambda x:np.isclose(x[0],length), walls=lambda x:np.isclose(x[1],0)|np.isclose(x[1],2)|np.isclose(x[2],0)|np.isclose(x[2],2), body=body))
            after = quality(mesh, lo, hi, length); reasons = []; ratios = {}
            if mesh.nelements>120000: reasons.append('mesh cap')
            if not after['physical_boundary_planes_preserved']: reasons.append('physical planes')
            if not after['all_reflections_and_yz_exchange_preserved']: reasons.append('actual cell symmetry')
            if abs(after['global_quality']['volume_m3']-(4*length-1))>1e-9: reasons.append('fluid volume')
            if any(abs(after['boundary_areas_m2'][k]-v)>1e-9 for k,v in dict(body=6.,inlet=4.,outlet=4.,walls=8*length).items()): reasons.append('boundary area')
            for region in ('global','edge-0.025','edge-0.05','edge-0.1','body-0.05'):
                a = before['global_quality'] if region=='global' else before['bands'][region]
                b = after['global_quality'] if region=='global' else after['bands'][region]
                ratios[region] = {k:b[k]/a[k] for k in ('worst_shape','max_condition')}
                for k, v in ratios[region].items():
                    if v>QUALITY_RATIO: reasons.append(region+' '+k+' exceeds prospective anisotropy bound')
            if after['global_quality']['max_condition']>MAX_CONDITION: reasons.append('absolute conditioning bound')
            if after['global_quality']['worst_shape']>MAX_SHAPE: reasons.append('absolute shape bound')
            prefix = f'L{int(length)}_{name}_'
            arrays.update({prefix+'vertices_m':mesh.p, prefix+'tetrahedra':mesh.t,
                           prefix+'lo':lo, prefix+'hi':hi, prefix+'macro_tetrahedra':np.array(macro.nelements)})
            arrays.update({prefix+'axis'+str(i):a for i,a in enumerate(changed)})
            row = dict(length=length, profile=name, tetrahedra=mesh.nelements,
                       parent_quality=before, quality=after, quality_ratios=ratios,
                       geometry_screen_passed=not reasons, reasons=reasons)
            rows.append(row)
            print(json.dumps({k:row[k] for k in ('length','profile','tetrahedra','geometry_screen_passed','reasons')}),flush=True)
            assert time.monotonic()-start<600 and resource.getrusage(resource.RUSAGE_SELF).ru_maxrss<2048*1024**2
    paired = {name:compare_cells(inner_cells(arrays,'L4_'+name+'_'),inner_cells(arrays,'L8_'+name+'_')) for name in PROFILES}
    with (DEST/'geometry.npz').open('xb') as f: np.savez_compressed(f, **arrays)
    for name,h in hashes.items(): assert sha(ROOT/name)==h
    accepted = [name for name in PROFILES if paired[name]['cells_match'] and
                all(r['geometry_screen_passed'] for r in rows if r['profile']==name)]
    save(DEST/'assessment.json', dict(status='completed_directional_geometry_screen',
         source_sha256=hashes, geometry_sha256=sha(DEST/'geometry.npz'), rows=rows,
         actual_inner_cell_comparison=paired, accepted_paired_profiles=accepted,
         wall_s=time.monotonic()-start, peak_rss_bytes=resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
         numerical_factor_attempted=False, flow_field_published=False, physical_accuracy_certified=False))
    print(json.dumps(dict(status='completed_directional_geometry_screen',accepted_paired_profiles=accepted)),flush=True)

if __name__=='__main__': main()
