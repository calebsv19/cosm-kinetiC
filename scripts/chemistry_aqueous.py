"""RC6 conditional aqueous-glucose isothermal model, backed by the native chemistry kernel."""
import argparse,copy,ctypes as C,hashlib,json,math
from pathlib import Path
from chemistry_reactor import Reactor,digest
from chemistry_transport import Spatial
from chemistry_aqueous_data import load_data,qualification,R
MODEL='aqueous-glucose-effective/v1'

def code_identity():
 return digest({name:hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest() for name in ('chemistry_aqueous.py','chemistry_aqueous_data.py','chemistry_reactor.py','chemistry_transport.py')})
def mechanism(p):
 return dict(schema='rc1/v1',phase='synthetic-isothermal',atomic_mass_kg_mol=[.012011,.001008,.015999],species=[dict(id=name,atoms=[6,12,6],charge=0) for name in ['alpha-D-glucopyranose','beta-D-glucopyranose']],reactions=[dict(reactant=[1,0],product=[0,1],k=p['k_forward'],unit='s^-1'),dict(reactant=[0,1],product=[1,0],k=p['k_reverse'],unit='s^-1')],initial_mol=[0.,0.],volume_m3=1.,dt_s=1.)
def conditions(p):
 return dict(model=MODEL,dataset_sha256=p['sha256'],temperature_k=274.65,solvent='reported-water-only-recipe',ph=None,pressure_pa=None,ionic_strength_mol_l=None,cosolvent_mass_fraction=0.,added_catalyst=False)
def example(spatial=True):
 p=load_data();chem=mechanism(p);aq=conditions(p);alpha=p['first_alpha_fraction'];c=p['concentration_mol_m3']
 if not spatial:return dict(schema='rc6-local/v1',chemistry=chem,aqueous=aq,volume_m3=p['recipe_volume_m3'],initial_alpha_fraction=alpha,dt_s=30.)
 return dict(schema='rc6/v1',chemistry=chem,aqueous=aq,n=[4,4,4],boundary=[0,0,0],spacing_m=[.01]*3,velocity_m_s=[0.,0.,0.],diffusion_m2_s=0.,dt_s=30.,reacting=True,initial_mol=[[c*1e-6*alpha,c*1e-6*(1-alpha)] for _ in range(64)])
