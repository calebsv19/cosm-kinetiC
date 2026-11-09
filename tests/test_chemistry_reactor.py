import copy,ctypes as C,json,math,os,subprocess,sys,tempfile,unittest
from decimal import Decimal as D,localcontext
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from chemistry_reactor import Reactor,fixture,digest
LIB=os.environ['CHEMISTRY_LIBRARY']
def reference(c,duration,interval=".05"):
 # Independent arbitrary-precision power-series integration of polynomial ODE.
 with localcontext() as ctx:
  ctx.prec=60;n=[D(str(x)) for x in c['initial_mol']];h=D(interval);v=D(str(c['volume_m3']))
  for _ in range(round(duration/float(h))):
   coeff=[[x]+[D(0)]*28 for x in n]
   for degree in range(28):
    deriv=[D(0)]*len(n)
    for r in c['reactions']:
     react=[i for i,a in enumerate(r['reactant']) for _ in range(a)]
     q=coeff[react[0]][degree] if len(react)==1 else sum(coeff[react[0]][j]*coeff[react[1]][degree-j] for j in range(degree+1))/v
     q*=D(str(r['k']))
     for i in range(len(n)):deriv[i]+=(r['product'][i]-r['reactant'][i])*q
    for i in range(len(n)):coeff[i][degree+1]=deriv[i]/D(degree+1)
   n=[sum(a*h**j for j,a in enumerate(row)) for row in coeff]
  return [float(x) for x in n]
class Tests(unittest.TestCase):
 def test_analytic(self):
  for mode in ('decay','reversible'):
   c=fixture(mode);r=Reactor(LIB,c);r.advance(500);t=10
   a=math.exp(-.1*t) if mode=='decay' else .8+.2*math.exp(-.3*t)
   self.assertAlmostEqual(r.s.amount[0],a,places=12)
   self.assertAlmostEqual(r.s.amount[0]+r.s.amount[1],1.2,places=12)
 def test_network_refinement(self):
  c=fixture();ref=reference(c,4);refined=reference(c,4,'.025');self.assertLess(max(abs(a-b) for a,b in zip(ref,refined)),1e-15);errors=[]
  for dt in (.2,.1,.05):
   c['dt_s']=dt;r=Reactor(LIB,c);r.advance(round(4/dt));n=r.report()
   err=max(abs(a-b) for a,b in zip(n['amount_mol'],ref));errors.append(err)
   for a,b in zip(n['amount_mol'],ref):self.assertLess(abs(a-b),1e-12+1e-7*abs(b))
   self.assertLess(max(map(abs,n['element_residual_mol'])),1e-12)
   self.assertEqual(n['amount_mol'][3],.1)
   self.assertAlmostEqual(sum(n['mass_kg']),.01*1.25+.02*.95,places=12)
  self.assertGreater(errors[0]/errors[1],12);self.assertGreater(errors[1]/errors[2],12)
  print('network L-infinity errors:',errors)
 def test_limits(self):
  c=fixture();c['initial_mol']=[0]*7;r=Reactor(LIB,c);r.advance(100);self.assertEqual(list(r.s.amount),[0]*16)
  c=fixture();[x.update(k=0) for x in c['reactions']];r=Reactor(LIB,c);before=bytes(r.s);r.advance(30);self.assertEqual(bytes(r.s),before)
  r=Reactor(LIB,fixture());before=bytes(r.s);r.advance(0);self.assertEqual(bytes(r.s),before)
  self.assertTrue(r.lib.chem_step(C.byref(r.m),C.byref(r.s),1,0));self.assertEqual(bytes(r.s),before)
  c=fixture();c['initial_mol']=[0,1e-9,1,0,0,0,0];r=Reactor(LIB,c);r.advance(1000);self.assertTrue(all(x>=0 for x in r.s.amount));self.assertLess(r.s.amount[6],1e-9)
 def test_refusal(self):
  for modify in [lambda c:c['reactions'][0].update(unit='wrong'),lambda c:c['species'][1].update(id='A'),lambda c:c['species'][0].update(atoms=[2,0]),lambda c:c['reactions'][0]['product'].pop(),lambda c:c.update(phase='aqueous'),lambda c:c['species'][0].update(charge=1),lambda c:c['species'][0].update(atoms=[2**32,0]),lambda c:c['reactions'][0]['reactant'].__setitem__(0,-1)]:
   c=fixture();modify(c)
   with self.assertRaises((ValueError,TypeError)):Reactor(LIB,c)
  r=Reactor(LIB,fixture());before=bytes(r.s)
  for dt in (-1,float('nan'),1000):self.assertFalse(r.lib.chem_step(C.byref(r.m),C.byref(r.s),1,dt));self.assertEqual(bytes(r.s),before)
 def test_restart(self):
  c=fixture();r=Reactor(LIB,c);r.advance(100);p=r.checkpoint();r.advance(100)
  with tempfile.TemporaryDirectory() as tmp:
   config=Path(tmp)/'config.json';checkpoint=Path(tmp)/'state.json';config.write_text(json.dumps(c));checkpoint.write_text(json.dumps(p))
   cli=Path(__file__).resolve().parents[1]/'scripts/chemistry_reactor.py'
   lines=subprocess.check_output([sys.executable,'-B',str(cli),'--library',LIB,'--config',str(config),'--resume',str(checkpoint),'--steps','100'],text=True).splitlines()
   self.assertEqual(json.loads(lines[-1]),r.report())
  q=Reactor(LIB,c);q.restore(p);before=q.checkpoint()
  for key,value in [('engine','changed'),('config','changed'),('tick',-1),('amount',[-1]*7)]:
   bad=copy.deepcopy(p);bad['payload'][key]=value;bad['sha256']=digest(bad['payload'])
   with self.assertRaises(ValueError):q.restore(bad)
   self.assertEqual(q.checkpoint(),before)
  for key,value in [('dt_s',.03),('volume_m3',2)]:
   bad=copy.deepcopy(c);bad[key]=value
   with self.assertRaises(ValueError):Reactor(LIB,bad).restore(p)
if __name__=='__main__':unittest.main()
