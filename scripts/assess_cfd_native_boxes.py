"""Read-only physical refinement and independent complete-binary box load checks.

No fitted force, pressure gauge adjustment or absolute force certification.
"""
import argparse
import json
from pathlib import Path
import struct
import numpy as np
from run_cfd_native_accuracy_regression import require, sha
ROOT=Path(__file__).resolve().parents[1]

def verify(path):
 r=json.loads(path.read_text());directory=path.parent
 require(r['status']=='completed_native_box_with_complete_SI_readback','Terminal field')
 for q,h in r['artifact_sha256'].items():require(sha(directory/q)==h,'Artifact identity '+q)
 for q,h in r['source_sha256'].items():require(sha(ROOT/q)==h,'Current solver source '+q)
 c=r['control'];binary=directory/'field.bin'
 with binary.open('rb') as f:
  require(f.read(8)==b'C3DBOX1\0','Complete field schema')
  h=struct.unpack('=11i',f.read(44));parameters=struct.unpack('=7d',f.read(56))
 require(list(h[:3])==c['grid'] and h[9]==c['faces'] and h[10]==c['fluid_cells'],'Counts')
 require(binary.stat().st_size==108+8*(h[9]+h[10]),'Complete field bytes')
 values=np.memmap(binary,dtype='=f8',mode='r',offset=108,shape=(h[9]+h[10],))
 require(np.isfinite(values).all(),'Complete finite field')
 shape=tuple(reversed(h[:3]));lo=np.array(h[3:6]);hi=np.array(h[6:9])
 spacing=np.array(parameters[:3])/np.array(h[:3])
 np.testing.assert_allclose(lo*spacing,c['lower_m'],rtol=0,atol=1e-12)
 np.testing.assert_allclose(hi*spacing,c['upper_m'],rtol=0,atol=1e-12)
 z,y,x=np.ogrid[:shape[0],:shape[1],:shape[2]]
 solid=(x>=lo[0])&(x<hi[0])&(y>=lo[1])&(y<hi[1])&(z>=lo[2])&(z<hi[2])
 pressure=np.full(shape,np.nan);pressure[~solid]=values[h[9]:]
 integrated=[]
 for axis in range(3):
  other=[a for a in range(3) if a!=axis]
  area=float(np.prod((hi-lo)[other]*spacing[other]));loads=[]
  for side in (0,1):
   available=int(lo[axis] if side==0 else h[axis]-hi[axis])
   depth=min(4,available);require(depth>=2,'Normal pressure intervals')
   weights={2:np.array([1.5,-.5]),3:np.array([11,-7,2])/6,
            4:np.array([25,-23,13,-3])/12}[depth]
   means=[]
   for d in range(depth):
    selector=[slice(int(lo[a]),int(hi[a])) for a in reversed(range(3))]
    selector[2-axis]=int(lo[axis]-1-d if side==0 else hi[axis]+d)
    means.append(float(pressure[tuple(selector)].mean()))
   loads.append(float(weights@means)*area)
  integrated.append(loads[0]-loads[1])
 np.testing.assert_allclose(integrated,c['diagnostic_pressure_force_n'],rtol=0,atol=1e-12)
 return c,dict(receipt=str(path.resolve()),receipt_sha256=sha(path),field_sha256=sha(binary),
               independent_pressure_force_n=integrated,physical_absolute_force_certification=False)

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--root',type=Path,default=ROOT/'build/c3d-box/runs')
 parser.add_argument('--diagnostic-root',type=Path,default=ROOT/'build/c3d-box-accuracy/runs')
 args=parser.parse_args();fields={};verification={};failures={};diagnostic_repeats={}
 for path in sorted(args.root.glob('*/receipt.json')):
  receipt=json.loads(path.read_text())
  if receipt['status']=='failed':
   for q,h in receipt['artifact_sha256'].items():require(sha(path.parent/q)==h,'Failed artifact identity '+q)
   require('failure' in receipt,'Failed terminal reason')
   failures[path.parent.name]=dict(receipt=str(path.resolve()),receipt_sha256=sha(path),failure=receipt['failure'])
   continue
  c,v=verify(path);fields[path.parent.name]=c;verification[path.parent.name]=v
 for path in sorted(args.diagnostic_root.glob('*/receipt.json')):
  receipt=json.loads(path.read_text());label='accuracy/'+path.parent.name
  if receipt['status']=='failed':
   for q,h in receipt['artifact_sha256'].items():require(sha(path.parent/q)==h,'Failed diagnostic identity '+q)
   failures[label]=dict(receipt=str(path.resolve()),receipt_sha256=sha(path),failure=receipt['failure'])
   continue
  c,v=verify(path);fields[label]=c;verification[label]=v
  parent_name=path.parent.name.removesuffix('-after600')
  require(parent_name in failures,'Original failed case retained')
  original=json.loads((args.root/parent_name/'contract.json').read_text())
  diagnostic=json.loads((path.parent/'contract.json').read_text())
  for key in ('grid','length_m','lower_m','upper_m','density_kg_m3','viscosity_pa_s','flow_m3_s','owned_cap_bytes','rss_cap_bytes','original_cell_cap','complete_momentum_max','divergence_max','flux_max'):
   require(original[key]==diagnostic[key],'Exact matched diagnostic '+key)
  require(original['fixture_wall_cap_s']==600 and diagnostic['fixture_wall_cap_s']==1800,'Separate prospective time allowance')
  diagnostic_repeats[parent_name]=label
 def field_label(name):return name if name in fields else diagnostic_repeats.get(name)
 require(all(f'{name}-n{n}' in fields for name in ('long','short','large') for n in (16,32,64)),'Complete three-shape refinement')
 comparisons={}
 for name in ('long','short','large'):
  comparisons[name]=[]
  pairs=[(16,32),(32,64)]
  if field_label(f'{name}-n80'):pairs.append((64,80))
  for coarse,fine in pairs:
   coarse_label,fine_label=field_label(f'{name}-n{coarse}'),field_label(f'{name}-n{fine}')
   a,b=fields[coarse_label],fields[fine_label]
   require(a['lower_m']==b['lower_m'] and a['upper_m']==b['upper_m'] and a['length_m']==b['length_m'],'Same physical case')
   changes={k:abs(b[k]/a[k]-1) for k in ('pressure_drop_pa','physical_dissipation_w')}
   changes.update({k:abs(b[k][0]/a[k][0]-1) for k in ('pressure_force_n','viscous_force_n')})
   comparisons[name].append(dict(coarse=coarse,fine=fine,coarse_case=coarse_label,fine_case=fine_label,relative_changes=changes,
     successive_grid_1pct_passed=all(v<=.01 for v in changes.values()),
     physical_energy_passed=b['physical_energy_imbalance']<=.03,
     physical_momentum_passed=b['physical_momentum_relative_max']<=.02))
 anisotropic=None
 if field_label('large-gap-fine'):
  a,b=fields['large-gap-coarse'],fields[field_label('large-gap-fine')]
  require([2*n for n in a['grid']]==b['grid'],'Anisotropic actual grid pair')
  require(a['lower_m']==b['lower_m'] and a['upper_m']==b['upper_m'],'Anisotropic physical body')
  changes={k:abs(b[k]/a[k]-1) for k in ('pressure_drop_pa','physical_dissipation_w')}
  changes.update({k:abs(b[k][0]/a[k][0]-1) for k in ('pressure_force_n','viscous_force_n')})
  anisotropic=dict(coarse_case='large-gap-coarse',fine_case=field_label('large-gap-fine'),
    relative_changes=changes,successive_grid_1pct_passed=all(v<=.01 for v in changes.values()),
    physical_energy_passed=b['physical_energy_imbalance']<=.03,
    physical_momentum_passed=b['physical_momentum_relative_max']<=.02)
 domain_comparisons={}
 selection=args.root.parent/'domain-selection-contract.json'
 if selection.exists():
  declared=json.loads(selection.read_text())
  for name,parent in declared['parents'].items():
   longer=name+'-n64-L6'
   if longer not in fields:continue
   parent_receipt=ROOT/parent['receipt']
   require(sha(parent_receipt)==parent['sha256'],'Declared domain parent identity')
   a,b=fields[name+'-n64'],fields[longer]
   require(a['length_m']==4 and b['length_m']==6,'Matched domain lengths')
   np.testing.assert_allclose(np.array(a['length_m'])/a['grid'][0],b['length_m']/b['grid'][0],rtol=0,atol=1e-15)
   require(a['grid'][1:]==b['grid'][1:],'Matched transverse spacing')
   for key in ('lower_m','upper_m'):
    np.testing.assert_allclose(np.array(b[key])-a[key],[1,0,0],rtol=0,atol=1e-15)
   changes={key:abs(b[key][0]/a[key][0]-1) for key in ('pressure_force_n','viscous_force_n')}
   domain_comparisons[name]=dict(shorter_case=name+'-n64',longer_case=longer,force_relative_changes=changes,
     body_force_length_1pct_passed=all(value<=.01 for value in changes.values()),
     physical_energy_passed=b['physical_energy_imbalance']<=.03,
     physical_momentum_passed=b['physical_momentum_relative_max']<=.02,
     pressure_drop_and_dissipation_length_invariance_claimed=False,
     scope='matched-cell-spacing force sensitivity to simultaneous inlet/outlet displacement; no arbitrary-domain certificate')
 # Nested obstruction is a physically expected response check, not an exact value oracle.
 nested=[fields[k]['pressure_drop_pa'] for k in ('small-n16','cube-n16','large-n16')]
 require(nested[0]<nested[1]<nested[2],'Matched-grid pressure increases with nested obstruction')
 result=dict(status='passed_complete_box_evidence_and_measured_refinement',fields=fields,
  verification=verification,failed_runs=failures,diagnostic_repeats=diagnostic_repeats,anisotropic_comparison=anisotropic,domain_comparisons=domain_comparisons,comparisons=comparisons,nested_obstruction_pressure_pa=nested,
  physical_absolute_force_certification=False,persistent_goal_complete=False,
  scope='complete original SI equations/field readback and independent physical box pressure area integration; measured grid sensitivity, no absolute force reference')
 print(json.dumps(result,indent=2))

if __name__=='__main__':main()
