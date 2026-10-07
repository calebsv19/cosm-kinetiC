"""Independent translation/error, restart and resource-policy gates for the opt-in scheme."""
import copy,math,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'));sys.path.insert(0,str(ROOT/'tests'))
from open_atmosphere import run,validate
from test_open_atmosphere import request,WORKER
class TransportTests(unittest.TestCase):
 def pulse(self,nz,scheme,dt=.005):
  r=request(nz,dt);n=64*nz;vol=.0625*2/nz
  r['transport_scheme']=scheme;r['properties']['dynamic_viscosity_pa_s']=.00002
  r['initial_face_velocity_m_s'][2*n:]=[1.]*(n+64)
  def concentration(z):return math.cos(math.pi*(z-1)/.6)**2 if abs(z-1)<.3 else 0.
  r['initial_smoke_kg']=[concentration((q//64+.5)*2/nz)*vol for q in range(n)];r['steps']=[r['steps'][0]]*round(.25/dt)
  result=run(r,WORKER);exact=[concentration((q//64+.5)*2/nz-.25)*vol for q in range(n)]
  return r,result,math.fsum(abs(a-b) for a,b in zip(exact,result['fields']['smoke_kg']))
 def test_translation_less_dissipative_and_refines(self):
  errors={}
  for n in (16,32):
   for scheme in ('upwind','muscl_minmod'):
    r,result,error=self.pulse(n,scheme);errors[n,scheme]=error
    self.assertGreaterEqual(min(result['fields']['smoke_kg']),0)
    self.assertLessEqual(max(result['fields']['smoke_kg']),max(r['initial_smoke_kg'])*(1+1e-12))
    self.assertAlmostEqual(result['budgets']['smoke_kg']['stored']+result['budgets']['smoke_kg']['outflow'],math.fsum(r['initial_smoke_kg']),places=12)
  self.assertLess(errors[16,'muscl_minmod'],errors[16,'upwind'])
  self.assertLess(errors[32,'muscl_minmod'],.8*errors[16,'muscl_minmod'])
  print('translation L1 mass errors',errors)
 def test_restart_and_time_refinement(self):
  r,whole,error=self.pulse(32,'muscl_minmod');first=run(dict(r,steps=r['steps'][:20]),WORKER)
  resumed=run(dict(r,state=first['state'],steps=r['steps'][20:]),WORKER);self.assertEqual(whole['state'],resumed['state'])
  refined=self.pulse(32,'muscl_minmod',.0025)[1]
  difference=math.fsum(abs(a-b) for a,b in zip(whole['fields']['smoke_kg'],refined['fields']['smoke_kg']))/math.fsum(whole['fields']['smoke_kg'])
  self.assertLess(difference,.08);print('time-refinement relative L1',difference)
 def test_streamed_digest_existing_protocol_exact(self):
  from numeric_digest_stream import digest as stream
  from surface_sources.growth_fire_v1 import digest as reference
  import random
  rng=random.Random(91)
  values={'z':'unicode \u03a9 and escape \"','a':[0,0.0,-0.0,True,False,None,2**53,{'digest':'nested remains'}], 'numbers':[rng.uniform(-1e20,1e20) for _ in range(4096)],'digest':'excluded'}
  self.assertEqual(stream(values),reference(values))
  for invalid in (float('nan'),float('inf'),2**53+1):
   with self.assertRaises(ValueError):stream({'value':invalid})
 def test_pressure_cache_nonuniform_buoyancy_and_restart(self):
  r=request();n=512;r['buoyancy']['enabled']=True;r['transport_scheme']='muscl_minmod'
  r['initial_energy_j']=[1. if q%8<3 and (q//8)%8<3 and q<64 else 0. for q in range(n)]
  r['steps']=r['steps'][:6];uncached=run(r,WORKER)
  self.assertGreater(max(map(abs,uncached['fields']['pressure_pa'])),1e-6)
  r['resource_limits']={'max_cells':32768,'scalar_work_cells':100000000,'cache_pressure_operator':True}
  cached=run(r,WORKER);self.assertEqual(cached['state']['data'],uncached['state']['data'])
  from numeric_digest_stream import digest
  self.assertEqual(digest({'data':cached['state']['data']}),digest({'data':uncached['state']['data']}))
  first=run(dict(r,steps=r['steps'][:3]),WORKER);resumed=run(dict(r,state=first['state'],steps=r['steps'][3:]),WORKER)
  self.assertEqual(cached['state'],resumed['state'])
 def test_strict_native_output_loader_bounds(self):
  from numeric_digest_stream import strict_load
  import tempfile
  with tempfile.TemporaryDirectory() as tmp:
   p=Path(tmp)/'data.json';p.write_text('{"a":1}')
   with self.assertRaises(ValueError):strict_load(p,1)
   self.assertEqual(strict_load(p,100),{'a':1})
   for text in ('{"a":1,"a":2}','{"a":NaN}'):
    p.write_text(text)
    with self.assertRaises(ValueError):strict_load(p,100)
 def test_resource_default_and_invalid_opt_in(self):
  r=request();r['grid']=[64]*3
  with self.assertRaises(ValueError):validate(r)
  r=request();r['resource_limits']={'max_cells':262145,'scalar_work_cells':400000000}
  with self.assertRaises(ValueError):validate(r)
  r=request();r['resource_limits']={'max_cells':32768,'scalar_work_cells':100000000,'native_output_bytes':257*1024*1024}
  with self.assertRaises(ValueError):validate(r)
  r=request();r['transport_scheme']='unknown'
  with self.assertRaises(ValueError):validate(r)
if __name__=='__main__':unittest.main()