class Aqueous:
 def __init__(self,library,config):
  self.config=copy.deepcopy(config);self.p=load_data();self.code=code_identity();self.local=config.get('schema')=='rc6-local/v1';self.validation=qualification()
  if not self.validation['passed']:raise ValueError('aqueous empirical qualification failed')
  if config.get('schema') not in ('rc6-local/v1','rc6/v1') or config.get('aqueous')!=conditions(self.p) or config.get('chemistry')!=mechanism(self.p):raise ValueError('aqueous dataset, mechanism or solvent/temperature condition mismatch')
  base=copy.deepcopy(config);base.pop('aqueous');chem=base['chemistry'];concentration=self.p['concentration_mol_m3']
  if self.local:
   if set(config)!={'schema','chemistry','aqueous','volume_m3','initial_alpha_fraction','dt_s'}:raise ValueError('aqueous local fields')
   v=config['volume_m3'];a=config['initial_alpha_fraction']
   if not math.isfinite(v) or not 1e-12<=v<=1 or not math.isfinite(a) or not 0<=a<=1:raise ValueError('aqueous local inventory')
   chem.update(volume_m3=v,dt_s=config['dt_s'],initial_mol=[v*concentration*a,v*concentration*(1-a)]);self.core=Reactor(library,chem)
  else:
   base['schema']='rc2/v1';self.core=Spatial(library,base)
  if not math.isfinite(config['dt_s']) or self.p['rate_s']*config['dt_s']>.25:raise ValueError('aqueous reaction time step domain')
  self.audit(self.core)
 def audit(self,core):
  if self.local:rows=[list(core.s.amount)[:2]];volume=self.config['volume_m3']
  else:rows=[list(c.amount)[:2] for c in core.cells];volume=math.prod(self.config['spacing_m'])
  target=self.p['concentration_mol_m3']*volume
  if any(any(not math.isfinite(n) or n<0 for n in row) or abs(sum(row)-target)>1e-10*target for row in rows):raise ValueError('local total concentration outside documented recipe')
  return rows,volume
 def advance(self,steps):self.core.advance(steps)
 def report(self):
  r=self.core.report();rows,vol=self.audit(self.core);K=self.p['K'];xeq=[1/(1+K),K/(1+K)];RT=R*self.config['aqueous']['temperature_k'];total=[math.fsum(row[s] for row in rows) for s in range(2)];fractions=[row[0]/sum(row) for row in rows]
  free=math.fsum(RT*n*math.log((n/sum(row))/xeq[s]) for row in rows for s,n in enumerate(row) if n>0);A,B=[self.p['raw']['equilibrium'][key] for key in ('specific_rotation_alpha','specific_rotation_beta')]
  uncertainty=dict(rate_rounding_only_range_s=self.p['rate_rounding_range_s'],K_rounding_only_range=self.p['K_rounding_range'],initial_alpha_rotation_rounding_only_range=self.p['first_alpha_rotation_rounding_range'],experimental_measurement_uncertainty=None,unquantified=['temperature dependence','late-endpoint temperature/time protocol','time-stamp precision','pH and ionic strength','purity','preparation mass/density uncertainty','systematic measurement error','unresolved intermediate/minor forms'],status=self.validation['uncertainty_status'])
  r['aqueous']=dict(model=MODEL,dataset_id=self.p['raw']['id'],dataset_sha256=self.p['sha256'],temperature_k=274.65,phase='aqueous-isothermal-effective',equilibrium_ratio_beta_over_alpha=K,relaxation_rate_s=self.p['rate_s'],forward_rate_s=self.p['k_forward'],reverse_rate_s=self.p['k_reverse'],alpha_fraction=fractions,relative_gibbs_j=free,conditional_delta_g0_j_mol=-RT*math.log(K),reaction_enthalpy_j_mol=None,uncertainty=uncertainty,qualification='conditional historical-recipe empirical comparison',transport_calibration='operator-selected transport coefficients; not measured glucose diffusivity or solved flow')
  r['aqueous']['empirical_kinetic_window_elapsed_s']=[0.,14160.]
  r['aqueous']['within_empirical_kinetic_window']=0<=r['time_s']<=14160
  r['aqueous']['equilibrium_endpoint_status']='Asymptotic calibration only; timed late-endpoint trajectory not qualified'
  r['aqueous']['rotation_deg_saccharimeter']=self.p['rotation_scale']*(A*total[0]+B*total[1])/sum(total)
  r['aqueous']['net_forward_mol_s']=self.p['k_forward']*total[0]-self.p['k_reverse']*total[1]
  r['thermal_model']='external isothermal bath; reaction enthalpy unavailable; no thermal coupling'
  if self.local and self.config['initial_alpha_fraction']==self.p['first_alpha_fraction']:r['experiment_time_min']=4+r['time_s']/60
  r['aqueous']['initial_state_status']='historical first-reading inference' if (self.local and self.config['initial_alpha_fraction']==self.p['first_alpha_fraction']) or (not self.local and all(abs(row[0]/sum(row)-self.p['first_alpha_fraction'])<1e-14 for row in self.config['initial_mol'])) else 'engineered anomer-mixture initial state; no direct experimental spatial validation'
  return r
 def checkpoint(self):
  p=dict(schema='rc6-checkpoint/v1',config=digest(self.config),model_code=self.code,core=self.core.checkpoint());return dict(payload=p,sha256=digest(p))
 def restore(self,packet):
  p=packet['payload']
  if set(p)!={'schema','config','model_code','core'} or p['schema']!='rc6-checkpoint/v1' or digest(p)!=packet['sha256'] or p['config']!=digest(self.config) or p['model_code']!=self.code:raise ValueError('aqueous checkpoint identity')
  candidate=copy.copy(self.core);candidate.restore(p['core']);self.audit(candidate);self.core=candidate

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--library');p.add_argument('--config');p.add_argument('--steps',type=int,default=0);p.add_argument('--resume');p.add_argument('--checkpoint');p.add_argument('--qualify',action='store_true');a=p.parse_args()
 if a.qualify:print(json.dumps(qualification(),allow_nan=False,indent=2));return
 if not a.library or not a.config:p.error('library and config required')
 if not 0<=a.steps<=10000:p.error('steps')
 m=Aqueous(a.library,json.loads(Path(a.config).read_text()))
 if a.resume:m.restore(json.loads(Path(a.resume).read_text()))
 print(json.dumps(m.report(),allow_nan=False))
 for _ in range(a.steps):m.advance(1);print(json.dumps(m.report(),allow_nan=False))
 if a.checkpoint:
  with open(a.checkpoint,'x') as f:json.dump(m.checkpoint(),f,allow_nan=False)
if __name__=='__main__':main()
