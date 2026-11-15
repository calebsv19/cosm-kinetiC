"""Opt-in wall identity and real coupled restart/rollback admission."""
import copy, hashlib, json, math, os
from pathlib import Path
import subprocess, sys, tempfile, unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from coupling import viscous_wall as wall
from coupling.framework import Framework
from coupling.open_weather_cases import configuration
from coupling.open_boundaries import layout
from coupling.solid_box import topology
from coupling.fluid_adapter import validate as fluid_validate
from numeric_digest_stream import digest, sealed
import open_atmosphere


def recipe(workers, layers=4):
    c=configuration(workers,'continuous',32)
    c['windows']=2;c.pop('source_delivery')
    f=c['fluid'];f.pop('inert_object');f['grid']=[32,32,8*layers]
    f['length_m']=[.5]*3
    f['solid_box']['lower_m']=[.125,.125,.0625]
    f['solid_box']['upper_m']=[.375,.375,.3125]
    f['initial_face_velocity_m_s']=[0.]*layout(f)[0]
    for key in ('initial_energy_j','initial_smoke_kg'):f[key]=[0.]*math.prod(f['grid'])
    f['resource_limits']['max_cells']=math.prod(f['grid'])
    f['open_domain_forcing']['ramp_height_m']=.25
    f['open_domain_forcing']['response_s']=.1
    f['open_domain_forcing']['knots']=[{'step':0,'target_m_s':.25,'strength':1}]
    f['scalar_transport']='mc_ssprk2/v1';f[wall.FIELD]=wall.PROFILE
    s=c['source'];s['origin_m']=[.03125,.1875,0];s['cell_size_m']=.0625/16
    s['evolution']['ignition_rectangle_m']=[.015625,.015625,.046875,.046875]
    r=c['receiver'];r['spacing_m']=[l/n for l,n in zip(f['length_m'],f['grid'])]
    r['dimensions']=f['grid'][:];mask=topology(f);plane=32**2
    r['fluid_mask']=[1-v for v in mask[:plane]]
    r['injection_fluid_mask']=[1-v for v in mask[:plane*round(r['injection_depth_m']/r['spacing_m'][2])]]
    return c


def workers():
    return tuple(os.environ[k] for k in ('FIRE_FLUID_GROWTH_ROOT','FIRE_FLUID_FIRE_WORKER','FIRE_FLUID_FLUID_WORKER','FIRE_FLUID_CLOCK_WORKER'))


class ProtocolTests(unittest.TestCase):
    def test_closed_identity_and_absence(self):
        wall.validate({});wall.state({},{});wall.fields({}, {}, {})
        for value in (None,True,{},'quadratic','low_box_quadratic/v2'):
            with self.assertRaises(ValueError):wall.validate({wall.FIELD:value})
        with self.assertRaises(ValueError):wall.state({}, {wall.FIELD:wall.PROFILE})

    @unittest.skipUnless(os.environ.get('FIRE_FLUID_FLUID_WORKER'),'native fixture paths required')
    def test_geometry_refusal_precedes_workers_and_store(self):
        for layers in (4,8):fluid_validate(recipe(workers(),layers)['fluid'])
        c=recipe(workers());original=copy.deepcopy(c)
        for field,value in (('grid',[32,32,16]),('length_m',[1,1,1]),('inert_object',{}),(wall.FIELD,'unknown')):
            bad=copy.deepcopy(c);bad['fluid'][field]=value
            with tempfile.TemporaryDirectory() as tmp:
                store=Path(tmp)/'store'
                with patch('subprocess.run',side_effect=AssertionError('worker invoked')):
                    with self.assertRaises(ValueError):Framework(bad,store)
                self.assertFalse(store.exists())
        self.assertEqual(c,original)


