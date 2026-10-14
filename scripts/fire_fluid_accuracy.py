#!/usr/bin/env python3
"""Replay immutable Fire amounts to separate plume time and grid errors.

The compact native probe calls the production solver. Binary source/field arrays
are native-endian doubles and are bounded local verification artifacts only.
"""
import argparse,array,hashlib,json,math,subprocess
from pathlib import Path
from surface_sources.receiver import mapping
from surface_sources.growth_fire_v1 import frame_valid,require

def file_sha256(path):
    with Path(path).open('rb') as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def dimensions(n,*,full_grid=False):
    require(type(n) in (int,list,tuple),'integer or rectangular grid required')
    grid=[n]*3 if type(n) is int else list(n)
    require(len(grid)==3 and all(type(v) is int and 8<=v<=128 and v%8==0 for v in grid)
            and math.prod(grid)<=(2097152 if full_grid else 524288),'bounded rectangular probe grid')
    if full_grid:require(grid[0]==grid[1]==grid[2] and grid[0] in (32,64,128),'full-grid cubic qualification profile')
    return grid

def parse_grid(value,*,full_grid=False):
    try:
        parts=[int(v) for v in value.split('x')]
        n=parts[0] if len(parts)==1 else parts
        dimensions(n,full_grid=full_grid)
        return n
    except (ValueError,AssertionError) as e:
        raise argparse.ArgumentTypeError(str(e)) from e

def prepare(frames,n,path,*,full_grid=False):
    grid=dimensions(n,full_grid=full_grid);nx,ny,nz=grid;count=math.prod(grid)
    receiver={'frame':'right_handed_z_up_meters','origin_m':[0.,0.,0.],
              'spacing_m':[1/v for v in grid],'dimensions':[nx,ny,nz//8] if full_grid else grid,'layer':0,
              'fluid_mask':[1]*(nx*ny),'injection_depth_m':.125,
              'injection_fluid_mask':[1]*(nx*ny*(nz//8))}
    # Full-grid qualification maps only the bounded physical source support
    # (128x128x16), then embeds it in the zero-initialized full domain. This
    # preserves the normal receiver admission and identical area/depth weights.
    weights=None
    with path.open('wb') as out:
        for index,frame in enumerate(frames):
            frame_valid(frame)
            require(frame['config']==frames[0]['config'],'fixed source configuration')
            require(math.isclose(frame['interval']['start_s'],index*.1,abs_tol=1e-14)
                    and math.isclose(frame['interval']['end_s'],(index+1)*.1,abs_tol=1e-14),
                    'fixed 0.1-second source windows')
            policy={'schema':'physics_sim_surface_source_policy/v1','consumer_id':'accuracy-probe','model_id':'matched-frozen-fire','source_config':frame['config'],'producer':frame['producer'],'stream_start_tick':0,'receiver':receiver}
            if weights is None:weights=mapping(frame,policy)
            for name in ('energy_transferred_j','smoke_transferred_kg'):
                cells=array.array('d',[0.])*count
                for q,row in enumerate(weights):
                    amount=frame['cells'][name][q];prior=[]
                    for i,(cell,fraction) in enumerate(row):
                        value=amount-math.fsum(prior) if i==len(row)-1 else amount*fraction
                        prior.append(value);cells[cell]+=value
                require(math.isclose(math.fsum(cells),frame['totals'][name],rel_tol=1e-12,abs_tol=1e-20),'source budget')
                cells.tofile(out)
    return {'sha256':file_sha256(path),'bytes':path.stat().st_size,'native_double_size':array.array('d').itemsize}

def fields(path,n,*,full_grid=False):
    a=array.array('d');a.frombytes(path.read_bytes());grid=dimensions(n,full_grid=full_grid);count=math.prod(grid)
    require(len(a)==5*count+sum(math.prod(grid[b] for b in range(3) if b!=axis) for axis in range(3)),'native field dimensions')
    return {'energy_j':a[:count],'smoke_kg':a[count:2*count],'face_velocity_m_s':a[2*count:]}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--frames',type=Path,required=True);p.add_argument('--probe',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--grid',required=True);p.add_argument('--dt',type=float,default=.005);p.add_argument('--box',action='store_true')
    p.add_argument('--scheme',choices=('muscl_minmod','muscl_mc_scalar'),default='muscl_minmod')
    p.add_argument('--full-grid',action='store_true',help='Explicit contained 32/64/128 cubic no-box qualification; ordinary worker limits stay unchanged')
    p.add_argument('--source-input',type=Path,help='Reuse immutable native source arrays instead of copying them')
    a=p.parse_args();a.grid=parse_grid(a.grid,full_grid=a.full_grid)
    if a.full_grid:require(not a.box and a.scheme=='muscl_minmod','accepted no-box solver profile')
    require(math.isfinite(a.dt) and .0005<=a.dt<=.005
                            and abs(.1/a.dt-round(.1/a.dt))<1e-9,'bounded integral substeps')
    root=a.output;root.mkdir(parents=True,exist_ok=False)
    frames=json.loads(a.frames.read_text());require(len(frames)==32,'32 source windows')
    if a.source_input:
        source=a.source_input.resolve()
        require(source.stat().st_size==32*2*math.prod(dimensions(a.grid,full_grid=a.full_grid))*8,'source input dimensions')
        # Bind reused arrays to the selected frozen-frame identity and grid via
        # the original producer receipt, rather than trusting their filename.
        receipt=json.loads((source.parent/'summary.json').read_text())
        identity={'sha256':file_sha256(source),'bytes':source.stat().st_size,'native_double_size':array.array('d').itemsize}
        require(receipt['grid']==a.grid and receipt['source']==identity and receipt['source_frames_sha256']==hashlib.sha256(a.frames.read_bytes()).hexdigest(),'source input receipt identity')
    else:
        source=root/'sources.bin';identity=prepare(frames,a.grid,source,full_grid=a.full_grid)
    with (root/'native.log').open('w') as log:
        command=[str(a.probe.resolve()),(str(a.grid) if type(a.grid) is int else 'x'.join(map(str,a.grid))),str(a.dt),str(int(a.box)),str(source.resolve()),str((root/'fields.bin').resolve())]
        if a.scheme=='muscl_mc_scalar':command.append('mc')
        done=subprocess.run(command,stdout=subprocess.PIPE,stderr=log,text=True,timeout=7200 if a.full_grid else None)
    require(done.returncode==0,'native probe failure; inspect native.log')
    report=json.loads(done.stdout)
    if a.full_grid:
        require(report.get('qualification_profile')=='full_grid_128_v1' and report.get('scalar_transport_scheme')=='muscl_minmod','native full-grid profile identity')
        require(report['memory_limit_bytes']==2147483648 and 0<report['memory_peak_bytes']<=report['memory_limit_bytes'] and report['scalar_work_cells']<=report['max_scalar_work_cells']==8000000000,'qualified resources')
    else:require('qualification_profile' not in report,'explicit full-grid selection required')
    report.update(source=identity,source_path=str(source.resolve()),source_frames_sha256=hashlib.sha256(a.frames.read_bytes()).hexdigest(),probe_sha256=hashlib.sha256(a.probe.read_bytes()).hexdigest(),box=a.box,transport_scheme=a.scheme,fields_sha256=hashlib.sha256((root/'fields.bin').read_bytes()).hexdigest())
    (root/'summary.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report),flush=True)
if __name__=='__main__':main()
