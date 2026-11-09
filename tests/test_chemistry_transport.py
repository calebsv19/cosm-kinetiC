import copy,ctypes as C,json,math,os,subprocess,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from chemistry_reactor import fixture,digest
from chemistry_transport import Spatial
LIB=os.environ['CHEMISTRY_TRANSPORT_LIBRARY']
def config(n=(16,1,1),dt=.001,diffusion=.02,velocity=(0,0,0),reacting=False):
 h=[1/x for x in n];v=math.prod(h);initial=[]
 for z in range(n[2]):
  for y in range(n[1]):
   for x in range(n[0]):
    factor=1+.2*sum(math.sin(2*math.pi*(i+.5)/N)*math.sin(math.pi/N)/(math.pi/N) for i,N in zip((x,y,z),n) if N>1)
    initial.append([v*factor]+[0]*6)
 return dict(schema='rc2/v1',chemistry=fixture(),n=list(n),boundary=[0]*3,spacing_m=h,velocity_m_s=list(velocity),diffusion_m2_s=diffusion,dt_s=dt,reacting=reacting,initial_mol=initial)
def coupled_reference(c,dt):
 n=c['n'][0];v=math.prod(c['spacing_m']);h=c['spacing_m'][0];d=c['diffusion_m2_s'];u=c['velocity_m_s'][0];mechanism=c['chemistry']['reactions']
 a=[row[:] for row in c['initial_mol']]
 def rhs(a):
  out=[[0.]*7 for _ in a]
  for i in range(n):
   j=(i+1)%n
   for s in range(7):
    f=u/h*(a[i][s] if u>=0 else a[j][s])+d/h**2*(a[i][s]-a[j][s]);out[i][s]-=f;out[j][s]+=f
   for r in mechanism:
    q=v*r['k']
    for s in range(7):q*=(a[i][s]/v)**r['reactant'][s]
    for s in range(7):out[i][s]+=(r['product'][s]-r['reactant'][s])*q
  return out
 for _ in range(round(.2/dt)):
  k=[]
  for stage in range(4):
   b=a if not stage else [[a[i][s]+dt*(1 if stage==3 else .5)*k[-1][i][s] for s in range(7)] for i in range(n)]
   k.append(rhs(b))
  a=[[a[i][s]+dt*(k[0][i][s]+2*k[1][i][s]+2*k[2][i][s]+k[3][i][s])/6 for s in range(7)] for i in range(n)]
 return a
