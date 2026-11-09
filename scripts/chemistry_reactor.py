"""RC1 closed isothermal reactor CLI. No transport or energy solver."""
import argparse,ctypes as C,hashlib,json,math
from pathlib import Path
class Mechanism(C.Structure):
 _fields_=[('species',C.c_int),('reactions',C.c_int),('elements',C.c_int),('id',(C.c_char*32)*16),('atoms',(C.c_int*4)*16),('charge',C.c_int*16),('atomic_mass',C.c_double*4),('reactant',(C.c_int*16)*32),('product',(C.c_int*16)*32),('order',C.c_int*32),('k',C.c_double*32)]
class State(C.Structure):
 _fields_=[('amount',C.c_double*16),('extent',C.c_double*32)]
def digest(x):return hashlib.sha256(json.dumps(x,sort_keys=True,allow_nan=False,separators=(',',':')).encode()).hexdigest()
def fixture(mode='network'):
 species=[dict(id=n,atoms=a,charge=0) for n,a in zip('ABCDEFG',[[1,0],[1,0],[0,1],[0,1],[1,1],[1,1],[2,1]])]
 reactions=[([1,0,0,0,0,0,0],[0,1,0,0,0,0,0],.1),([0,1,1,0,0,0,0],[0,0,0,0,1,0,0],.2),([0,0,0,0,1,0,0],[0,1,1,0,0,0,0],.05),([0,1,0,0,1,0,0],[0,0,0,0,0,0,1],.08),([1,0,0,0,1,0,0],[0,1,0,0,0,1,0],.03)]
 if mode=='decay':reactions=reactions[:1]
 if mode=='reversible':reactions=[reactions[0],([0,1,0,0,0,0,0],[1,0,0,0,0,0,0],.2)]
 return dict(schema='rc1/v1',phase='synthetic-isothermal',atomic_mass_kg_mol=[.01,.02],species=species,reactions=[dict(reactant=a,product=b,k=k,unit='s^-1' if sum(a)==1 else 'm3 mol^-1 s^-1') for a,b,k in reactions],initial_mol=[1,.2,.8,.1,.05,0,0],volume_m3=1.,dt_s=.02)
def admit(c):
 if set(c)!=set(fixture()) or c['schema']!='rc1/v1' or c['phase']!='synthetic-isothermal':raise ValueError('unsupported schema/property model')
 ns,nr,ne=len(c['species']),len(c['reactions']),len(c['atomic_mass_kg_mol'])
 if not(1<=ns<=16 and 1<=nr<=32 and 1<=ne<=4):raise ValueError('capacity')
 m=Mechanism();m.species,m.reactions,m.elements=ns,nr,ne
 for e,w in enumerate(c['atomic_mass_kg_mol']):m.atomic_mass[e]=w
 for i,s in enumerate(c['species']):
  if set(s)!={'id','atoms','charge'} or len(s['atoms'])!=ne or not s['id'].isascii() or not 0<len(s['id'])<32:raise ValueError('species')
  if type(s['charge'])!=int or not -100<=s['charge']<=100:raise ValueError('charge')
  m.id[i].value=s['id'].encode();m.charge[i]=s['charge']
  for e,a in enumerate(s['atoms']):
   if type(a)!=int or not 0<=a<=100:raise ValueError('integer atoms')
   m.atoms[i][e]=a
 for r,x in enumerate(c['reactions']):
  if set(x)!={'reactant','product','k','unit'} or len(x['reactant'])!=ns or len(x['product'])!=ns:raise ValueError('reaction')
  order=sum(x['reactant']);unit={1:'s^-1',2:'m3 mol^-1 s^-1'}.get(order)
  if x['unit']!=unit or unit is None:raise ValueError('rate unit')
  m.order[r]=order;m.k[r]=x['k']
  for i,(a,b) in enumerate(zip(x['reactant'],x['product'])):
   if type(a)!=int or type(b)!=int or not 0<=a<=8 or not 0<=b<=8:raise ValueError('integer stoichiometry')
   m.reactant[r][i]=a;m.product[r][i]=b
 if len(c['initial_mol'])!=ns or any(not math.isfinite(x) or x<0 for x in c['initial_mol']):raise ValueError('initial')
 if any(not math.isfinite(c[k]) or c[k]<=0 for k in ('volume_m3','dt_s')):raise ValueError('clock/volume')
 return m
