"""RC7 selected finite oxygen gas/water partition with an external isothermal bath."""
import argparse,copy,ctypes as C,hashlib,json,math
from pathlib import Path
from chemistry_reactor import digest,State
from chemistry_transport import Spatial,Grid
R=8.31446261815324
DATA=Path(__file__).resolve().parents[1]/'data/chemistry/oxygen_partition_v1.json'
class Model(C.Structure):
 _fields_=[(name,C.c_double) for name in ('temperature','henry','delta_u','gas_volume','liquid_volume','area','permeance')]
def properties(variant='rettich_fit'):
 raw=DATA.read_bytes();d=json.loads(raw);T=d['reference_temperature_k'];f=d['fit']
 if variant=='rettich_fit':H=math.exp(f['A']+f['B_k']/T+f['C']*math.log(T));slope=f['B_k']-f['C']*T
 else:
  if variant not in d['sensitivity_scenarios']:raise ValueError('phase property variant')
  x=d['sensitivity_scenarios'][variant];H=x['henry_mol_m3_pa'];slope=x['slope_k']
 return dict(data=d,sha256=hashlib.sha256(raw).hexdigest(),T=T,H=H,slope=slope,delta_h=-R*slope,delta_u=R*(T-slope))
def mechanism():
 return dict(schema='rc1/v1',phase='synthetic-isothermal',atomic_mass_kg_mol=[.015999],species=[dict(id=name,atoms=[2],charge=0) for name in ('O2_gas','O2_aqueous')],reactions=[dict(reactant=a,product=b,k=0.,unit='s^-1') for a,b in [([1,0],[0,1]),([0,1],[1,0])]],initial_mol=[0.,0.],volume_m3=1.,dt_s=1.)
def code_identity():
 return digest({name:hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest() for name in ('chemistry_phase.py','chemistry_reactor.py','chemistry_transport.py')})
def example(local=False):
 p=properties();n=[1,1,1] if local else [4,4,4];V=1e-6;fraction=.5
 phase=dict(model='oxygen-water-fixed-isothermal/v1',energy_basis='relative-phase-internal-energy/v1',data_sha256=p['sha256'],property_variant='rettich_fit',temperature_k=298.15,solvent='pure-water',salinity_g_kg=0.,chemistry='nonreactive-O2',liquid_fraction=fraction,interface_area_m2=1e-4,permeance_mol_m2_s_pa=1e-9,gas_velocity_m_s=[0.]*3,liquid_velocity_m_s=[0.]*3,gas_diffusion_m2_s=0.,liquid_diffusion_m2_s=0.)
 rows=[[5000*V*(1-fraction)/(R*p['T']),0.] for _ in range(math.prod(n))]
 return dict(schema='rc7/v1',chemistry=mechanism(),phase=phase,n=n,boundary=[0]*3,spacing_m=[.01]*3,velocity_m_s=[0.]*3,diffusion_m2_s=0.,dt_s=1.,reacting=True,initial_mol=rows)
