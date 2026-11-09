import copy,ctypes as C,json,math,os,subprocess,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from chemistry_reactor import fixture
from chemistry_thermal import LocalThermal,ThermalCoupled
LIB=os.environ['CHEMISTRY_THERMAL_LIBRARY'];os.environ.setdefault('CHEMISTRY_FLOW_LIBRARY',LIB)
from test_chemistry_flow import config as flow_config

def thermo(count=1,activation=0,formation=3000):
 return dict(model='synthetic-caloric/v1',phase='synthetic-single-liquid',energy_basis='formation-inclusive-internal',reference_temperature_k=300.,valid_temperature_k=[250.,400.],carrier_cv_j_m3_k=1000.,formation_j_mol=[formation]+[0.]*6,cv_j_mol_k=[10.]*7,activation_j_mol=[activation],conductivity_w_m_k=0.,bath_temperature_k=300.,bath_conductance_w_m3_k=0.,power_w_m3=0.,thermal_expansion_1_k=0.,provenance=dict(id='rc5-synthetic-caloric-1',source='Analytic engineering fixture; no material calibration',uncertainty='synthetic / not experimentally calibrated'),initial_temperature_k=[300.]*count)
def local(dt=.02,activation=0):
 c=fixture('decay');c.update(schema='rc5-local/v1',initial_mol=[1.,0,0,0,0,0,0],dt_s=dt);c['reactions'][0]['k']=.4;c['thermal']=thermo(activation=activation);return c
def spatial(N=4,dt=.002):
 c=flow_config(N=N,dt=dt,D=.01,beta=0);c['schema']='rc5/v1';c['reacting']=True;c['chemistry']=fixture('decay');c['chemistry']['reactions'][0]['k']=.4;c['thermal']=thermo(N**3);c['initial_mol']=[[1/N**3]+[0.]*6 for _ in range(N**3)];return c

def arrhenius_reference(time):
 # Separate variable: z=log(A0/A). Time is integral dz/k(T(z)).
 def elapsed(z):
  count=1000;total=0
  for i in range(count+1):
   x=z*i/count;T=300+3000*(1-math.exp(-x))/1010;k=.4*math.exp(-12000/8.31446261815324*(1/T-1/300));total+=(1 if i in (0,count) else 4 if i%2 else 2)/k
  return total*z/(3*count)
 lo,hi=0.,4.
 for _ in range(48):
  mid=.5*(lo+hi)
  if elapsed(mid)>time:hi=mid
  else:lo=mid
 return math.exp(-.5*(lo+hi))
