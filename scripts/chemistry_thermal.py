"""RC5 formation-inclusive thermal chemistry; synthetic fixed-volume liquid."""
import argparse,copy,ctypes as C,json,math
from pathlib import Path
from chemistry_reactor import Reactor,Mechanism,State,digest
from chemistry_transport import Spatial,Grid
from chemistry_flow import Coupled,Model,Metrics
class Thermo(C.Structure):
 _fields_=[('reference_t',C.c_double),('min_t',C.c_double),('max_t',C.c_double),('carrier_cv',C.c_double),('formation',C.c_double*16),('cv',C.c_double*16),('activation',C.c_double*32),('conductivity',C.c_double),('bath_t',C.c_double),('bath_conductance',C.c_double),('power',C.c_double),('expansion',C.c_double)]
def properties(chem,ns,nr,config):
 keys={'model','phase','energy_basis','reference_temperature_k','valid_temperature_k','carrier_cv_j_m3_k','formation_j_mol','cv_j_mol_k','activation_j_mol','conductivity_w_m_k','bath_temperature_k','bath_conductance_w_m3_k','power_w_m3','thermal_expansion_1_k','provenance','initial_temperature_k'}
 if set(config)!=keys or config['model']!='synthetic-caloric/v1' or config['phase']!='synthetic-single-liquid' or config['energy_basis']!='formation-inclusive-internal':raise ValueError('thermodynamic model/phase/energy convention')
 provenance=config['provenance']
 if set(provenance)!={'id','source','uncertainty'} or any(not isinstance(x,str) or not x or len(x)>500 for x in provenance.values()):raise ValueError('property provenance')
 if len(config['valid_temperature_k'])!=2 or len(config['formation_j_mol'])!=ns or len(config['cv_j_mol_k'])!=ns or len(config['activation_j_mol'])!=nr:raise ValueError('property shape')
 values=[config['reference_temperature_k'],*config['valid_temperature_k'],config['carrier_cv_j_m3_k'],*config['formation_j_mol'],*config['cv_j_mol_k'],*config['activation_j_mol'],config['conductivity_w_m_k'],config['bath_temperature_k'],config['bath_conductance_w_m3_k'],config['power_w_m3'],config['thermal_expansion_1_k']]
 if any(not math.isfinite(x) for x in values):raise ValueError('property finite')
 return Thermo(config['reference_temperature_k'],*config['valid_temperature_k'],config['carrier_cv_j_m3_k'],(C.c_double*16)(*config['formation_j_mol']),(C.c_double*16)(*config['cv_j_mol_k']),(C.c_double*32)(*config['activation_j_mol']),config['conductivity_w_m_k'],config['bath_temperature_k'],config['bath_conductance_w_m3_k'],config['power_w_m3'],config['thermal_expansion_1_k'])
def bind(lib):
 for name,args in [('chem_thermal_valid',[C.POINTER(Mechanism),C.POINTER(Thermo)]),('chem_thermal_energy',[C.POINTER(Mechanism),C.POINTER(Thermo),C.POINTER(State),C.c_double,C.c_double,C.POINTER(C.c_double)]),('chem_thermal_temperature',[C.POINTER(Mechanism),C.POINTER(Thermo),C.POINTER(State),C.c_double,C.c_double,C.POINTER(C.c_double)]),('chem_thermal_react',[C.POINTER(Mechanism),C.POINTER(Thermo),C.POINTER(State),C.POINTER(C.c_double),C.c_double,C.c_double,C.POINTER(C.c_double)])]:
  if not hasattr(lib,name):raise ValueError('selected engine lacks RC5 thermal chemistry')
  f=getattr(lib,name);f.argtypes=args;f.restype=C.c_bool