@unittest.skipUnless(os.environ.get('WALL_ADMISSION_NATIVE')=='1','explicit native admission proof')
class NativeTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.root=Path(self.temp.name)
        self.addCleanup(self.temp.cleanup);self.c=recipe(workers())

    def test_native_profile_restart_and_refusals(self):
        f=self.c['fluid'];zero=[0.]*math.prod(f['grid']);steps=[dict(energy_j=zero,smoke_kg=zero)]*2
        r=dict(f,schema=open_atmosphere.SCHEMA,state=None,steps=steps)
        whole=open_atmosphere.run(r,self.c['fluid_worker'],io_format='field_packet/v1')
        first=open_atmosphere.run(dict(r,steps=steps[:1]),self.c['fluid_worker'],io_format='field_packet/v1')
        final=open_atmosphere.run(dict(r,state=first['state'],steps=steps[1:]),self.c['fluid_worker'],io_format='field_packet/v1')
        self.assertEqual(whole['state'],final['state'])
        self.assertEqual(final['state']['data'][wall.FIELD],wall.PROFILE)
        wall.fields(f,final['fields'],final['state']['data'])
        # Missing echo models an older worker silently ignoring the option.
        with self.assertRaises(ValueError):wall.fields(f,{},final['state']['data'])
        for value in (None,'low_box_quadratic/v2'):
            bad=copy.deepcopy(first['state'])
            if value is None:bad['data'].pop(wall.FIELD)
            else:bad['data'][wall.FIELD]=value
            with self.assertRaises(ValueError):open_atmosphere.validate(dict(r,state=sealed(bad)))
        legacy=copy.deepcopy(r);legacy.pop(wall.FIELD)
        with self.assertRaises(ValueError):open_atmosphere.validate(dict(legacy,state=first['state']))
        unbound=copy.deepcopy(first['state']);unbound['config_digest']=digest(open_atmosphere.configuration(legacy))
        with self.assertRaises(ValueError):open_atmosphere.validate(dict(legacy,state=sealed(unbound)))
        # Native restore independently refuses removed or unexpected mode identity.
        from field_packet import encode
        for selected in (True,False):
            bad=copy.deepcopy(r if selected else legacy);bad['steps']=[]
            bad['state']=copy.deepcopy(first['state']['data'])
            if selected:bad['state'].pop(wall.FIELD)
            path=self.root/('bad-'+str(selected)+'.fpk');path.write_bytes(encode(bad,64*1024**2))
            process=subprocess.run([self.c['fluid_worker'],'--field-packet',str(path)],capture_output=True,timeout=120)
            self.assertNotEqual(process.returncode,0)

    def test_coupled_fresh_process_restart_and_atomic_rollback(self):
        a=Framework(self.c,self.root/'continuous');b=Framework(self.c,self.root/'segmented')
        self.assertEqual(a.initialize(),b.initialize())
        a.step(0,'first');b.step(0,'first')
        before=(self.root/'segmented/joint.sqlite3').read_bytes()
        def fail(cp):raise RuntimeError('injected before publication')
        with self.assertRaisesRegex(RuntimeError,'injected'):b.step(1,'second',before_publish=fail)
        self.assertEqual((self.root/'segmented/joint.sqlite3').read_bytes(),before)
        expected=a.step(1,'second');config=self.root/'config.json';config.write_text(json.dumps(self.c))
        code='import json,sys; from pathlib import Path; sys.path.insert(0,sys.argv[1]); from coupling.framework import Framework; h=Framework(json.loads(Path(sys.argv[2]).read_text()),sys.argv[3]); print(json.dumps(h.step(1,"second")))'
        child=subprocess.run([sys.executable,'-B','-c',code,str(ROOT/'scripts'),str(config),str(self.root/'segmented')],check=True,capture_output=True,text=True,timeout=240)
        self.assertEqual(json.loads(child.stdout),expected)
        resumed=Framework(self.c,self.root/'segmented');self.assertEqual(resumed.inspect(),a.inspect())
        self.assertEqual(resumed.step(1,'second'),expected)
        self.assertEqual((self.root/'segmented/joint.sqlite3').read_bytes(),(self.root/'continuous/joint.sqlite3').read_bytes())
        # A changed option cannot reopen an existing joint binding.
        changed=copy.deepcopy(self.c);changed['fluid'].pop(wall.FIELD)
        with self.assertRaises(ValueError):Framework(changed,self.root/'segmented').inspect()

if __name__=='__main__':unittest.main()
