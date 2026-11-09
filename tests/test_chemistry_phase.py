import cmath,copy,ctypes as C,decimal as dec,json,math,os,subprocess,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from chemistry_phase import Partition,Model,properties,example,R
from chemistry_reactor import digest
from chemistry_workspace import execute,packet,restore
LIB=os.environ['CHEMISTRY_PHASE_LIBRARY']
def coefficients(m):
 G=m.model.area*m.model.permeance;return G*R*298.15/m.model.gas_volume,G/(m.p['H']*m.model.liquid_volume)
def exponential(matrix,vector,time):
 a,b,c,d=matrix;mean=(a+d)/2;delta=cmath.sqrt(((a-d)/2)**2+b*c);z=delta*time;scale=cmath.sinh(z)/delta if delta else time;factor=cmath.exp(mean*time);co=cmath.cosh(z)
 return [factor*((co+scale*(a-mean))*vector[0]+scale*b*vector[1]),factor*(scale*c*vector[0]+(co+scale*(d-mean))*vector[1])]
class Tests(unittest.TestCase):
 def test_property_conventions_and_scenarios(self):
  p=properties();self.assertLess(abs(p['H']/(.0013*997/1e5)-1),.05);self.assertLess(abs(p['slope']/1500-1),.05);self.assertAlmostEqual(p['delta_u'],p['delta_h']+R*298.15,places=10)
  f=p['data']['fit'];T=298.15;eps=.001
  def logH(t):return f['A']+f['B_k']/t+f['C']*math.log(t)
  slope=(logH(T+eps)-logH(T-eps))/(1/(T+eps)-1/(T-eps));self.assertLess(abs(slope-p['slope']),1e-5)
  for variant in ('rettich_fit','millero_rounded','sherwood_rounded'):
   c=example(True);c['phase']['property_variant']=variant;m=Partition(LIB,c);m.advance(10000);ng,nl=m.core.cells[0].amount[:2];self.assertAlmostEqual((nl/m.model.liquid_volume)/(ng*R*298.15/m.model.gas_volume),m.p['H'],places=14)
 def test_finite_pair_decimal_and_gross_extents(self):
  dec.getcontext().prec=70
  for dt in (1e-8,.1,100,1000):
   c=example(True);c['dt_s']=dt;m=Partition(LIB,c);ng,nl=m.core.cells[0].amount[:2];a,b=map(dec.Decimal.from_float,coefficients(m));t=dec.Decimal.from_float(dt);N=dec.Decimal.from_float(ng+nl);L0=dec.Decimal.from_float(nl);leq=N*a/(a+b);e=(-(a+b)*t).exp();L=leq+(L0-leq)*e;integ_l=leq*t+(L0-leq)*(1-e)/(a+b);forward=a*(N*t-integ_l);reverse=b*integ_l;m.advance(1);s=m.core.cells[0]
   self.assertLess(abs(s.amount[1]-float(L)),1e-20);self.assertLess(abs(s.extent[0]-float(forward)),1e-20);self.assertLess(abs(s.extent[1]-float(reverse)),1e-20);self.assertAlmostEqual(sum(s.amount[:2]),ng+nl,places=20)
 def test_energy_sign_degassing_and_free_energy(self):
  for degas in (False,True):
   c=example(True)
   if degas:c['initial_mol']=[[0.,properties()['H']*5000*5e-7]]
   m=Partition(LIB,c);a,b=coefficients(m);total=sum(c['initial_mol'][0]);eq=[total*b/(a+b),total*a/(a+b)]
   def free():return R*298.15*sum(n*math.log(n/e) for n,e in zip(m.core.cells[0].amount[:2],eq) if n)
   previous=free()
   for _ in range(100):m.advance(1);value=free();self.assertLessEqual(value,previous+1e-15);previous=value
   r=m.report()['phase'];self.assertLess(abs(r['energy_residual_j']),1e-15);self.assertEqual(r['bath_received_j'],-r['external_bath_heat_j']);self.assertGreater(r['bath_received_j']*(-1 if degas else 1),0)
 def test_area_permeance_and_zero_exchange(self):
  base=example(True);base['dt_s']=10;a=Partition(LIB,base);a.advance(1)
  for key in ('interface_area_m2','permeance_mol_m2_s_pa'):
   c=copy.deepcopy(base);c['phase'][key]*=2;c['dt_s']=5;b=Partition(LIB,c);b.advance(1);self.assertLess(max(abs(x-y) for x,y in zip(a.core.cells[0].amount,b.core.cells[0].amount)),1e-20)
   c['phase'][key]=0;b=Partition(LIB,c);before=bytes(b.core.cells);b.advance(100);self.assertEqual(bytes(b.core.cells),before);self.assertEqual(b.report()['phase']['external_bath_heat_j'],0)
 def test_occupied_phase_volume_and_finite_reservoir(self):
  for f in (.1,.5,.9):
   c=example(True);c['phase']['liquid_fraction']=f;V=math.prod(c['spacing_m']);c['initial_mol']=[[5000*V*(1-f)/(R*298.15),0.]];m=Partition(LIB,c);m.advance(10000);r=m.report();s=m.core.cells[0]
   self.assertEqual(r['concentration_mol_m3'][0],[s.amount[0]/(V*(1-f)),s.amount[1]/(V*f)]);self.assertLess(r['phase']['partial_pressure_pa'][0],5000);self.assertAlmostEqual(sum(s.amount[:2]),sum(c['initial_mol'][0]),places=19)
 def grid(self,n):
  c=example();c['n']=n;c['spacing_m']=[.08/n[0],.01,.01];V=math.prod(c['spacing_m']);c['phase']['interface_area_m2']=100*V;pg=4000;pl=2000;p=properties();c['initial_mol']=[[pg*V*.5/(R*298.15)*(1+.2*math.sin(2*math.pi*(q%n[0]+.5)/n[0])),p['H']*V*.5*pl*(1+.1*math.sin(2*math.pi*(q%n[0]+.5)/n[0]))] for q in range(math.prod(n))];return c
 def test_coupled_1d_2d_3d_matrix_reference_and_time(self):
  errors=[]
  for dt in (.2,.1,.05):
   worst=0
   for n in ([8,1,1],[8,4,1],[8,4,3]):
    c=self.grid(n);c['dt_s']=dt;f=c['phase'];f.update(gas_velocity_m_s=[1e-4,0,0],liquid_velocity_m_s=[-5e-5,0,0],gas_diffusion_m2_s=2e-6,liquid_diffusion_m2_s=1e-7);m=Partition(LIB,c);a,b=coefficients(m);T=2.;h=c['spacing_m'][0];theta=2*math.pi/8
    def symbol(u,D):return abs(u)/h*(cmath.exp((-1j if u>=0 else 1j)*theta)-1)+D/h**2*(2*math.cos(theta)-2)
    V=math.prod(c['spacing_m']);mean=[4000*V*.5/(R*298.15),m.p['H']*V*.5*2000];background=exponential([-a,b,a,-b],mean,T);amplitude=exponential([symbol(1e-4,2e-6)-a,b,a,symbol(-5e-5,1e-7)-b],[mean[0]*.2,mean[1]*.1],T);m.advance(round(T/dt))
    for q,s in enumerate(m.core.cells):
     for phase in range(2):expected=background[phase].real+(amplitude[phase]*cmath.exp(1j*theta*(q%8+.5))).imag;worst=max(worst,abs(s.amount[phase]-expected)/mean[0])
    self.assertLess(abs(m.report()['phase']['energy_residual_j']),1e-13)
   errors.append(worst)
  self.assertGreater(errors[0]/errors[1],3.5);self.assertGreater(errors[1]/errors[2],3.5);self.assertLess(errors[-1],1e-7);print('phase coupled time errors',errors)
 def test_spatial_diffusion_refinement(self):
  errors=[]
  for N in (8,16):
   c=self.grid([N,1,1]);c['phase'].update(gas_diffusion_m2_s=1e-8,liquid_diffusion_m2_s=2e-9);m=Partition(LIB,c);a,b=coefficients(m);V=math.prod(c['spacing_m']);mean=[4000*V*.5/(R*298.15),m.p['H']*V*.5*2000];back=exponential([-a,b,a,-b],mean,100);k=2*math.pi/.08;amp=exponential([-1e-8*k*k-a,b,a,-2e-9*k*k-b],[.2*mean[0],.1*mean[1]],100);m.advance(100);error=0
   for q,s in enumerate(m.core.cells):error+=abs(s.amount[0]-(back[0].real+(amp[0]*cmath.exp(1j*k*(q+.5)*.08/N)).imag))/mean[0]/N
   errors.append(error)
  self.assertGreater(errors[0]/errors[1],3.5);self.assertLess(errors[-1],.0005);print('phase diffusion spatial errors',errors)
 def test_refusal_atomic_and_conditions(self):
  m=Partition(LIB,self.grid([8,4,3]));before=(bytes(m.core.cells),bytes(m.energy),m.checkpoint());m.grids[0].diffusion=100;out=(C.c_double*m.count)(*([123.]*m.count));ok=m.lib.chem_phase_spatial_step(C.byref(m.model),C.byref(m.grids[0]),C.byref(m.grids[1]),m.core.cells,m.energy,m.config['dt_s'],out);self.assertFalse(ok);self.assertEqual(list(out),[123.]*m.count);self.assertEqual((bytes(m.core.cells),bytes(m.energy),m.checkpoint()),before)
  for k,v in [('energy_basis','extra-heat-source'),('temperature_k',300),('solvent','saltwater'),('salinity_g_kg',35),('data_sha256','bad'),('interface_area_m2',-1),('liquid_fraction',0)]:
   c=example(True);c['phase'][k]=v
   with self.assertRaises(ValueError):Partition(LIB,c)
  c=example(True);c['initial_mol'][0][0]*=3
  with self.assertRaises(ValueError):Partition(LIB,c)
 def test_exact_restart_and_bad_energy_ledger(self):
  for c in (example(True),self.grid([8,4,3])):
   m=Partition(LIB,c);m.advance(5);packet=m.checkpoint();m.advance(7)
   with tempfile.TemporaryDirectory() as root:
    root=Path(root);(root/'c.json').write_text(json.dumps(c));(root/'p.json').write_text(json.dumps(packet));lines=subprocess.check_output([sys.executable,'-B',str(Path(__file__).resolve().parents[1]/'scripts/chemistry_phase.py'),'--library',LIB,'--config',str(root/'c.json'),'--resume',str(root/'p.json'),'--steps','7'],text=True).splitlines();self.assertEqual(json.loads(lines[-1]),m.report())
   target=Partition(LIB,c);before=target.checkpoint();bad=copy.deepcopy(packet);bad['payload']['external'][0]+=1;bad['sha256']=digest(bad['payload'])
   with self.assertRaises(ValueError):target.restore(bad)
   self.assertEqual(target.checkpoint(),before)
 def test_workspace_save_reopen(self):
  m=Partition(LIB,self.grid([8,4,3]));s=packet(m);s=execute(LIB,dict(token=1,action='advance',args=dict(session=s,steps=10)))['session'];m.advance(10);self.assertEqual(s['report'],m.report())
  with tempfile.TemporaryDirectory() as root:
   p=str(Path(root)/'saved.json');execute(LIB,dict(token=2,action='save',args=dict(session=s,path=p)));opened=execute(LIB,dict(token=3,action='open',args=dict(path=p)))['session'];self.assertEqual(opened,s);self.assertEqual(restore(LIB,opened).report(),m.report())
if __name__=='__main__':unittest.main()