class LocalThermal(Reactor):
 def __init__(self,library,config):
  original=copy.deepcopy(config);base=copy.deepcopy(config)
  if base.pop('schema',None)!='rc5-local/v1':raise ValueError('local thermal schema')
  thermo=base.pop('thermal');base['schema']='rc1/v1';super().__init__(library,base);self.c=original;bind(self.lib);self.thermo=properties(self.m,self.m.species,self.m.reactions,thermo)
  if not self.lib.chem_thermal_valid(C.byref(self.m),C.byref(self.thermo)):raise ValueError('thermal property/mechanism admission')
  if len(thermo['initial_temperature_k'])!=1:raise ValueError('local temperature')
  self.energy=C.c_double();self.external=0.
  if not self.lib.chem_thermal_energy(C.byref(self.m),C.byref(self.thermo),C.byref(self.s),config['volume_m3'],thermo['initial_temperature_k'][0],C.byref(self.energy)):raise ValueError('initial thermal state')
  self.initial_energy=self.energy.value
 def temperature(self):
  temp=C.c_double()
  if not self.lib.chem_thermal_temperature(C.byref(self.m),C.byref(self.thermo),C.byref(self.s),self.c['volume_m3'],self.energy.value,C.byref(temp)):raise ValueError('temperature')
  return temp.value
 def advance(self,steps):
  if type(steps)!=int or not 0<=steps<=1000000:raise ValueError('steps')
  for _ in range(steps):
   q=C.c_double()
   if not self.lib.chem_thermal_react(C.byref(self.m),C.byref(self.thermo),C.byref(self.s),C.byref(self.energy),self.c['volume_m3'],self.c['dt_s'],C.byref(q)):raise ValueError('thermal reaction refused')
   self.external+=q.value;self.tick+=1
 def report(self):
  r=super().report();evaluated=Mechanism.from_buffer_copy(bytes(self.m));T=self.temperature()
  for i in range(self.m.reactions):evaluated.k[i]*=math.exp(-self.thermo.activation[i]/8.31446261815324*(1/T-1/self.thermo.reference_t))
  rates=(C.c_double*32)()
  if not self.lib.chem_rates(C.byref(evaluated),C.byref(self.s),self.c['volume_m3'],rates):raise ValueError('thermal rate readout')
  r['rates_mol_s']=list(rates)[:self.m.reactions];r['thermal_model']='formation-inclusive internal energy';r['thermal']=dict(temperature_k=self.temperature(),internal_energy_j=self.energy.value,external_heat_j=self.external,energy_residual_j=self.energy.value-self.initial_energy-self.external,property_id=self.c['thermal']['provenance']['id']);return r
 def checkpoint(self):
  p=dict(schema='rc5-local-checkpoint/v1',reactor=super().checkpoint(),energy=self.energy.value,external=self.external);return dict(payload=p,sha256=digest(p))
 def restore(self,packet):
  p=packet['payload']
  if digest(p)!=packet['sha256'] or set(p)!={'schema','reactor','energy','external'} or p['schema']!='rc5-local-checkpoint/v1' or not math.isfinite(p['external']):raise ValueError('thermal checkpoint')
  candidate=LocalThermal.__new__(LocalThermal);candidate.__dict__=self.__dict__.copy();Reactor.restore(candidate,p['reactor']);candidate.energy=C.c_double(p['energy']);candidate.external=p['external'];candidate.temperature()
  if abs(candidate.energy.value-candidate.initial_energy-candidate.external)>1e-8*max(1,abs(candidate.external)):raise ValueError('thermal ledger')
  self.__dict__=candidate.__dict__
