import copy,ctypes as C,json,math,os,subprocess,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from chemistry_reactor import fixture
from chemistry_flow import Coupled,Metrics
LIB=os.environ['CHEMISTRY_FLOW_LIBRARY']
def config(N=4,dt=.002,D=.01,beta=.001,axis=0):
 chem=fixture()
 for r in chem['reactions']:r['k']=0
 h=1/N;v=h**3;rows=[]
 for z in range(N):
  for y in range(N):
   for x in range(N):
    coord=(x,y,z)[axis];a=1+.2*math.sin(math.pi/N)/(math.pi/N)*math.sin(2*math.pi*(coord+.5)/N)
    rows.append([a*v,.2*v,.8*v,.1*v,.05*v,0,0])
 return dict(schema='rc4/v1',chemistry=chem,n=[N]*3,boundary=[0]*3,spacing_m=[h]*3,velocity_m_s=[0]*3,diffusion_m2_s=D,dt_s=dt,reacting=False,initial_mol=rows,flow=dict(model='periodic-unsteady-stokes-solutal/v1',rho_kg_m3=1000.,nu_m2_s=.1,gravity_m_s2=[0,-1.,0],beta_m3_mol=[beta]+[0]*6,reference_mol_m3=[1,0,0,0,0,0,0],feedback=True,initial_velocity_m_s=[0.]*(3*N**3)))
