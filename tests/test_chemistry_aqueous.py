import cmath,copy,json,math,os,subprocess,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from chemistry_aqueous import Aqueous,example
from chemistry_aqueous_data import load_data,qualification
from chemistry_reactor import digest
from chemistry_workspace import execute,packet,restore
LIB=os.environ['CHEMISTRY_AQUEOUS_LIBRARY']
class Tests(unittest.TestCase):
 def test_held_out_native_observations_and_units(self):
  p=load_data();q=qualification();self.assertTrue(q['passed']);self.assertLess(q['relative_published_rate_error'],.1)
  m=Aqueous(LIB,example(False));last=0
  for row in q['held_out']:
   steps=round((row['time_min']-4)*60/30);m.advance(steps-last);last=steps;actual=m.report()['aqueous']['rotation_deg_saccharimeter'];self.assertAlmostEqual(actual,row['predicted'],places=9);self.assertLess(abs(actual-row['observed']),.2)
  self.assertAlmostEqual(p['k_forward']/p['k_reverse'],p['K'],places=14);self.assertLess(abs(p['k_forward']/(.00052*math.log(10)/60)-1),.1);print('held-out qualification',q)
 def test_local_exact_solution_and_fourth_order(self):
  errors=[];p=load_data();eq=1/(1+p['K']);T=20000
  for dt in (2000,1000,500):
   c=example(False);c['dt_s']=dt;c['initial_alpha_fraction']=1.;m=Aqueous(LIB,c);m.advance(T//dt);fraction=m.report()['aqueous']['alpha_fraction'][0];expected=eq+(1-eq)*math.exp(-p['rate_s']*T);errors.append(abs(fraction-expected))
   self.assertLess(max(map(abs,m.report()['element_residual_mol'])),1e-12);self.assertEqual(m.report()['charge_residual_mol'],0)
  self.assertGreater(errors[0]/errors[1],12);self.assertGreater(errors[1]/errors[2],12);print('aqueous RK4 errors',errors)
 def test_detailed_balance_and_gibbs(self):
  p=load_data();eq=1/(1+p['K'])
  for initial in (0.,eq,1.):
   c=example(False);c.update(initial_alpha_fraction=initial,dt_s=1000.);m=Aqueous(LIB,c);previous=m.report()['aqueous']['relative_gibbs_j'];first=list(m.core.s.amount)
   for _ in range(200):
    m.advance(1);g=m.report()['aqueous']['relative_gibbs_j'];self.assertGreaterEqual(g,-1e-12);self.assertLessEqual(g,previous+1e-12);previous=g
   self.assertLess(abs(m.report()['aqueous']['alpha_fraction'][0]-eq),.002);self.assertFalse(m.report()['aqueous']['within_empirical_kinetic_window'])
   if initial==eq:self.assertLess(max(abs(a-b) for a,b in zip(first,m.core.s.amount)),1e-14);self.assertLess(abs(m.report()['aqueous']['net_forward_mol_s']),1e-20)
 def test_uncertainty_is_limited_and_not_confidence(self):
  p=load_data();q=qualification();self.assertLess(p['rate_rounding_range_s'][0],p['rate_s']);self.assertGreater(p['rate_rounding_range_s'][1],p['rate_s']);self.assertTrue(all(not r['within_rounding_only_range'] for r in q['held_out']))
  self.assertLess(q['approximate_equilibrium_fraction_error'],.03);r=Aqueous(LIB,example(False)).report()['aqueous'];self.assertIsNone(r['reaction_enthalpy_j_mol']);self.assertIsNone(r['uncertainty']['experimental_measurement_uncertainty']);self.assertIn('unquantified',r['uncertainty']['status']);self.assertAlmostEqual(r['conditional_delta_g0_j_mol'],-8.31446261815324*274.65*math.log(p['K']))
 def grid(self,n,dt=30):
  c=example();c['n']=n;c['dt_s']=dt;c['spacing_m']=[.08/n[0],.01,.01];v=math.prod(c['spacing_m']);p=load_data();eq=1/(1+p['K']);rows=[]
  for q in range(math.prod(n)):
   a=eq+.2*math.sin(2*math.pi*(q%n[0]+.5)/n[0]);rows.append([v*p['concentration_mol_m3']*a,v*p['concentration_mol_m3']*(1-a)])
  c['initial_mol']=rows;return c
 def test_1d_2d_3d_discrete_transport_reaction(self):
  p=load_data();eq=1/(1+p['K']);steps=100
  for n in ([8,1,1],[8,4,1],[8,4,3]):
   c=self.grid(n);c['diffusion_m2_s']=1e-8;c['velocity_m_s']=[1e-6,0,0];m=Aqueous(LIB,c);m.advance(steps);h=c['spacing_m'][0];theta=2*math.pi/8;z=30*(1e-6/h*(cmath.exp(-1j*theta)-1)+1e-8/h**2*(2*math.cos(theta)-2));x=-p['rate_s']*15;reaction=1+x+x*x/2+x**3/6+x**4/24;factor=((1+z+z*z/2)*reaction**2)**steps
   expected=[eq+.2*(factor*cmath.exp(1j*theta*(q%8+.5))).imag for q in range(math.prod(n))];r=m.report();self.assertLess(max(abs(a-b) for a,b in zip(r['aqueous']['alpha_fraction'],expected)),1e-11);self.assertLess(max(map(abs,r['species_budget_residual_mol'])),1e-12)
 def test_spatial_continuum_refinement(self):
  p=load_data();eq=1/(1+p['K']);errors=[]
  for N in (8,16):
   c=self.grid([N,1,1]);c['diffusion_m2_s']=1e-8;m=Aqueous(LIB,c);m.advance(100);factor=math.exp(-(p['rate_s']+1e-8*(2*math.pi/.08)**2)*3000);exact=[eq+.2*math.sin(2*math.pi*(q+.5)/N)*factor for q in range(N)];errors.append(sum(abs(a-b) for a,b in zip(m.report()['aqueous']['alpha_fraction'],exact))/N)
  self.assertGreater(errors[0]/errors[1],3.5);self.assertLess(errors[-1],.0005);print('aqueous diffusion continuum errors',errors)
 def test_condition_and_data_refusals(self):
  for key,value in [('temperature_k',293.15),('ph',7.),('cosolvent_mass_fraction',.1),('added_catalyst',True),('dataset_sha256','bad'),('ionic_strength_mol_l',.1)]:
   c=example();c['aqueous'][key]=value
   with self.assertRaises(ValueError):Aqueous(LIB,c)
  c=example();c['initial_mol'][0][0]*=2
  with self.assertRaises(ValueError):Aqueous(LIB,c)
  c=example(False);c['dt_s']=1e8
  with self.assertRaises(ValueError):Aqueous(LIB,c)
 def test_exact_restart_fresh_process_and_atomic_refusal(self):
  for c in (example(False),self.grid([8,4,3])):
   m=Aqueous(LIB,c);m.advance(5);p=m.checkpoint();m.advance(7)
   with tempfile.TemporaryDirectory() as root:
    root=Path(root);(root/'config.json').write_text(json.dumps(c));(root/'checkpoint.json').write_text(json.dumps(p));lines=subprocess.check_output([sys.executable,'-B',str(Path(__file__).resolve().parents[1]/'scripts/chemistry_aqueous.py'),'--library',LIB,'--config',str(root/'config.json'),'--resume',str(root/'checkpoint.json'),'--steps','7'],text=True).splitlines();self.assertEqual(json.loads(lines[-1]),m.report())
   target=Aqueous(LIB,c);before=target.checkpoint();bad=copy.deepcopy(p);bad['payload']['model_code']='changed';bad['sha256']=digest(bad['payload'])
   with self.assertRaises(ValueError):target.restore(bad)
   self.assertEqual(target.checkpoint(),before)
 def test_failed_transport_tick_is_atomic(self):
  m=Aqueous(LIB,self.grid([8,4,3]));before=m.checkpoint();m.core.g.diffusion=100
  with self.assertRaises(ValueError):m.advance(1)
  self.assertEqual(m.checkpoint(),before)
 def test_workspace_save_open_and_continue(self):
  c=self.grid([8,4,3]);m=Aqueous(LIB,c);s=packet(m);s=execute(LIB,dict(token=1,action='advance',args=dict(session=s,steps=10)))['session'];m.advance(10);self.assertEqual(s['report'],m.report())
  with tempfile.TemporaryDirectory() as root:
   path=str(Path(root)/'saved.json');execute(LIB,dict(token=2,action='save',args=dict(session=s,path=path)));opened=execute(LIB,dict(token=3,action='open',args=dict(path=path)))['session'];self.assertEqual(opened,s);self.assertEqual(restore(LIB,opened).report(),m.report())
if __name__=='__main__':unittest.main()
