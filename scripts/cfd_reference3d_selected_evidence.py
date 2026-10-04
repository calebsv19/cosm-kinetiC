"""Read-only immutable encoded-action receipts; full physical gates unchanged."""
import json,hashlib
from pathlib import Path
R=Path(__file__).resolve().parents[1];D=R/'build/c3d-size-selected'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def frozen(p):
    r=json.loads(p.read_text());assert r['mesh_cap']==50000 and r['rss_cap_bytes']==1800*1024**2 and r['wall_cap_s']==180 and r['linear_iteration_cap']==3000
    for q,h in r['artifact_sha256'].items():assert sha(Path(q))==h
    for q,h in r['source_sha256'].items():assert sha(p.parent/'source'/q)==h==sha(R/'scripts'/q)
    assert p.parent.name==hashlib.sha256(json.dumps(r['source_sha256'],sort_keys=True).encode()).hexdigest()
    assert sha(D/'supervisor-source'/(r['runner_sha256']+'.py'))==r['runner_sha256']
    if 'factor_build' in r:
        b=r['factor_build'];assert sha(p.parent/'source/factor.dylib')==r['factor_library_sha256']==b['library_sha256']
        assert sha(p.parent/'source/factor-build.json')==r['factor_build_record_sha256']
        assert b['source_sha256']==r['source_sha256']['cfd_reference3d_encoded_storage.c']
        assert all(x in b['command'] for x in ('-std=c11','-Wall','-Wextra','-Werror'))
    row=json.loads(Path(r['command'][r['command'].index('--output')+1]).read_text());return r,row
