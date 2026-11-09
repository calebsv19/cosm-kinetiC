"""RC4 periodic unsteady-Stokes / solutal Boussinesq experiment."""
import argparse,copy,ctypes as C,json,math
from pathlib import Path
from chemistry_reactor import Mechanism,State,digest
from chemistry_transport import Spatial,Grid
class Model(C.Structure):
 _fields_=[('rho',C.c_double),('nu',C.c_double),('gravity',C.c_double*3),('beta',C.c_double*16),('reference',C.c_double*16),('feedback',C.c_int)]
class Metrics(C.Structure):
 _fields_=[(k,C.c_double) for k in ('divergence','reynolds','contrast','kinetic','work','dissipation','time_defect','energy_residual','momentum_residual')]
class Coupled(Spatial):
 def __init__(self,library,config):
  if config.get('schema')!='rc4/v1' or 'flow' not in config:raise ValueError('flow schema')
  original=copy.deepcopy(config);base=copy.deepcopy(config);f=base.pop('flow');base['schema']='rc2/v1';super().__init__(library,base);self.config=original
  if set(f)!={'model','rho_kg_m3','nu_m2_s','gravity_m_s2','beta_m3_mol','reference_mol_m3','feedback','initial_velocity_m_s'} or f['model']!='periodic-unsteady-stokes-solutal/v1':raise ValueError('closure')
  if config['dt_s']>.1 or any(not .001<=n*h<=1000 for n,h in zip(config['n'],config['spacing_m'])):raise ValueError('flow geometry/time domain')
  if any(n<4 for n in config['n']) or any(config['boundary']) or any(config['velocity_m_s']):raise ValueError('periodic3D faces only; prescribed velocity must be zero')
  if type(f['feedback'])!=bool or len(f['gravity_m_s2'])!=3 or len(f['beta_m3_mol'])!=self.m.species or len(f['reference_mol_m3'])!=self.m.species:raise ValueError('closure shape')
  vals=[f['rho_kg_m3'],f['nu_m2_s'],*f['gravity_m_s2'],*f['beta_m3_mol'],*f['reference_mol_m3']]
  if any(not math.isfinite(x) for x in vals) or f['rho_kg_m3']<=0 or f['nu_m2_s']<=0 or any(x<0 for x in f['reference_mol_m3']):raise ValueError('closure properties')
  self.model=Model(f['rho_kg_m3'],f['nu_m2_s'],(C.c_double*3)(*f['gravity_m_s2']),(C.c_double*16)(*f['beta_m3_mol']),(C.c_double*16)(*f['reference_mol_m3']),f['feedback'])
  self.velocity=(C.c_double*(3*self.count))();self.pressure=(C.c_double*self.count)()
  if len(f['initial_velocity_m_s'])!=3*self.count:raise ValueError('face count')
  for i,x in enumerate(f['initial_velocity_m_s']):self.velocity[i]=x
  if not hasattr(self.lib,'chem_flow_step'):raise ValueError('selected engine lacks RC4 solved flow')
  self.lib.chem_flow_step.argtypes=[C.POINTER(Mechanism),C.POINTER(Grid),C.POINTER(Model),C.POINTER(State),C.POINTER(C.c_double),C.POINTER(C.c_double),C.c_double,C.c_bool,C.POINTER(Metrics)];self.lib.chem_flow_step.restype=C.c_bool
  self.metrics=Metrics();self.cumulative=dict(work=0.,dissipation=0.,time_defect=0.);self.initial_kinetic=self.kinetic();self.metrics.reynolds,self.metrics.contrast,self.metrics.divergence=self.admit_fields();self.metrics.kinetic=self.initial_kinetic
  if not math.isfinite(self.initial_kinetic):raise ValueError("initial kinetic energy")
 def kinetic(self):return .5*self.model.rho*math.prod(self.config['spacing_m'])*math.fsum(v*v for v in self.velocity)
 def admit_fields(self):
  if any(not math.isfinite(v) for v in list(self.velocity)+list(self.pressure)):raise ValueError('flow finite')
  n=self.config['n'];h=self.config['spacing_m'];stride=[1,n[0],n[0]*n[1]];length=max(N*x for N,x in zip(n,h));speed=max(math.sqrt(sum(self.velocity[a*self.count+q]**2 for a in range(3))) for q in range(self.count));re=speed*length/self.model.nu
  v=math.prod(h);contrast=max(abs(sum(self.model.beta[s]*(c.amount[s]/v-self.model.reference[s]) for s in range(self.m.species))) for c in self.cells)
  if not math.isfinite(contrast) or contrast>.01 or re>.1:raise ValueError('Boussinesq/Re domain')
  maximum_divergence=0.
  for q in range(self.count):
   divergence=0
   for a in range(3):
    x=(q//stride[a])%n[a];plus=q+stride[a] if x<n[a]-1 else q-(n[a]-1)*stride[a];divergence+=(self.velocity[a*self.count+plus]-self.velocity[a*self.count+q])/h[a]
   maximum_divergence=max(maximum_divergence,abs(divergence))
   if abs(divergence)>1e-8:raise ValueError('initial divergence')
  return re,contrast,maximum_divergence
 def advance(self,steps):
  if type(steps)!=int or not 0<=steps<=10000:raise ValueError('flow batch')
  for _ in range(steps):
   metrics=Metrics()
   if not self.lib.chem_flow_step(C.byref(self.m),C.byref(self.g),C.byref(self.model),self.cells,self.velocity,self.pressure,self.config['dt_s'],self.config['reacting'],C.byref(metrics)):raise ValueError('flow step refused; state retained')
   self.metrics=metrics;self.tick+=1
   for k in self.cumulative:self.cumulative[k]+=getattr(metrics,k)
 def report(self):
  r=super().report();f=self.config['flow'];v=math.prod(self.config['spacing_m']);contrast=[sum(b*(c.amount[s]/v-f['reference_mol_m3'][s]) for s,b in enumerate(f['beta_m3_mol'])) for c in self.cells]
  r['flow']=dict(model=f['model'],velocity_m_s=list(self.velocity),pressure_pa=list(self.pressure),density_contrast=contrast,metrics={k:getattr(self.metrics,k) for k,_ in Metrics._fields_},cumulative_j=self.cumulative.copy(),kinetic_j=self.kinetic(),energy_ledger_residual_j=self.kinetic()-self.initial_kinetic+self.cumulative['dissipation']+self.cumulative['time_defect']-self.cumulative['work'])
  return r
 def checkpoint(self):
  p=dict(schema='rc4-checkpoint/v1',spatial=super().checkpoint(),velocity=list(self.velocity),pressure=list(self.pressure),metrics={k:getattr(self.metrics,k) for k,_ in Metrics._fields_},cumulative=self.cumulative.copy());return dict(payload=p,sha256=digest(p))
 def restore(self,packet):
  p=packet['payload']
  if digest(p)!=packet['sha256'] or set(p)!={'schema','spatial','velocity','pressure','metrics','cumulative'} or p['schema']!='rc4-checkpoint/v1':raise ValueError('flow checkpoint identity')
  if len(p['velocity'])!=3*self.count or len(p['pressure'])!=self.count or set(p['metrics'])!={k for k,_ in Metrics._fields_} or set(p['cumulative'])!=set(self.cumulative):raise ValueError('flow checkpoint shape')
  candidate=Coupled.__new__(Coupled);candidate.__dict__=self.__dict__.copy();Spatial.restore(candidate,p['spatial'])
  candidate.velocity=(C.c_double*(3*self.count))(*p['velocity']);candidate.pressure=(C.c_double*self.count)(*p['pressure']);candidate.metrics=Metrics(**p['metrics']);candidate.cumulative=p['cumulative'].copy()
  if any(not math.isfinite(x) for x in list(p['metrics'].values())+list(p['cumulative'].values())) or p['cumulative']['dissipation']<0 or p['cumulative']['time_defect']<0:raise ValueError('flow ledger')
  re,contrast,div=candidate.admit_fields()
  if any(getattr(candidate.metrics,k)<0 for k in ('divergence','reynolds','contrast','kinetic','dissipation','time_defect','momentum_residual')):raise ValueError('flow metric sign')
  if abs(candidate.metrics.reynolds-re)>1e-12 or abs(candidate.metrics.contrast-contrast)>1e-12 or abs(candidate.metrics.divergence-div)>1e-8:raise ValueError('flow metric witness')
  r=candidate.report();scale=max(1e-12,abs(candidate.initial_kinetic),abs(p['cumulative']['work']))
  if abs(r['flow']['energy_ledger_residual_j'])>1e-9*scale:raise ValueError('flow energy ledger')
  self.__dict__=candidate.__dict__
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--library',required=True);p.add_argument('--config',required=True);p.add_argument('--steps',type=int,default=0);p.add_argument('--resume');p.add_argument('--checkpoint');a=p.parse_args()
 if not 0<=a.steps<=10000:p.error('steps')
 r=Coupled(a.library,json.loads(Path(a.config).read_text()))
 if a.resume:r.restore(json.loads(Path(a.resume).read_text()))
 print(json.dumps(r.report(),allow_nan=False))
 for _ in range(a.steps):r.advance(1);print(json.dumps(r.report(),allow_nan=False))
 if a.checkpoint:
  with open(a.checkpoint,'x') as f:json.dump(r.checkpoint(),f,allow_nan=False)
if __name__=='__main__':main()
