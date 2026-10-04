"""C3D-7 shared-session boundary and cooperative cancellation verification."""
import concurrent.futures
import hashlib
import json
import base64
import select
import subprocess
from pathlib import Path
import sys
import tempfile
import time
import unittest
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts/agent_session'))
from service import Service, SessionError, TERMINAL
from protocol import call
from test_agent_refined import wait
from cartesian3d import assess_3d

TEMPLATES = ('cfd_wall_stokes_3d', 'cfd_wall_transport_3d', 'cfd_pressure_startup_3d', 'cfd_open_wall_transient_3d')
FLUID = {'density_kg_m3': 1, 'dynamic_viscosity_pa_s': .1}

class TransientTests(unittest.TestCase):
    def fields(self, service, run):
        artifact = next(a for a in service.run_result(run)['artifacts'] if a['path'].endswith('channel_fields.json'))
        data = Path(artifact['path']).read_bytes()
        self.assertEqual(hashlib.sha256(data).hexdigest(), artifact['sha256'])
        return json.loads(data)

    def test_four_modes_inspection_artifacts_and_assessment(self):
        with tempfile.TemporaryDirectory() as root:
            s = Service(root)
            self.assertIn('incompressible_cartesian3d_v1', s.capabilities()['models'])
            for template in TEMPLATES:
                scene = call(s, 'scene_create', dict(scene_id=template, template=template))
                options = dict(scene_id=template, scene_revision=scene['scene_revision'], grid=[8,8,8],
                               steps=3, dt=.025, fluid=FLUID, numerical_memory_limit_mib=32)
                self.assertTrue(s.scene_validate(**options)['valid'])
                s.run_start(template, **options)
                self.assertEqual(wait(s, template)['state'], 'paused')
                self.assertEqual(s.run_assess(template)['numerical_status'], 'not_established')
                try:
                    self.assertEqual(s.run_control(template, 'step', 'step', scene['scene_revision'], 5000)['status'], 'applied')
                    before = s.run_inspect(template, history=True)
                    self.assertEqual(before['tick'], 1)
                    self.assertEqual(before['health']['last_step_outcome'], 'accepted')
                    self.assertIn('qualification', before['history'][-1])
                    for plane in ('XY','XZ','YZ'):
                        result = s.run_sample(template, 'sample'+plane, plane=plane, resolution=8, field='pressure_pa',
                                              points=[[1,1,1],[1,0,1],[99,1,1]], wait_ms=5000)
                        self.assertEqual(result['tick'], 1)
                        preview = result['preview']
                        self.assertEqual(len(preview['samples']), 64)
                        self.assertIsNotNone(preview['probes'][0]['values'][9])
                        self.assertEqual(preview['probes'][1]['values'][0], 0)
                        self.assertFalse(preview['probes'][2]['inside'])
                        self.assertIsNone(preview['statistics']['pressure_proxy'])
                    after = s.run_inspect(template)
                    self.assertEqual(after['simulation_time'], before['simulation_time'])
                    self.assertEqual(after['health']['numerical_memory']['peak_bytes'], before['health']['numerical_memory']['peak_bytes'])
                    self.assertEqual(s.run_assess(template)['numerical_status'], 'passed')
                    self.assertEqual(s.run_assess(template)['harmonic_accuracy']['status'], 'not_applicable' if template=='cfd_pressure_startup_3d' else 'not_established')
                    s.run_control(template, 'go', 'continue', scene['scene_revision'], 5000)
                    row = wait(s, template, True)
                    self.assertEqual(row['state'], 'completed', row)
                    fields = self.fields(s, template)['cartesian_fields']
                    self.assertEqual(len(fields['velocity_faces_m_s']), 512)
                    self.assertEqual(fields['periodic_axes'][0], template not in TEMPLATES[2:])
                    if template in TEMPLATES[2:]: self.assertEqual(len(fields['outlet_x_velocity_m_s']), 64)
                    self.assertGreater(row['runtime_cost']['final_export_wall_ms'], 0)
                finally:
                    if s.run_inspect(template)['state'] not in TERMINAL:
                        s.run_control(template, 'cleanup', 'cancel', scene['scene_revision'], 5000)
                        wait(s, template, True)

    def test_inflight_sampling_and_ordered_cancel_preserve_accepted_state(self):
        with tempfile.TemporaryDirectory() as root:
            s = Service(root)
            scene = s.scene_create('open', 'cfd_open_wall_transient_3d')
            opts = dict(scene_id='open', scene_revision=scene['scene_revision'], grid=[64,32,32],
                        steps=100, dt=.005, fluid=FLUID, numerical_memory_limit_mib=192)
            s.run_start('open', **opts); self.assertEqual(wait(s, 'open')['state'], 'paused')
            try:
                # Reference export at exactly one accepted step, identical configuration.
                s.run_control('open', 'first', 'step', scene['scene_revision'], 5000)
                self.assertEqual(s.run_inspect('open')['tick'], 1)
                sample = s.run_sample('open', 'before', resolution=8, field='pressure_pa', wait_ms=5000)
                before = s.run_inspect('open')
                with concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:
                    future = pool.submit(s.run_control, 'open', 'candidate', 'step', scene['scene_revision'], 5000)
                    end = time.monotonic()+5
                    while time.monotonic()<end:
                        current = s.run_inspect('open')
                        if current.get('solve_in_progress'): break
                        time.sleep(.005)
                    self.assertTrue(current.get('solve_in_progress'), current)
                    sample_begin = time.monotonic()
                    live = s.run_sample('open', 'during', resolution=8, field='pressure_pa', wait_ms=5000)
                    sample_ms = 1000*(time.monotonic()-sample_begin)
                    self.assertEqual(live['tick'], 1)
                    self.assertEqual(live['preview']['samples'], sample['preview']['samples'])
                    started = time.monotonic()
                    receipt = s.run_control('open', 'cancel', 'cancel', scene['scene_revision'], 5000)
                    self.assertEqual(receipt['status'], 'applied', receipt)
                    immediate_fields = self.fields(s,'open')['cartesian_fields']
                    cancel_ms = 1000*(time.monotonic()-started)
                    self.assertLess(cancel_ms, 2000)
                    step_receipt = future.result(timeout=5)
                self.assertEqual(step_receipt['status'], 'cancelled', step_receipt)
                self.assertEqual(step_receipt['sequence']+1, receipt['sequence'])
                self.assertEqual(s.run_control('open','cancel','cancel',scene['scene_revision'],5000),receipt)
                final = wait(s,'open',True)
                self.assertEqual(final['state'],'cancelled', final)
                self.assertEqual(final['tick'], before['tick'])
                self.assertEqual(final['simulation_time'], before['simulation_time'])
                self.assertEqual(final['health']['last_step_outcome'],'cancelled_preserved_last_accepted')
                self.assertEqual(s.run_assess('open')['numerical_status'], 'passed')
                cancelled = self.fields(s,'open')['cartesian_fields']
                self.assertEqual(immediate_fields,cancelled)
                # A second run accepts precisely one step, providing an independent full-field comparison.
                s.run_start('reference', **dict(opts, steps=1, start_paused=False))
                self.assertEqual(wait(s,'reference',True)['state'],'completed')
                self.assertEqual(cancelled, self.fields(s,'reference')['cartesian_fields'])
                print(json.dumps({'c3d7_control_measurement':True,'grid':[64,32,32],
                                  'sample_wall_ms':sample_ms,'cancel_receipt_wall_ms':cancel_ms,
                                  'accepted_tick_preserved':final['tick'],'full_field_exact':True,
                                  'checkpoint_service':final['runtime_cost']}),flush=True)
            finally:
                if s.run_inspect('open')['state'] not in TERMINAL:
                    s.run_control('open','cleanup','cancel',scene['scene_revision'],5000)
                    wait(s,'open',True)

    def test_mcp_transient_modes_and_transport_reconnect(self):
        """Exercise the external JSON-RPC path, including model discovery and Pa images."""
        with tempfile.TemporaryDirectory() as root:
            process = None
            sequence = 0

            def connect():
                return subprocess.Popen(
                    [sys.executable, str(ROOT/'scripts/physics_sim_session.py'),
                     '--root', root, '--mcp'], stdin=subprocess.PIPE,
                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)

            def disconnect():
                process.stdin.close()
                process.wait(timeout=10)
                self.assertEqual(process.returncode, 0)
                process.stdout.close()
                process.stderr.close()

            def rpc(method, params=None):
                nonlocal sequence
                sequence += 1
                process.stdin.write(json.dumps({'jsonrpc':'2.0', 'id':sequence,
                                               'method':method, 'params':params or {}})+'\n')
                process.stdin.flush()
                self.assertTrue(select.select([process.stdout], [], [], 10)[0], method)
                response = json.loads(process.stdout.readline())
                self.assertEqual(response['id'], sequence)
                self.assertNotIn('error', response)
                return response['result']

            def tool(name, arguments):
                response = rpc('tools/call', {'name':name, 'arguments':arguments})
                self.assertFalse(response['isError'], response)
                return response

            try:
                process = connect()
                rpc('initialize', {'protocolVersion':'2025-11-25'})
                catalog = rpc('tools/list')['tools']
                discovered = next(t for t in catalog if t['name']=='scene_create')
                self.assertTrue(set(TEMPLATES).issubset(
                    discovered['inputSchema']['properties']['template']['enum']))
                for template in TEMPLATES:
                    scene = tool('scene_create', {'scene_id':template, 'template':template})['structuredContent']
                    options = dict(scene_id=template, scene_revision=scene['scene_revision'],
                                   grid=[8,8,8], dt=.025, fluid=FLUID, numerical_memory_limit_mib=32)
                    self.assertTrue(tool('scene_validate', options)['structuredContent']['valid'])
                    tool('run_start', dict(options, request_id=template, steps=2))
                    controls = dict(run_id=template, scene_revision=scene['scene_revision'], wait_ms=5000)
                    receipt = tool('run_control', dict(controls, command_id='one', action='step'))['structuredContent']
                    self.assertEqual(receipt['tick'], 1)
                    before = tool('run_inspect', dict(run_id=template, history=True))['structuredContent']
                    for plane in ('XY','XZ','YZ'):
                        response = tool('run_sample', dict(run_id=template, request_id=plane,
                            plane=plane, field='pressure_pa', resolution=8,
                            points=[[1,1,1]], wait_ms=5000))
                        sample = response['structuredContent']
                        self.assertEqual(sample['tick'], 1)
                        self.assertIsNotNone(sample['preview']['statistics']['pressure_pa'])
                        self.assertIsNotNone(sample['preview']['probes'][0]['values'][9])
                        self.assertNotIn('samples', sample['preview'])
                        image = next(c for c in response['content'] if c['type']=='image')
                        self.assertEqual(image['mimeType'], 'image/png')
                        self.assertTrue(base64.b64decode(image['data']).startswith(b'\x89PNG\r\n\x1a\n'))
                    # The MCP connection does not own the simulation or its accepted state.
                    disconnect()
                    process = connect()
                    rpc('initialize', {'protocolVersion':'2025-11-25'})
                    after = tool('run_inspect', dict(run_id=template))['structuredContent']
                    self.assertEqual((after['tick'], after['simulation_time']),
                                     (before['tick'], before['simulation_time']))
                    assessment = tool('run_assess', dict(run_id=template))['structuredContent']
                    self.assertEqual(assessment['numerical_status'], 'passed')
                    tool('run_control', dict(controls, command_id='go', action='continue'))
                    end = time.monotonic()+10
                    while time.monotonic()<end:
                        status = tool('run_inspect', dict(run_id=template))['structuredContent']
                        if status['state'] in TERMINAL:
                            break
                        time.sleep(.01)
                    self.assertEqual(status['state'], 'completed', status)
                    result = tool('run_result', dict(run_id=template))['structuredContent']
                    self.assertEqual(result['status']['tick'], 2)
                    artifact = next(a for a in result['artifacts'] if a['path'].endswith('channel_fields.json'))
                    data = Path(artifact['path']).read_bytes()
                    self.assertEqual(hashlib.sha256(data).hexdigest(), artifact['sha256'])
                    fields = json.loads(data)['cartesian_fields']
                    self.assertEqual(len(fields['pressure_pa']), 512)
                    self.assertEqual(fields['periodic_axes'], [template in TEMPLATES[:2],False,False])
                    if template in TEMPLATES[2:]:
                        self.assertEqual(len(fields['outlet_x_velocity_m_s']), 64)
            finally:
                if process and process.poll() is None:
                    disconnect()
                service = Service(root)
                for template in TEMPLATES:
                    if not (Path(root)/'runs'/template/'request.json').exists():
                        continue
                    if service.run_inspect(template)['state'] not in TERMINAL:
                        service.run_control(template, 'cleanup', 'cancel',
                                            service.run_inspect(template)['scene_revision'], 5000)
                        wait(service, template, True)

    @unittest.skipUnless(sys.platform=='darwin', 'macOS process-residency regression')
    def test_repeated_inspection_memory_stays_bounded(self):
        with tempfile.TemporaryDirectory() as root:
            service = Service(root)
            scene = service.scene_create('memory', 'cfd_wall_stokes_3d')
            service.run_start('memory', scene_id='memory', scene_revision=scene['scene_revision'],
                              grid=[8,8,8], steps=2, dt=.025, fluid=FLUID)
            wait(service, 'memory')
            process = service.children['memory']
            try:
                service.run_control('memory', 'one', 'step', scene['scene_revision'], 5000)
                before = service.run_inspect('memory')
                def rss():
                    return int(subprocess.check_output(
                        ['ps','-p',str(process.pid),'-o','rss='], text=True))*1024
                measurements = []
                for index in range(170):
                    sample = service.run_sample('memory', 'sample'+str(index), resolution=32,
                                                field='pressure_pa', wait_ms=5000)
                    self.assertEqual(sample['tick'], 1)
                    if index in (19,169):
                        measurements.append(rss())
                growth = max(0, measurements[1]-measurements[0])
                self.assertLess(growth, 16*1024*1024)
                after = service.run_inspect('memory')
                self.assertEqual(after['simulation_time'], before['simulation_time'])
                self.assertEqual(after['energy_budget'], before['energy_budget'])
                self.assertEqual(after['health']['numerical_memory'], before['health']['numerical_memory'])
                self.assertEqual(after['json_library_version'], before['json_library_version'])
                print(json.dumps({'c3d7_inspection_memory':True, 'samples_after_warmup':150,
                                  'resident_before_bytes':measurements[0],
                                  'resident_after_bytes':measurements[1], 'growth_bytes':growth,
                                  'growth_limit_bytes':16*1024*1024,
                                  'json_version':after['json_library_version']}), flush=True)
            finally:
                service.run_control('memory', 'stop', 'cancel', scene['scene_revision'], 5000)
                process.wait(timeout=10)

    def test_budget_and_immutable_admission(self):
        with tempfile.TemporaryDirectory() as root:
            s = Service(root)
            for template in TEMPLATES:
                scene = s.scene_create(template,template)
                with self.assertRaises(SessionError):
                    s.scene_create(template+'-bad',template,channel={'solve_mode':'steady_duct'})
                s.run_start(template,scene_id=template,scene_revision=scene['scene_revision'],grid=[32,32,32],steps=1,
                            dt=.005,fluid=FLUID,numerical_memory_limit_mib=1,start_paused=False)
                row=wait(s,template,True)
                self.assertEqual(row['state'],'failed')
                self.assertEqual(row['error'],'numerical_memory_budget_exceeded')
                self.assertEqual(row['health']['numerical_memory']['live_bytes'],0)
                self.assertEqual(s.run_assess(template)['numerical_status'],'failed')
            with self.assertRaises(SessionError):
                s.scene_create('bad-length','cfd_open_wall_transient_3d',dimensions=[3,2,2])
            scene=s.scene_create('transport','cfd_wall_transport_3d')
            with self.assertRaises(SessionError):
                s.request(scene_id='transport',scene_revision=scene['scene_revision'],grid=[64,64,64],steps=1,dt=.1,fluid=FLUID)
        fabricated={'state':'completed','solve_mode':'wall_stokes_transient',
                    'health':{'projection_status':'converged','linear_relative_residual':0,'max_abs_divergence_s_inv':0},
                    'qualification':{'transient_reference_gate':{'applicable':True,'passed':True,'reference_time_resolved':True}}}
        self.assertEqual(assess_3d(fabricated)['reference_accuracy']['status'],'not_established')

if __name__=='__main__': unittest.main()