class Tests(unittest.TestCase):
 def test_local_adiabatic_and_endothermic(self):
  for sign in (1,-1):
   c=local();c['thermal']['formation_j_mol'][0]=sign*3000;r=LocalThermal(LIB,c);r.advance(200);A=math.exp(-.4*4)
   self.assertAlmostEqual(r.s.amount[0],A,places=10);self.assertAlmostEqual(r.temperature(),300+sign*3000*(1-A)/1010,places=8);self.assertEqual(r.energy.value,r.initial_energy);self.assertEqual(r.external,0.)
 def test_arrhenius_reference(self):
  target=arrhenius_reference(4);errors=[]
  for dt in (.2,.1,.05):
   r=LocalThermal(LIB,local(dt,12000));r.advance(round(4/dt));errors.append(abs(r.s.amount[0]-target));T=r.temperature();expected=.4*math.exp(-12000/8.31446261815324*(1/T-1/300))*r.s.amount[0];self.assertAlmostEqual(r.report()['rates_mol_s'][0],expected,places=14)
  self.assertGreater(errors[0]/errors[1],12);self.assertGreater(errors[1]/errors[2],12);self.assertLess(errors[-1],1e-8);print('Arrhenius local errors',errors)
 def test_bath_and_power(self):
  c=local();c['reactions'][0]['k']=0;t=c['thermal'];t.update(bath_temperature_k=310.,bath_conductance_w_m3_k=100.,power_w_m3=50.);r=LocalThermal(LIB,c);r.advance(200);equilibrium=310.5;expected=equilibrium+(300-equilibrium)*math.exp(-100*4/1010)
  self.assertAlmostEqual(r.temperature(),expected,places=9);self.assertLess(abs(r.report()['thermal']['energy_residual_j']),1e-9)
 def test_conduction_reference(self):
  errors=[]
  for N in (4,8,12):
   c=spatial(N,dt=.001);c['reacting']=False;c['diffusion_m2_s']=0;c['thermal']['conductivity_w_m_k']=1
   c['thermal']['initial_temperature_k']=[300+math.sin(math.pi/N)/(math.pi/N)*math.sin(2*math.pi*(q%N+.5)/N) for q in range(N**3)]
   r=ThermalCoupled(LIB,c);r.advance(100);temps=r.temperatures();exact=[300+(T-300)*math.exp(-4*math.pi**2*.1/1010) for T in c['thermal']['initial_temperature_k']];errors.append(sum(abs(a-b) for a,b in zip(temps,exact))/r.count);self.assertLess(abs(r.report()['thermal']['internal_ledger_residual_j']),1e-8)
  self.assertGreater(errors[0]/errors[1],2.8);self.assertGreater(errors[1]/errors[2],1.7);print('thermal conduction spatial errors',errors)
 def test_thermal_advection_reference(self):
  errors=[]
  for N in (8,16):
   c=spatial(N,dt=.01);c['reacting']=False;c['diffusion_m2_s']=0;c['flow']['initial_velocity_m_s']=[.001]*(N**3)+[0.]*(2*N**3)
   c['thermal']['initial_temperature_k']=[300+math.sin(math.pi/N)/(math.pi/N)*math.sin(2*math.pi*(q%N+.5)/N) for q in range(N**3)]
   r=ThermalCoupled(LIB,c);r.advance(100);exact=[300+math.sin(math.pi/N)/(math.pi/N)*math.sin(2*math.pi*((q%N+.5)/N-.001)) for q in range(N**3)];errors.append(sum(abs(a-b) for a,b in zip(r.temperatures(),exact))/r.count)
   import cmath
   z=.01*.001*N*(cmath.exp(-2j*math.pi/N)-1);factor=(1+z+.5*z*z)**100
   discrete=[300+math.sin(math.pi/N)/(math.pi/N)*(factor*cmath.exp(2j*math.pi*(q%N+.5)/N)).imag for q in range(N**3)]
   self.assertLess(max(abs(a-b) for a,b in zip(r.temperatures(),discrete)),1e-9);self.assertLess(abs(r.report()['thermal']['internal_ledger_residual_j']),1e-8)
  print('thermal advection spatial errors',errors);self.assertGreater(errors[0]/errors[1],1.8);self.assertLess(errors[1],.001)
 def test_species_energy_transport_uniform_temperature(self):
  c=spatial();c['reacting']=False;c['thermal']['initial_temperature_k']=[310.]*64
  for q,row in enumerate(c['initial_mol']):row[0]=(1+.2*math.sin(2*math.pi*(q%4+.5)/4))/64
  r=ThermalCoupled(LIB,c);r.advance(30);self.assertLess(max(abs(T-310) for T in r.temperatures()),1e-10);self.assertLess(abs(r.report()['thermal']['internal_ledger_residual_j']),1e-8)
 def test_physical_viscous_heat_excludes_numerical(self):
  c=spatial();c['reacting']=False;c['diffusion_m2_s']=0
  for q in range(64):c['flow']['initial_velocity_m_s'][q]=.005*math.sin(2*math.pi*(((q//4)%4)+.5)/4)
  r=ThermalCoupled(LIB,c);r.advance(50);t=r.report()['thermal'];delta=t['total_internal_j']-r.initial_internal
  self.assertAlmostEqual(delta,r.cumulative['dissipation'],places=8);self.assertGreater(r.cumulative['time_defect'],1e-7);self.assertGreater(abs(delta-r.cumulative['dissipation']-r.cumulative['time_defect']),1e-7);self.assertEqual(t['numerical_heat_added_j'],0.);self.assertLess(abs(t['total_ledger_residual_j']),1e-8);print('viscous/numerical J',r.cumulative)
 def test_thermal_buoyancy_and_reaction_feedback(self):
  c=spatial();c['reacting']=False;c['thermal']['thermal_expansion_1_k']=.001;c['thermal']['initial_temperature_k']=[300+.5*math.sin(2*math.pi*(q%4+.5)/4) for q in range(64)];r=ThermalCoupled(LIB,c);r.advance(10)
  self.assertGreater(sum(r.velocity[64+q]*math.sin(2*math.pi*(q%4+.5)/4) for q in range(64)),0)
  c=spatial();c['reacting']=True;c['thermal']['activation_j_mol']=[12000.];c['thermal']['thermal_expansion_1_k']=.001;c['thermal']['initial_temperature_k']=[300+.2*math.sin(2*math.pi*(q%4+.5)/4) for q in range(64)];r=ThermalCoupled(LIB,c);r.advance(10);self.assertTrue(all(T>299 for T in r.temperatures()));self.assertGreater(r.kinetic(),0);self.assertLess(abs(r.report()['thermal']['total_ledger_residual_j']),1e-8)
 def test_thermal_forced_flow_convergence(self):
  errors=[];lam=32.;decay=lam/1010;damping=.1*lam;time=.04
  exact=.00005*(math.exp(-decay*time)-math.exp(-damping*time))/(damping-decay)
  for dt in (.004,.002,.001):
   c=spatial(dt=dt);c['reacting']=False;c['diffusion_m2_s']=0;c['thermal']['conductivity_w_m_k']=1;c['thermal']['thermal_expansion_1_k']=.0001
   c['thermal']['initial_temperature_k']=[300+.5*math.sin(2*math.pi*(q%4+.5)/4) for q in range(64)]
   r=ThermalCoupled(LIB,c);r.advance(round(time/dt));amplitude=sum(r.velocity[64+q]*math.sin(2*math.pi*(q%4+.5)/4) for q in range(64))/32;errors.append(abs(amplitude-exact))
  self.assertGreater(errors[0]/errors[1],1.8);self.assertGreater(errors[1]/errors[2],1.8);self.assertLess(errors[-1],1e-8);print('thermal forced-flow time errors',errors)
 def test_refusal_atomic_and_metadata(self):
  c=spatial();r=ThermalCoupled(LIB,c);before=(bytes(r.cells),bytes(r.velocity),bytes(r.pressure),bytes(r.energy));r.thermo.power=1e12
  with self.assertRaises(ValueError):r.advance(1)
  self.assertEqual((bytes(r.cells),bytes(r.velocity),bytes(r.pressure),bytes(r.energy)),before)
  for modify in (lambda c:c['thermal'].update(energy_basis='add-reaction-heat'),lambda c:c['thermal'].update(provenance={}),lambda c:c['thermal'].update(phase='gas'),lambda c:c['thermal']['cv_j_mol_k'].__setitem__(0,-1),lambda c:c['thermal']['initial_temperature_k'].__setitem__(0,500)):
   bad=local();modify(bad)
   with self.assertRaises(ValueError):LocalThermal(LIB,bad)
 def test_exact_local_and_spatial_restart(self):
  coupled=spatial();coupled["flow"]["beta_m3_mol"][0]=.001
  for c,klass in ((local(activation=12000),LocalThermal),(coupled,ThermalCoupled)):
   r=klass(LIB,c);r.advance(5);p=r.checkpoint();r.advance(5)
   with tempfile.TemporaryDirectory() as t:
    t=Path(t);(t/'c.json').write_text(json.dumps(c));(t/'p.json').write_text(json.dumps(p));cli=Path(__file__).resolve().parents[1]/'scripts/chemistry_thermal.py';lines=subprocess.check_output([sys.executable,'-B',str(cli),'--library',LIB,'--config',str(t/'c.json'),'--resume',str(t/'p.json'),'--steps','5'],text=True).splitlines();self.assertEqual(json.loads(lines[-1]),r.report())
   q=klass(LIB,c);before=q.checkpoint();bad=copy.deepcopy(p);bad['sha256']='corrupt'
   with self.assertRaises(ValueError):q.restore(bad)
   self.assertEqual(q.checkpoint(),before)
if __name__=='__main__':unittest.main()