class Reactor:
 def __init__(self,library,config):
  self.c=config;self.m=admit(config);self.s=State();self.tick=0
  self.lib=C.CDLL(str(Path(library).resolve()));self.engine=hashlib.sha256(Path(library).read_bytes()).hexdigest()
  for name,args in [('chem_valid',[C.POINTER(Mechanism)]),('chem_rates',[C.POINTER(Mechanism),C.POINTER(State),C.c_double,C.POINTER(C.c_double)]),('chem_step',[C.POINTER(Mechanism),C.POINTER(State),C.c_double,C.c_double])]:
   f=getattr(self.lib,name);f.argtypes=args;f.restype=C.c_bool
  if not self.lib.chem_valid(C.byref(self.m)):raise ValueError('invalid mechanism')
  for i,n in enumerate(config['initial_mol']):self.s.amount[i]=n
 def advance(self,steps):
  if type(steps)!=int or not 0<=steps<=1000000:raise ValueError('steps')
  for _ in range(steps):
   if not self.lib.chem_step(C.byref(self.m),C.byref(self.s),self.c['volume_m3'],self.c['dt_s']):raise ValueError('native step refused; reduce dt or inspect state')
   self.tick+=1
 def report(self):
  n=list(self.s.amount)[:self.m.species];rates=(C.c_double*32)()
  if not self.lib.chem_rates(C.byref(self.m),C.byref(self.s),self.c['volume_m3'],rates):raise ValueError('state')
  initial=self.c['initial_mol'];weights=[sum(a*w for a,w in zip(s['atoms'],self.c['atomic_mass_kg_mol'])) for s in self.c['species']]
  residual=[sum((n[i]-initial[i])*s['atoms'][e] for i,s in enumerate(self.c['species'])) for e in range(self.m.elements)]
  return dict(tick=self.tick,time_s=self.tick*self.c['dt_s'],amount_mol=n,mass_kg=[a*w for a,w in zip(n,weights)],rates_mol_s=list(rates)[:self.m.reactions],extent_mol=list(self.s.extent)[:self.m.reactions],element_residual_mol=residual,charge_residual_mol=sum((n[i]-initial[i])*s['charge'] for i,s in enumerate(self.c['species'])),thermal_model='external isothermal bath; no energy prediction')
 def checkpoint(self):
  p=dict(schema='rc1-checkpoint/v1',engine=self.engine,config=digest(self.c),tick=self.tick,amount=list(self.s.amount)[:self.m.species],extent=list(self.s.extent)[:self.m.reactions]);return dict(payload=p,sha256=digest(p))
 def restore(self,packet):
  p=packet['payload']
  if digest(p)!=packet['sha256'] or set(p)!={'schema','engine','config','tick','amount','extent'} or p['schema']!='rc1-checkpoint/v1' or p['engine']!=self.engine or p['config']!=digest(self.c):raise ValueError('checkpoint identity')
  if type(p['tick'])!=int or p['tick']<0 or len(p['amount'])!=self.m.species or len(p['extent'])!=self.m.reactions or any(not math.isfinite(x) or x<0 for x in p['amount']+p['extent']):raise ValueError('checkpoint state')
  for i,n in enumerate(p['amount']):
   expected=self.c['initial_mol'][i]+sum((r['product'][i]-r['reactant'][i])*x for r,x in zip(self.c['reactions'],p['extent']))
   if abs(n-expected)>1e-11*max(1,abs(expected)):raise ValueError('extent budget')
  candidate=State()
  for i,x in enumerate(p['amount']):candidate.amount[i]=x
  for r,x in enumerate(p['extent']):candidate.extent[r]=x
  rates=(C.c_double*32)()
  if not self.lib.chem_rates(C.byref(self.m),C.byref(candidate),self.c['volume_m3'],rates):raise ValueError('checkpoint admission')
  self.s=candidate;self.tick=p['tick']
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--library',required=True);p.add_argument('--config',required=True);p.add_argument('--steps',type=int,default=0);p.add_argument('--resume');p.add_argument('--checkpoint');a=p.parse_args()
 if not 0<=a.steps<=1000000:p.error('steps must be between 0 and 1000000')
 r=Reactor(a.library,json.loads(Path(a.config).read_text()))
 if a.resume:r.restore(json.loads(Path(a.resume).read_text()))
 print(json.dumps(r.report(),allow_nan=False))
 for _ in range(a.steps):r.advance(1);print(json.dumps(r.report(),allow_nan=False))
 if a.checkpoint:
  with open(a.checkpoint,'x') as f:json.dump(r.checkpoint(),f,allow_nan=False)
if __name__=='__main__':main()