def amplitude(r,component,axis):
 N=r.config['n'][0];return 2*sum(r.velocity[component*r.count+q]*math.sin(2*math.pi*(((q//(N**axis))%N)+.5)/N) for q in range(r.count))/r.count
class Tests(unittest.TestCase):
 def test_forced_reference_and_time(self):
  errors=[];N=4;lam=4*N*N*math.sin(math.pi/N)**2;A=.2*math.sin(math.pi/N)/(math.pi/N)
  target=-.001*A*(math.exp(-.01*lam*.04)-math.exp(-.1*lam*.04))/((.1-.01)*lam)
  for dt in (.004,.002,.001):
   c=config(N,dt);r=Coupled(LIB,c);r.advance(round(.04/dt));err=abs(amplitude(r,1,0)-target);errors.append(err)
   report=r.report();self.assertLess(max(map(abs,report['species_budget_residual_mol'])),1e-12);self.assertLess(abs(report['flow']['energy_ledger_residual_j']),1e-15);self.assertLess(r.metrics.divergence,1e-8)
  self.assertGreater(errors[0]/errors[1],1.8);self.assertGreater(errors[1]/errors[2],1.8);self.assertLess(errors[-1],2e-8);print('coupled time errors',errors)
 def test_space_and_shear_decay(self):
  errors=[]
  for N in (4,8,12):
   dt=.0002;c=config(N,dt,D=0,beta=0);v=c['flow']['initial_velocity_m_s'];count=N**3
   # u_x(y) at x-lower faces, cell-centered in y, is exactly divergence-free.
   for q in range(count):v[q]=.001*math.sin(2*math.pi*(((q//N)%N)+.5)/N)
   r=Coupled(LIB,c);r.advance(50);target=.001*math.exp(-.1*4*math.pi**2*.01);error=abs(amplitude(r,0,1)-target);errors.append(error)
   lam=4*N*N*math.sin(math.pi/N)**2;discrete=.001/(1+dt*.1*lam)**50;self.assertLess(abs(amplitude(r,0,1)-discrete),1e-11)
  self.assertGreater(errors[0]/errors[1],2.8);self.assertGreater(errors[1]/errors[2],1.7);print('flow continuum spatial errors',errors)
 def test_hydrostatic_sign_and_passive(self):
  c=config(axis=1,D=0);r=Coupled(LIB,c);r.advance(3);self.assertLess(max(map(abs,r.velocity)),1e-12);self.assertGreater(max(map(abs,r.pressure)),1e-5)
  N=4;A=.2*math.sin(math.pi/N)/(math.pi/N);P=1000*.001*A*(1/N)*math.cos(math.pi/N)/(2*math.sin(math.pi/N))
  self.assertLess(max(abs(r.pressure[q]-P*math.cos(2*math.pi*(((q//N)%N)+.5)/N)) for q in range(r.count)),1e-10)
  p=Coupled(LIB,config(D=0));c=config(D=0,beta=-.001);q=Coupled(LIB,c);p.advance(5);q.advance(5)
  self.assertLess(max(abs(a+b) for a,b in zip(p.velocity,q.velocity)),1e-13)
  c=config();c['flow']['feedback']=False
  for i in range(64):c['flow']['initial_velocity_m_s'][i]=.001*math.sin(2*math.pi*(((i//4)%4)+.5)/4)
  c2=copy.deepcopy(c);c2['flow']['beta_m3_mol']=[0]*7
  r=Coupled(LIB,c);s=Coupled(LIB,c2);r.advance(5);s.advance(5);self.assertEqual(bytes(r.velocity),bytes(s.velocity));self.assertEqual(bytes(r.cells),bytes(s.cells))
 def test_solved_transport_and_uniform(self):
  c=config(N=8,D=0,beta=0);N=8
  for q in range(N**3):c['flow']['initial_velocity_m_s'][q]=.005*math.sin(2*math.pi*(((q//N)%N)+.5)/N)
  r=Coupled(LIB,c);initial=bytes(r.cells);r.advance(20);self.assertNotEqual(bytes(r.cells),initial);self.assertGreater(max(map(abs,r.velocity)),.001)
  self.assertLess(max(map(abs,r.report()['species_budget_residual_mol'])),1e-12)
  for row in c['initial_mol']:row[0]=1/N**3
  r=Coupled(LIB,c);r.advance(10);self.assertLess(max(abs(cell.amount[0]-1/N**3) for cell in r.cells),1e-13)
 def test_passive_shear_transport_reference(self):
  errors=[]
  for N in (8,16):
   c=config(N=N,dt=.002,D=0,beta=0);count=N**3
   for q in range(count):c['flow']['initial_velocity_m_s'][q]=.005*math.sin(2*math.pi*(((q//N)%N)+.5)/N)
   r=Coupled(LIB,c);r.advance(20);error=0;T=.04;decay=.1*4*math.pi**2;factor=.005*(1-math.exp(-decay*T))/decay;A=.2*math.sin(math.pi/N)/(math.pi/N)
   for y in range(N):
    for x in range(N):
     exact=0
     for j in range(21):
      yy=(y+j/20)/N;value=1+A*math.sin(2*math.pi*((x+.5)/N-factor*math.sin(2*math.pi*yy)));weight=1 if j in (0,20) else (4 if j%2 else 2);exact+=weight*value/60
     error+=abs(r.cells[x+N*y].amount[0]*N**3-exact)/(N*N)
   errors.append(error)
  self.assertGreater(errors[0]/errors[1],1.7);self.assertLess(errors[1],2e-5);print('solved shear tracer continuum errors',errors)
 def test_transverse_density_feedback(self):
  c=config(beta=.005);c['flow']['gravity_m_s2'][1]=-10
  for q,row in enumerate(c['initial_mol']):row[0]=(1+.2*math.sin(2*math.pi*(q%4+.5)/4)*math.cos(2*math.pi*(((q//4)%4)+.5)/4))/64
  passive=copy.deepcopy(c);passive['flow']['feedback']=False;r=Coupled(LIB,c);p=Coupled(LIB,passive);r.advance(20);p.advance(20)
  difference=max(abs(a.amount[0]-b.amount[0]) for a,b in zip(r.cells,p.cells));self.assertGreater(difference,1e-10);self.assertGreater(r.kinetic(),0);self.assertEqual(p.kinetic(),0)
  self.assertLess(max(map(abs,r.report()['species_budget_residual_mol'])),1e-12);print('two-way composition change mol',difference)
 def test_refusal_atomic(self):
  r=Coupled(LIB,config());before=(bytes(r.cells),bytes(r.velocity),bytes(r.pressure));metrics=Metrics();r.model.beta[0]=1
  self.assertFalse(r.lib.chem_flow_step(C.byref(r.m),C.byref(r.g),C.byref(r.model),r.cells,r.velocity,r.pressure,.002,False,C.byref(metrics)));self.assertEqual((bytes(r.cells),bytes(r.velocity),bytes(r.pressure)),before)
  r.model.beta[0]=.001;r.g.diffusion=100
  self.assertFalse(r.lib.chem_flow_step(C.byref(r.m),C.byref(r.g),C.byref(r.model),r.cells,r.velocity,r.pressure,.002,False,C.byref(metrics)));self.assertEqual((bytes(r.cells),bytes(r.velocity),bytes(r.pressure)),before)
  bad=config();bad['flow']['initial_velocity_m_s'][0]=.02
  with self.assertRaises(ValueError):Coupled(LIB,bad)
 def test_anisotropic_face_mapping(self):
  c=config();c['n']=[4,6,8];c['spacing_m']=[.25,1/6,1/8];vol=1/192;c['initial_mol']=[]
  A=.2*math.sin(math.pi/4)/(math.pi/4)
  for q in range(192):c['initial_mol'].append([(1+A*math.sin(2*math.pi*(q%4+.5)/4))*vol]+[0]*6)
  c['flow']['initial_velocity_m_s']=[0.]*576;c['flow']['gravity_m_s2']=[0,0,-1]
  r=Coupled(LIB,c);r.advance(1);amp=-.002*.001*A/(1+.002*.1*32)
  self.assertLess(max(abs(r.velocity[384+q]-amp*math.sin(2*math.pi*(q%4+.5)/4)) for q in range(192)),1e-12)
  self.assertLess(max(map(abs,r.report()['species_budget_residual_mol'])),1e-12)
 def test_workspace_flow_session(self):
  from chemistry_workspace import execute,restore
  c=config();r=Coupled(LIB,c);r.advance(2);session=dict(schema='rc3-session/v1',config=c,checkpoint=r.checkpoint(),report=r.report())
  response=execute(LIB,dict(token=2,action='advance',args=dict(session=session,steps=3)));r.advance(3);self.assertEqual(response['session']['report'],r.report())
  reset=execute(LIB,dict(token=3,action='reset',args=dict(session=response['session'])))['session'];self.assertEqual(reset['report']['tick'],0);restore(LIB,reset)
 def test_restart(self):
  c=config();c['reacting']=True;c['chemistry']=fixture();r=Coupled(LIB,c);r.advance(5);p=r.checkpoint();r.advance(5)
  with tempfile.TemporaryDirectory() as t:
   t=Path(t);(t/'c.json').write_text(json.dumps(c));(t/'p.json').write_text(json.dumps(p))
   cli=Path(__file__).resolve().parents[1]/'scripts/chemistry_flow.py';result=subprocess.check_output([sys.executable,'-B',str(cli),'--library',LIB,'--config',str(t/'c.json'),'--resume',str(t/'p.json'),'--steps','5'],text=True).splitlines();self.assertEqual(json.loads(result[-1]),r.report())
  s=Coupled(LIB,c);before=s.checkpoint();bad=copy.deepcopy(p);bad['payload']['velocity'][0]=1
  with self.assertRaises(ValueError):s.restore(bad)
  self.assertEqual(s.checkpoint(),before)
if __name__=='__main__':unittest.main()
