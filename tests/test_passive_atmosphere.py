import copy
import json
import math
from pathlib import Path
import sys
import os
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from passive_atmosphere import atmosphere_worker_path,worker_subprocess_descriptors
from passive_atmosphere import run,validate,SCHEMA
WORKER=atmosphere_worker_path('passive')

def request(n=8,dt=.01,steps=1,velocity=(0,0,0),diffusion=0):
    count=n**3;zero=[0.]*count
    return {'schema':SCHEMA,'grid':[n]*3,'length_m':[2]*3,
        'properties':{'density_kg_m3':2,'heat_capacity_j_kg_k':1000,'reference_temperature_k':300,
            'conductivity_w_m_k':diffusion*2000,'tracer_diffusivity_m2_s':diffusion},
        'initial_energy_j':zero.copy(),'initial_smoke_kg':zero.copy(),
        'steps':[{'dt_s':dt,'face_velocity_m_s':[u for u in velocity for _ in range(count)],
            'energy_j':zero.copy(),'smoke_kg':zero.copy()} for _ in range(steps)]}

class PassiveTests(unittest.TestCase):
    def record(self,name,values):
        if os.environ.get('PASSIVE_QUALIFICATION_DIR'):
            root=Path(os.environ['PASSIVE_QUALIFICATION_DIR']);root.mkdir(parents=True,exist_ok=True)
            (root/(name+'.json')).write_text(json.dumps({'errors':values,'ratios':[b/a for a,b in zip(values,values[1:])]})+'\n')
    def test_uniform_known_volume_temperature(self):
        r=request();n=8**3;r['steps'][0]['energy_j']=[6000/n]*n;r['steps'][0]['smoke_kg']=[.0008/n]*n
        result=run(r,WORKER);self.assertAlmostEqual(result['budgets']['energy_j']['stored'],6000)
        for t in result['fields']['temperature_k']:self.assertAlmostEqual(t,300+6000/(2*1000*8),places=12)
        self.assertAlmostEqual(result['budgets']['smoke_kg']['stored'],.0008,places=15)

    def test_translation_subcycle_known_answer_and_periodic_wrap(self):
        r=request(dt=.25,velocity=(1,0,0));r['initial_energy_j'][7]=2;r['initial_smoke_kg'][7]=.5
        result=run(r,WORKER);f=result['fields'];self.assertAlmostEqual(sum(f['energy_j']),2)
        # Subcycling at CFL .8: numerical diffusion allowed, correct mean displacement across wrap.
        self.assertAlmostEqual(f['energy_j'][7],.5);self.assertAlmostEqual(f['energy_j'][0],1);self.assertAlmostEqual(f['energy_j'][1],.5)
        self.assertTrue(all(v>=0 for v in f['smoke_kg']))

    def test_fourier_diffusion_and_spatial_refinement(self):
        errors=[]
        for n in (8,16,32):
            r=request(n,dt=.001,steps=20,diffusion=.1);h=2/n
            initial=[2+math.cos(math.pi*(i%n+.5)*h) for i in range(n**3)]
            r['initial_energy_j']=initial;r['initial_smoke_kg']=[v*.001 for v in initial]
            f=run(r,WORKER)['fields'];amp=math.exp(-.1*math.pi**2*.02)
            expected=[2+amp*math.cos(math.pi*(i%n+.5)*h) for i in range(n**3)]
            errors.append(math.sqrt(sum((a-b)**2 for a,b in zip(f['energy_j'],expected))/n**3))
            self.assertAlmostEqual(math.fsum(f['energy_j']),2*n**3,places=7)
            for a,b in zip(f['energy_j'],f['smoke_kg']):self.assertAlmostEqual(a*.001,b,places=14)
        self.assertLess(errors[1],errors[0]*.3);self.assertLess(errors[2],errors[1]*.3)
        self.record('diffusion_spatial_l2',errors)

    def test_translation_refinement(self):
        errors=[]
        for n in (8,16,32):
            r=request(n,dt=.01,steps=20,velocity=(.5,0,0));h=2/n
            r['initial_energy_j']=[2+math.sin(math.pi*(i%n+.5)*h) for i in range(n**3)]
            f=run(r,WORKER)['fields']['energy_j']
            expected=[2+math.sin(math.pi*((i%n+.5)*h-.1)) for i in range(n**3)]
            errors.append(math.sqrt(sum((a-b)**2 for a,b in zip(f,expected))/n**3))
        self.assertLess(errors[1],errors[0]*.6);self.assertLess(errors[2],errors[1]*.6)
        self.record('advection_spatial_l2',errors)

    def test_advection_temporal_refinement(self):
        errors=[];n=8;u=.5;h=2/n;t=.2
        amplitude=math.exp(u/h*(math.cos(math.pi*h)-1)*t)
        phase=u/h*math.sin(math.pi*h)*t
        for dt in (.02,.01,.005):
            r=request(n,dt=dt,steps=round(t/dt),velocity=(u,0,0))
            r['initial_energy_j']=[2+math.sin(math.pi*(i%n+.5)*h) for i in range(n**3)]
            f=run(r,WORKER)['fields']['energy_j']
            errors.append(math.sqrt(sum((f[i]-2-amplitude*math.sin(math.pi*(i%n+.5)*h-phase))**2 for i in range(n**3))/n**3))
        self.assertLess(errors[1],errors[0]*.55);self.assertLess(errors[2],errors[1]*.55)
        self.record('advection_temporal_l2_semidiscrete',errors)

    def test_temporal_refinement_diffusion(self):
        errors=[];n=8;lam=4*.1*math.sin(math.pi/n)**2/(2/n)**2
        for dt in (.02,.01,.005):
            r=request(n,dt=dt,steps=round(.1/dt),diffusion=.1)
            r['initial_energy_j']=[2+math.cos(math.pi*(i%n+.5)*2/n) for i in range(n**3)]
            f=run(r,WORKER)['fields']['energy_j'];amp=math.exp(-lam*.1)
            errors.append(math.sqrt(sum((f[i]-2-amp*math.cos(math.pi*(i%n+.5)*2/n))**2 for i in range(n**3))/n**3))
        self.assertLess(errors[1],errors[0]*.55);self.assertLess(errors[2],errors[1]*.55)
        self.record('diffusion_temporal_l2_semidiscrete',errors)

    def test_rate_duration_unequal_steps_and_zero(self):
        for boundaries in ([.005,.015,.03],[.01]*10,[.02]*5):
            r=request(steps=len(boundaries));rate=120000
            for s,dt in zip(r['steps'],boundaries):s['dt_s']=dt;s['energy_j'][0]=rate*dt;s['smoke_kg'][0]=.016*dt
            result=run(r,WORKER);self.assertAlmostEqual(result['budgets']['energy_j']['stored'],rate*sum(boundaries))
        r=request();self.assertEqual(run(r,WORKER)['budgets']['energy_j']['stored'],0)

    def test_half_rate_twice_duration_and_cli_publication(self):
        totals=[]
        for rate,duration in ((120000,.05),(60000,.05),(120000,.1)):
            r=request(dt=duration);r['steps'][0]['energy_j'][0]=rate*duration
            totals.append(run(r,WORKER)['budgets']['energy_j']['stored'])
        self.assertEqual(totals,[6000,3000,12000])
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);src=root/'request.json';out=root/'result.json';src.write_text(json.dumps(request()))
            cmd=[sys.executable,'-B',str(ROOT/'scripts/passive_atmosphere.py'),'--request',str(src),'--output',str(out)]
            accepted=subprocess.run(cmd,capture_output=True,text=True,pass_fds=worker_subprocess_descriptors());self.assertEqual(accepted.returncode,0,accepted.stderr)
            before=out.read_bytes();retry=subprocess.run(cmd,capture_output=True,text=True,pass_fds=worker_subprocess_descriptors())
            self.assertNotEqual(retry.returncode,0);self.assertEqual(before,out.read_bytes())
            src.write_text(src.read_text().replace('"dt_s": 0.01','"dt_s": true'))
            rejected=subprocess.run(cmd[:-1]+[str(root/'rejected.json')],capture_output=True,text=True,pass_fds=worker_subprocess_descriptors())
            self.assertNotEqual(rejected.returncode,0);self.assertFalse((root/'rejected.json').exists())

    def test_malformed_and_native_rejection(self):
        for key,value in [('grid',[True,8,8]),('length_m',[0,2,2]),('schema','wrong')]:
            r=request();r[key]=value
            with self.assertRaises(ValueError):validate(r)
        r=request();r['steps'][0]['face_velocity_m_s'][0]=1
        with self.assertRaises(ValueError):run(r,WORKER)
        r=request();r['steps'][0]['energy_j'][0]=-1
        with self.assertRaises(ValueError):run(r,WORKER)
        r=request();r['properties']['tracer_diffusivity_m2_s']=1e9
        with self.assertRaises(ValueError):run(r,WORKER)

if __name__=='__main__':unittest.main()