class ThermalCoupled(Coupled):
 def __init__(self,library,config):
  original=copy.deepcopy(config);base=copy.deepcopy(config)
  if base.get('schema')!='rc5/v1':raise ValueError('thermal flow schema')
  thermo=base.pop('thermal');base['schema']='rc4/v1';super().__init__(library,base);self.config=original;bind(self.lib);self.thermo=properties(self.m,self.m.species,self.m.reactions,thermo)
  if not self.lib.chem_thermal_valid(C.byref(self.m),C.byref(self.thermo)):raise ValueError('thermal property/mechanism admission')
  if len(thermo['initial_temperature_k'])!=self.count:raise ValueError('temperature field count')
  self.energy=(C.c_double*self.count)();vol=math.prod(config['spacing_m'])
  for q,T in enumerate(thermo['initial_temperature_k']):
   e=C.c_double()
   if not self.lib.chem_thermal_energy(C.byref(self.m),C.byref(self.thermo),C.byref(self.cells[q]),vol,T,C.byref(e)):raise ValueError('initial temperature')
   self.energy[q]=e.value
  self.initial_internal=math.fsum(self.energy);self.external_heat=0.;self.viscous_heat=0.;self.temperatures()
  self.lib.chem_thermal_flow_step.argtypes=[C.POINTER(Mechanism),C.POINTER(Grid),C.POINTER(Model),C.POINTER(Thermo),C.POINTER(State),C.POINTER(C.c_double),C.POINTER(C.c_double),C.POINTER(C.c_double),C.c_double,C.c_bool,C.POINTER(Metrics),C.POINTER(C.c_double),C.POINTER(C.c_double)];self.lib.chem_thermal_flow_step.restype=C.c_bool
 def temperatures(self):
  temps=[];volume=math.prod(self.config['spacing_m']);f=self.config['flow']
  for q in range(self.count):
   T=C.c_double()
   if not self.lib.chem_thermal_temperature(C.byref(self.m),C.byref(self.thermo),C.byref(self.cells[q]),volume,self.energy[q],C.byref(T)):raise ValueError('temperature validity')
   contrast=sum(b*(self.cells[q].amount[s]/volume-f['reference_mol_m3'][s]) for s,b in enumerate(f['beta_m3_mol']))-self.thermo.expansion*(T.value-self.thermo.reference_t)
   if not math.isfinite(contrast) or abs(contrast)>.01:raise ValueError('combined thermal/solutal contrast')
   temps.append(T.value)
  return temps
 def advance(self,steps):
  if type(steps)!=int or not 0<=steps<=10000:raise ValueError('thermal batch')
  for _ in range(steps):
   metrics=Metrics();external=C.c_double();viscous=C.c_double()
   if not self.lib.chem_thermal_flow_step(C.byref(self.m),C.byref(self.g),C.byref(self.model),C.byref(self.thermo),self.cells,self.velocity,self.pressure,self.energy,self.config['dt_s'],self.config['reacting'],C.byref(metrics),C.byref(external),C.byref(viscous)):raise ValueError('thermal joint step refused; state retained')
   self.metrics=metrics;self.tick+=1;self.external_heat+=external.value;self.viscous_heat+=viscous.value
   for k in self.cumulative:self.cumulative[k]+=getattr(metrics,k)
 def report(self):
  r=super().report();temps=self.temperatures();internal=math.fsum(self.energy);vol=math.prod(self.config['spacing_m']);f=self.config['flow']
  contrast=[sum(b*(self.cells[q].amount[s]/vol-f['reference_mol_m3'][s]) for s,b in enumerate(f['beta_m3_mol']))-self.thermo.expansion*(T-self.thermo.reference_t) for q,T in enumerate(temps)]
  r['thermal_model']='formation-inclusive internal energy'
  r['thermal']=dict(temperature_k=temps,internal_energy_j=list(self.energy),total_internal_j=internal,external_heat_j=self.external_heat,viscous_heat_j=self.viscous_heat,combined_density_contrast=contrast,internal_ledger_residual_j=internal-self.initial_internal-self.external_heat-self.viscous_heat,total_ledger_residual_j=internal-self.initial_internal+self.kinetic()-self.initial_kinetic+self.cumulative['time_defect']-self.external_heat-self.cumulative['work'],property_id=self.config['thermal']['provenance']['id'],numerical_heat_added_j=0.)
  return r
 def checkpoint(self):
  p=dict(schema='rc5-checkpoint/v1',flow=super().checkpoint(),energy=list(self.energy),external_heat=self.external_heat,viscous_heat=self.viscous_heat);return dict(payload=p,sha256=digest(p))
 def restore(self,packet):
  p=packet['payload']
  if digest(p)!=packet['sha256'] or set(p)!={'schema','flow','energy','external_heat','viscous_heat'} or p['schema']!='rc5-checkpoint/v1' or len(p['energy'])!=self.count or not math.isfinite(p['external_heat']) or not math.isfinite(p['viscous_heat']) or p['viscous_heat']<0:raise ValueError('thermal checkpoint')
  candidate=ThermalCoupled.__new__(ThermalCoupled);candidate.__dict__=self.__dict__.copy();Coupled.restore(candidate,p['flow']);candidate.energy=(C.c_double*self.count)(*p['energy']);candidate.external_heat=p['external_heat'];candidate.viscous_heat=p['viscous_heat'];candidate.temperatures();r=candidate.report()['thermal']
  if abs(r['internal_ledger_residual_j'])>1e-8*max(1,abs(candidate.external_heat)+candidate.viscous_heat) or abs(candidate.viscous_heat-candidate.cumulative['dissipation'])>1e-10*max(1e-12,candidate.viscous_heat) or abs(r['total_ledger_residual_j'])>1e-8*max(1,abs(candidate.external_heat)+abs(candidate.cumulative['work'])):raise ValueError('thermal checkpoint ledger')
  self.__dict__=candidate.__dict__
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--library',required=True);p.add_argument('--config',required=True);p.add_argument('--steps',type=int,default=0);p.add_argument('--resume');p.add_argument('--checkpoint');a=p.parse_args();config=json.loads(Path(a.config).read_text());r=LocalThermal(a.library,config) if config['schema']=='rc5-local/v1' else ThermalCoupled(a.library,config)
 if not 0<=a.steps<=10000:p.error('steps')
 if a.resume:r.restore(json.loads(Path(a.resume).read_text()))
 print(json.dumps(r.report(),allow_nan=False))
 for _ in range(a.steps):r.advance(1);print(json.dumps(r.report(),allow_nan=False))
 if a.checkpoint:
  with open(a.checkpoint,'x') as f:json.dump(r.checkpoint(),f,allow_nan=False)
if __name__=='__main__':main()
