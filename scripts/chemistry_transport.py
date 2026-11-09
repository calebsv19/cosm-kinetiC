"""RC2 prescribed constant-flow, periodic/sealed uniform-grid experiment."""
import argparse,ctypes as C,hashlib,json,math
from pathlib import Path
from chemistry_reactor import Reactor,State,Mechanism,digest
class Grid(C.Structure):
 _fields_=[('n',C.c_int*3),('boundary',C.c_int*3),('spacing',C.c_double*3),('velocity',C.c_double*3),('diffusion',C.c_double)]
class Spatial:
 def __init__(self,library,config):
  self.config=config
  if set(config)!={'schema','chemistry','n','boundary','spacing_m','velocity_m_s','diffusion_m2_s','dt_s','reacting','initial_mol'} or config['schema']!='rc2/v1':raise ValueError('schema')
  for k in ('n','boundary','spacing_m','velocity_m_s'):
   if len(config[k])!=3:raise ValueError('axes')
  if any(type(x)!=int or not 1<=x<=4096 for x in config['n']) or math.prod(config['n'])>4096:raise ValueError('capacity')
  if any(type(x)!=int or x not in (0,1) for x in config['boundary']):raise ValueError('boundary')
  if type(config['reacting'])!=bool or not math.isfinite(config['dt_s']) or config['dt_s']<=0:raise ValueError('clock/mode')
  local=Reactor(library,config['chemistry']);self.m=local.m;self.lib=local.lib;self.engine=local.engine;self.tick=0
  self.g=Grid((C.c_int*3)(*config['n']),(C.c_int*3)(*config['boundary']),(C.c_double*3)(*config['spacing_m']),(C.c_double*3)(*config['velocity_m_s']),config['diffusion_m2_s'])
  self.lib.chem_grid_valid.argtypes=[C.POINTER(Grid)];self.lib.chem_grid_valid.restype=C.c_bool
  self.lib.chem_spatial_step.argtypes=[C.POINTER(Mechanism),C.POINTER(Grid),C.POINTER(State),C.c_double,C.c_bool];self.lib.chem_spatial_step.restype=C.c_bool
  if not self.lib.chem_grid_valid(C.byref(self.g)):raise ValueError('grid')
  self.count=math.prod(config['n']);self.cells=(State*self.count)()
  if len(config['initial_mol'])!=self.count:raise ValueError('cell count')
  for i,row in enumerate(config['initial_mol']):
   if len(row)!=self.m.species or any(not math.isfinite(x) or x<0 for x in row):raise ValueError('inventory')
   for s,x in enumerate(row):self.cells[i].amount[s]=x
 def advance(self,steps):
  if type(steps)!=int or not 0<=steps<=1000000:raise ValueError('steps')
  for _ in range(steps):
   if not self.lib.chem_spatial_step(C.byref(self.m),C.byref(self.g),self.cells,self.config['dt_s'],self.config['reacting']):raise ValueError('spatial step refused')
   self.tick+=1
 def report(self):
  ns,nr=self.m.species,self.m.reactions;v=math.prod(self.config['spacing_m'])
  amounts=[list(c.amount)[:ns] for c in self.cells];totals=[math.fsum(row[s] for row in amounts) for s in range(ns)]
  initial=[math.fsum(row[s] for row in self.config['initial_mol']) for s in range(ns)]
  ext=[math.fsum(c.extent[r] for c in self.cells) for r in range(nr)]
  residual=[totals[s]-initial[s]-math.fsum((r['product'][s]-r['reactant'][s])*x for r,x in zip(self.config['chemistry']['reactions'],ext)) for s in range(ns)]
  return dict(tick=self.tick,time_s=self.tick*self.config['dt_s'],n=self.config['n'],amount_mol=amounts,concentration_mol_m3=[[x/v for x in row] for row in amounts],totals_mol=totals,extent_mol=ext,species_budget_residual_mol=residual,boundary_transfer_mol=[0.]*ns,boundary_policy='periodic or sealed; no external exchange')
 def checkpoint(self):
  p=dict(schema='rc2-checkpoint/v1',engine=self.engine,config=digest(self.config),tick=self.tick,amount=[list(c.amount)[:self.m.species] for c in self.cells],extent=[list(c.extent)[:self.m.reactions] for c in self.cells]);return dict(payload=p,sha256=digest(p))
 def restore(self,packet):
  p=packet['payload']
  if digest(p)!=packet['sha256'] or set(p)!={'schema','engine','config','tick','amount','extent'} or p['schema']!='rc2-checkpoint/v1' or p['engine']!=self.engine or p['config']!=digest(self.config):raise ValueError('checkpoint identity')
  if type(p['tick'])!=int or p['tick']<0 or len(p['amount'])!=self.count or len(p['extent'])!=self.count:raise ValueError('checkpoint shape')
  cells=(State*self.count)()
  for i in range(self.count):
   for key,width in [('amount',self.m.species),('extent',self.m.reactions)]:
    row=p[key][i]
    if len(row)!=width or any(not math.isfinite(x) or x<0 for x in row):raise ValueError('checkpoint state')
    target=getattr(cells[i],key)
    for j,x in enumerate(row):target[j]=x
  # Global inventories relate to cumulative extents; local extents do not advect.
  for s in range(self.m.species):
   total=math.fsum(c.amount[s] for c in cells);expected=math.fsum(row[s] for row in self.config['initial_mol'])+math.fsum((r['product'][s]-r['reactant'][s])*math.fsum(c.extent[k] for c in cells) for k,r in enumerate(self.config['chemistry']['reactions']))
   if abs(total-expected)>1e-11*max(1,abs(expected)):raise ValueError('checkpoint budget')
  self.cells=cells;self.tick=p['tick']
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--library',required=True);p.add_argument('--config',required=True);p.add_argument('--steps',type=int,default=0);p.add_argument('--resume');p.add_argument('--checkpoint');a=p.parse_args()
 if not 0<=a.steps<=1000000:p.error('steps range')
 r=Spatial(a.library,json.loads(Path(a.config).read_text()))
 if a.resume:r.restore(json.loads(Path(a.resume).read_text()))
 print(json.dumps(r.report(),allow_nan=False))
 for _ in range(a.steps):r.advance(1);print(json.dumps(r.report(),allow_nan=False))
 if a.checkpoint:
  with open(a.checkpoint,'x') as f:json.dump(r.checkpoint(),f,allow_nan=False)
if __name__=='__main__':main()