class Tests(unittest.TestCase):
 def test_diffusion_dimensions(self):
  for dim in (1,2,3):
   errors=[]
   for N in (8,12,16):
    n=tuple([N]*dim+[1]*(3-dim));c=config(n,dt=.001);r=Spatial(LIB,c);r.advance(50);report=r.report();err=0
    for i,row in enumerate(report['concentration_mol_m3']):
     coords=[i%N,(i//N)%N,(i//(N*N))%N]
     exact=1+.2*math.exp(-4*math.pi**2*.02*.05)*math.sin(math.pi/N)/(math.pi/N)*sum(math.sin(2*math.pi*(coords[a]+.5)/N) for a in range(dim))
     err+=abs(row[0]-exact)/r.count
    errors.append(err);self.assertLess(max(map(abs,report['species_budget_residual_mol'])),1e-12)
   self.assertGreater(errors[0]/errors[1],1.9);self.assertGreater(errors[1]/errors[2],1.6)
   print('diffusion dimension/errors',dim,errors)
 def test_advection(self):
  errors=[]
  for N in (16,32,64):
   c=config((N,1,1),dt=.001,diffusion=0,velocity=(.2,0,0));r=Spatial(LIB,c);r.advance(500)
   error=sum(abs(row[0]*N-(1+.2*math.sin(math.pi/N)/(math.pi/N)*math.sin(2*math.pi*((i+.5)/N-.1)))) for i,row in enumerate(r.report()['amount_mol']))/N
   errors.append(error)
  self.assertGreater(errors[0]/errors[1],1.7);self.assertGreater(errors[1]/errors[2],1.8);print('advection errors',errors)
 def test_split_coupled(self):
  c=config((4,1,1),dt=.02,diffusion=.02,velocity=(.15,0,0),reacting=True)
  for i,row in enumerate(c['initial_mol']):c['initial_mol'][i]=[x*.25*(1+.3*math.sin(i+s)) for s,x in enumerate(fixture()['initial_mol'])]
  ref=coupled_reference(c,.0002);ref2=coupled_reference(c,.0001)
  self.assertLess(max(abs(a-b) for row,row2 in zip(ref,ref2) for a,b in zip(row,row2)),1e-12)
  errors=[]
  for dt in (.01,.005,.0025):
   c['dt_s']=dt;r=Spatial(LIB,c);r.advance(round(.2/dt));report=r.report()
   errors.append(max(abs(a-b) for row,row2 in zip(report['amount_mol'],ref2) for a,b in zip(row,row2)))
   self.assertLess(max(map(abs,report['species_budget_residual_mol'])),1e-12)
  self.assertGreater(errors[0]/errors[1],3.5);self.assertGreater(errors[1]/errors[2],3.5);self.assertLess(errors[-1],1e-7);print('coupled split errors',errors)
 def test_temporal_and_directional(self):
  errors=[]
  for dt in (.01,.005,.0025):
   c=config((8,1,1),dt=dt);r=Spatial(LIB,c);r.advance(round(.2/dt))
   lam=-4*.02*64*math.sin(math.pi/8)**2
   target=[1+(row[0]*8-1)*math.exp(lam*.2) for row in c['initial_mol']]
   errors.append(max(abs(row[0]*8-e) for row,e in zip(r.report()['amount_mol'],target)))
  self.assertGreater(errors[0]/errors[1],3.9);self.assertGreater(errors[1]/errors[2],3.9);print('transport time errors',errors)
  # Axis permutation invariance checks stride, orientation and negative velocities.
  for axis in range(3):
   n=[1]*3;n[axis]=16;u=[0]*3;u[axis]=-.2
   c=config(tuple(n),dt=.001,diffusion=.01,velocity=tuple(u));r=Spatial(LIB,c);r.advance(30)
   values=r.report()['amount_mol']
   if axis==0:baseline=values
   else:self.assertEqual(values,baseline)
 def test_sealed_positivity_and_rollback(self):
  c=config((16,1,1),dt=.01,diffusion=.02);c['boundary']=[1]*3
  for i in range(16):c['initial_mol'][i]=[1/16 if i<8 else 0]+[0]*6
  r=Spatial(LIB,c);r.advance(30);self.assertTrue(all(x>=0 for row in r.report()['amount_mol'] for x in row));self.assertAlmostEqual(r.report()['totals_mol'][0],.5,places=13)
  before=bytes(r.cells)
  self.assertFalse(r.lib.chem_spatial_step(C.byref(r.m),C.byref(r.g),r.cells,100,True));self.assertEqual(bytes(r.cells),before)
  # Transport disabled: negative intermediate chemistry in a later cell rolls back all cells.
  r.g.diffusion=0;r.m.k[0]=1000
  self.assertFalse(r.lib.chem_spatial_step(C.byref(r.m),C.byref(r.g),r.cells,.01,True));self.assertEqual(bytes(r.cells),before)
  for mutate in [lambda x:x.update(n=[4097,1,1]),lambda x:x.update(diffusion_m2_s=-1),lambda x:x.update(velocity_m_s=[1,0,0]),lambda x:x.update(boundary=[2,0,0])]:
   bad=copy.deepcopy(c);mutate(bad)
   with self.assertRaises(ValueError):Spatial(LIB,bad)
 def test_restart(self):
  c=config((4,3,2),dt=.002,diffusion=.01,velocity=(.1,-.1,.03),reacting=True);r=Spatial(LIB,c);r.advance(15);p=r.checkpoint();r.advance(15)
  with tempfile.TemporaryDirectory() as temp:
   t=Path(temp);(t/'c.json').write_text(json.dumps(c));(t/'p.json').write_text(json.dumps(p))
   cli=Path(__file__).resolve().parents[1]/'scripts/chemistry_transport.py'
   lines=subprocess.check_output([sys.executable,'-B',str(cli),'--library',LIB,'--config',str(t/'c.json'),'--resume',str(t/'p.json'),'--steps','15'],text=True).splitlines();self.assertEqual(json.loads(lines[-1]),r.report())
  q=Spatial(LIB,c);q.restore(p);before=bytes(q.cells)
  for key,val in [('engine','drift'),('tick',-1),('config','drift')]:
   bad=copy.deepcopy(p);bad['payload'][key]=val;bad['sha256']=digest(bad['payload'])
   with self.assertRaises(ValueError):q.restore(bad)
   self.assertEqual(bytes(q.cells),before)
if __name__=='__main__':unittest.main()