class Partition:
 def __init__(self,library,config):
  self.config=copy.deepcopy(config);f=config['phase'];self.p=properties(f['property_variant']);self.code=code_identity();p=self.p
  expected=set(example()['phase'])
  if config.get('schema')!='rc7/v1' or set(f)!=expected or config.get('chemistry')!=mechanism() or f['energy_basis']!='relative-phase-internal-energy/v1' or f['model']!='oxygen-water-fixed-isothermal/v1' or f['data_sha256']!=p['sha256'] or f['temperature_k']!=298.15 or f['solvent']!='pure-water' or f['salinity_g_kg']!=0 or f['chemistry']!='nonreactive-O2':raise ValueError('phase data/conditions/mechanism')
  if config['velocity_m_s']!=[0.]*3 or config['diffusion_m2_s']!=0:raise ValueError('use separate phase transport inputs')
  base=copy.deepcopy(config);base.pop('phase');base['schema']='rc2/v1';self.core=Spatial(library,base);self.count=self.core.count;self.lib=self.core.lib;V=math.prod(config['spacing_m']);fraction=f['liquid_fraction']
  if not math.isfinite(fraction) or not .05<=fraction<=.95:raise ValueError('occupied phase fraction')
  self.model=Model(298.15,p['H'],p['delta_u'],V*(1-fraction),V*fraction,f['interface_area_m2'],f['permeance_mol_m2_s_pa'] if config['reacting'] else 0.)
  for name,args in [('chem_phase_valid',[C.POINTER(Model)]),('chem_phase_admit',[C.POINTER(Model),C.POINTER(State),C.c_double]),('chem_phase_spatial_step',[C.POINTER(Model),C.POINTER(Grid),C.POINTER(Grid),C.POINTER(State),C.POINTER(C.c_double),C.c_double,C.POINTER(C.c_double)])]:
   if not hasattr(self.lib,name):raise ValueError('selected engine lacks RC7 phase exchange')
   fn=getattr(self.lib,name);fn.argtypes=args;fn.restype=C.c_bool
  if not self.lib.chem_phase_valid(C.byref(self.model)) or config['dt_s']>1e6:raise ValueError('phase property/time admission')
  self.grids=[]
  for name in ('gas','liquid'):
   velocity=f[name+'_velocity_m_s']
   if len(velocity)!=3:raise ValueError('phase velocity shape')
   g=Grid.from_buffer_copy(bytes(self.core.g));g.velocity=(C.c_double*3)(*velocity);g.diffusion=f[name+'_diffusion_m2_s']
   if not self.lib.chem_grid_valid(C.byref(g)):raise ValueError('phase transport')
   self.grids.append(g)
  self.sensitivity=[]
  for variant in ['rettich_fit',*p['data']['sensitivity_scenarios']]:
   alt=properties(variant);ratio=alt['H']*R*298.15*self.model.liquid_volume/self.model.gas_volume;self.sensitivity.append(dict(variant=variant,henry_mol_m3_pa=alt['H'],delta_u_model_j_mol=alt['delta_u'],equilibrium_liquid_fraction=ratio/(1+ratio)))
  self.energy=(C.c_double*self.count)(*[p['delta_u']*s.amount[1] for s in self.core.cells]);self.external=[0.]*self.count;self.initial_energy=math.fsum(self.energy);self.audit(self.core,self.energy)
 def audit(self,core,energy):
  if any(not self.lib.chem_phase_admit(C.byref(self.model),C.byref(s),E) for s,E in zip(core.cells,energy)):raise ValueError('phase state/energy outside dilute model')
 def advance(self,steps):
  if type(steps)!=int or not 0<=steps<=10000:raise ValueError('phase steps')
  for _ in range(steps):
   heat=(C.c_double*self.count)()
   if not self.lib.chem_phase_spatial_step(C.byref(self.model),C.byref(self.grids[0]),C.byref(self.grids[1]),self.core.cells,self.energy,self.config['dt_s'],heat):raise ValueError('joint phase/transport step refused; accepted state retained')
   self.core.tick+=1
   for q,x in enumerate(heat):self.external[q]+=x
 def report(self):
  r=self.core.report();r['bulk_concentration_mol_m3']=r['concentration_mol_m3'];r['concentration_mol_m3']=[[s.amount[0]/self.model.gas_volume,s.amount[1]/self.model.liquid_volume] for s in self.core.cells];U=math.fsum(self.energy);Q=math.fsum(self.external)
  r['phase']=dict(model='oxygen-water-fixed-isothermal/v1',energy_basis='relative-phase-internal-energy/v1',data_id=self.p['data']['id'],data_sha256=self.p['sha256'],property_variant=self.config['phase']['property_variant'],temperature_k=298.15,henry_mol_m3_pa=self.p['H'],delta_h_model_j_mol=self.p['delta_h'],delta_u_model_j_mol=self.p['delta_u'],occupied_volume_m3=[self.model.gas_volume,self.model.liquid_volume],partial_pressure_pa=[s.amount[0]*R*298.15/self.model.gas_volume for s in self.core.cells],internal_energy_j=list(self.energy),total_internal_j=U,external_bath_heat_j=Q,bath_received_j=-Q,energy_residual_j=U-self.initial_energy-Q,oxygen_mass_kg=.031998*sum(r['totals_mol']),oxygen_atom_budget_residual_mol=2*sum(r['species_budget_residual_mol']),concentration_basis='per occupied phase volume',uncertainty=self.p['data']['uncertainty'],property_sensitivity_scenarios=self.sensitivity,interface_area_m2=self.model.area,permeance_mol_m2_s_pa=self.model.permeance,transport_calibration='imposed independent phase velocities/D; no solved-flow or calibrated transfer-rate claim')
  r['phase']['pressure_drive_pa']=[s.amount[0]*R*298.15/self.model.gas_volume-s.amount[1]/(self.p['H']*self.model.liquid_volume) for s in self.core.cells]
  r['phase']['net_transfer_mol_s']=[self.model.area*self.model.permeance*x for x in r['phase']['pressure_drive_pa']]
  r['thermal_model']='external isothermal bath; phase internal-energy difference includes dissolution energy once';return r
 def checkpoint(self):
  p=dict(schema='rc7-checkpoint/v1',config=digest(self.config),model_code=self.code,core=self.core.checkpoint(),energy=list(self.energy),external=self.external.copy());return dict(payload=p,sha256=digest(p))
 def restore(self,packet):
  p=packet['payload']
  if set(p)!={'schema','config','model_code','core','energy','external'} or p['schema']!='rc7-checkpoint/v1' or p['config']!=digest(self.config) or p['model_code']!=self.code or digest(p)!=packet['sha256'] or len(p['energy'])!=self.count or len(p['external'])!=self.count or any(not math.isfinite(x) for x in p['external']):raise ValueError('phase checkpoint identity/shape')
  candidate=copy.copy(self.core);candidate.restore(p['core']);energy=(C.c_double*self.count)(*p['energy']);self.audit(candidate,energy);residual=math.fsum(energy)-self.initial_energy-math.fsum(p['external'])
  if abs(residual)>1e-10*max(1e-12,abs(self.initial_energy),abs(math.fsum(energy))):raise ValueError('phase checkpoint bath ledger')
  self.core=candidate;self.energy=energy;self.external=p['external'].copy()
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--library',required=True);p.add_argument('--config',required=True);p.add_argument('--steps',type=int,default=0);p.add_argument('--resume');p.add_argument('--checkpoint');a=p.parse_args()
 if not 0<=a.steps<=10000:p.error('steps')
 m=Partition(a.library,json.loads(Path(a.config).read_text()))
 if a.resume:m.restore(json.loads(Path(a.resume).read_text()))
 print(json.dumps(m.report(),allow_nan=False))
 for _ in range(a.steps):m.advance(1);print(json.dumps(m.report(),allow_nan=False))
 if a.checkpoint:
  with open(a.checkpoint,'x') as f:json.dump(m.checkpoint(),f,allow_nan=False)
if __name__=='__main__':main()
