import concurrent.futures
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts/agent_session'))
from service import Service, SessionError


class SessionIntegration(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='physics-session-')
        self.service = Service(self.tmp.name)
        self.scene = self.service.scene_create('wind')
        self.revision = self.scene['scene_revision']

    def tearDown(self):
        for path in (Path(self.tmp.name)/'runs').iterdir():
            if self.service.owner_alive(path):
                status=self.service.run_inspect(path.name)
                if status['state'] not in {'completed','failed','cancelled'}:
                    revision=json.loads((path/'request.json').read_text())['scene_revision']
                    self.service.run_control(path.name, 'cleanup', 'cancel', revision, 5000)
                deadline=time.monotonic()+10
                while self.service.owner_alive(path) and time.monotonic()<deadline:
                    time.sleep(.01)
                self.assertFalse(self.service.owner_alive(path), 'worker must release ownership')
        for child in self.service.children.values():
            child.wait(timeout=10)
        self.tmp.cleanup()

    def start(self, **kw):
        self.service.run_start('run', 'wind', self.revision, **kw)
        return self.wait(lambda s:s['state']!='starting')

    def wait(self, predicate, limit=10):
        deadline=time.monotonic()+limit
        while time.monotonic()<deadline:
            status=self.service.run_inspect('run')
            if predicate(status):return status
            time.sleep(.01)
        self.fail(str(status))

    def control(self, action, cid=None):
        return self.service.run_control('run',cid or action,action,self.revision,5000)

    def test_lifecycle_pause_single_step_retry_and_bounded_observation(self):
        valid=self.service.scene_validate('wind',self.revision)
        self.assertEqual(valid['effective_grid'],[32,16,16])
        initial=self.start(steps=500)
        self.assertEqual(initial['state'],'paused')
        first=self.control('step')
        self.assertEqual(first['tick'],1)
        self.assertEqual(first['status'],'applied')
        self.assertEqual(self.control('step'),first)
        time.sleep(.08)
        snap=self.service.run_inspect('run',True)
        self.assertEqual(snap['tick'],1)
        self.assertAlmostEqual(snap['simulation_time'],snap['dt'])
        self.assertLessEqual(len(snap['preview']['samples']),4096)
        self.assertGreater(snap['preview']['speed_max'],0)
        self.assertEqual(snap['health']['export_materializations'],0)
        self.control('continue')
        self.wait(lambda s:s['tick']>3)
        paused=self.control('pause')
        time.sleep(.05)
        self.assertEqual(self.service.run_inspect('run')['tick'],paused['tick'])
        self.assertEqual(self.control('step','step2')['tick'],paused['tick']+1)
        self.assertEqual(self.control('cancel')['state'],'cancelled')
        result=self.service.run_result('run')
        self.assertEqual(result['provenance']['scene_revision'],self.revision)
        self.assertEqual(len(result['provenance']['worker_sha256']),64)
        self.assertTrue(all(a['sha256'] for a in result['artifacts']))

    def test_concurrent_start_and_commands_have_one_owner(self):
        def start(_):
            return self.service.run_start('run','wind',self.revision)
        with concurrent.futures.ThreadPoolExecutor(4) as pool:
            starts=list(pool.map(start,range(4)))
        self.wait(lambda s:s['state']=='paused')
        self.assertTrue(all(s['run_id']=='run' for s in starts))
        with self.assertRaises(SessionError):
            self.service.run_start('run','wind',self.revision,steps=20)
        with self.assertRaises(SessionError):
            self.service.run_start('second','wind',self.revision)
        def command(i):
            return Service(self.tmp.name).run_control('run',f'step{i}','step',self.revision,5000)
        with concurrent.futures.ThreadPoolExecutor(4) as pool:
            receipts=list(pool.map(command,range(4)))
        self.assertEqual(sorted(r['tick'] for r in receipts),[1,2,3,4])
        self.control('cancel')

    def test_stale_scene_and_invalid_commands(self):
        with self.assertRaises(SessionError):
            self.service.run_start('run','wind','0'*64)
        self.start()
        self.control('step')
        with self.assertRaises(SessionError):
            self.service.run_control('run','step','continue',self.revision)
        with self.assertRaises(SessionError):
            self.service.run_control('run','bad','pause','0'*64)
        self.control('continue')
        self.assertEqual(self.control('step','running-step')['status'],'rejected')
        self.control('cancel')
        with self.assertRaises(SessionError):self.control('continue','after-stop')

    def test_completion_events_and_snapshot_immutability(self):
        self.start(steps=3)
        source=Path(self.scene['project_path'])/'scene_runtime.json'
        original=json.loads(source.read_text())
        source.write_text('{}')
        self.control('continue')
        final=self.wait(lambda s:s['state'] in {'completed','failed'})
        self.assertEqual(final['state'],'completed')
        self.assertEqual(final['tick'],3)
        artifacts=self.service.run_result('run')['artifacts']
        self.assertTrue(any(a['path'].endswith('.vf3d') for a in artifacts))
        self.assertEqual(json.loads((self.service.run_dir('run')/'scene_runtime.json').read_text()),original)
        events=self.service.run_events('run')
        self.assertTrue(events['events'])
        self.assertEqual(self.service.run_events('run',events['next_cursor'])['events'],[])

    def test_budget_failure_is_not_success(self):
        self.start(solver_cell_budget=1)
        receipt=self.control('step')
        self.assertEqual(receipt['status'],'failed')
        status=self.service.run_inspect('run')
        self.assertEqual(status['state'],'failed')
        self.assertGreater(status['health']['skipped_clusters'],0)
        self.assertEqual(status['tick'],0)

    def test_worker_death_is_reconciled_and_new_run_allowed(self):
        self.start()
        worker=self.service.children['run']
        os.kill(worker.pid,signal.SIGKILL)
        worker.wait(timeout=5)
        self.assertEqual(self.service.run_inspect('run')['error'],'worker_exited_without_terminal_result')
        result=self.service.run_start('next','wind',self.revision,steps=1,start_paused=False)
        self.assertEqual(result['run_id'],'next')

    def test_protocol_end_to_end(self):
        p=subprocess.Popen([sys.executable,str(ROOT/'scripts/physics_sim_session.py'),'--root',self.tmp.name,'--mcp'],
                           stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        seq=0
        def rpc(method,params=None):
            nonlocal seq
            seq+=1
            p.stdin.write(json.dumps({'jsonrpc':'2.0','id':seq,'method':method,'params':params or {}})+'\n');p.stdin.flush()
            msg=json.loads(p.stdout.readline());self.assertEqual(msg['id'],seq);return msg['result']
        def tool(name,args):
            r=rpc('tools/call',{'name':name,'arguments':args})
            self.assertFalse(r['isError'],r)
            if name=='run_inspect' and args.get('preview'):
                self.assertEqual(r['content'][1]['mimeType'],'image/png')
                self.assertNotIn('samples',r['structuredContent']['preview'])
                self.assertLess(len(json.dumps(r)),25000)
            return r['structuredContent']
        try:
            self.assertIn('tools',rpc('initialize',{'protocolVersion':'2025-11-25'})['capabilities'])
            self.assertEqual(len(rpc('tools/list')['tools']),8)
            created=tool('scene_create',{'scene_id':'sphere','template':'wind_sphere'})
            revision=created['scene_revision']
            tool('scene_validate',{'scene_id':'sphere','scene_revision':revision})
            tool('run_start',{'request_id':'mcp','scene_id':'sphere','scene_revision':revision,'steps':2})
            args={'run_id':'mcp','scene_revision':revision,'wait_ms':5000}
            receipt=tool('run_control',dict(args,command_id='one',action='step'))
            self.assertEqual(receipt['tick'],1)
            self.assertIn('preview',tool('run_inspect',{'run_id':'mcp','preview':True}))
            # Transport disconnect must not own or stop the solver session.
            p.stdin.close();p.wait(timeout=10);p.stdout.close();p.stderr.close()
            p=subprocess.Popen([sys.executable,str(ROOT/'scripts/physics_sim_session.py'),'--root',self.tmp.name,'--mcp'],
                               stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            rpc('initialize',{'protocolVersion':'2025-11-25'})
            self.assertEqual(tool('run_inspect',{'run_id':'mcp','log_tail_lines':10})['tick'],1)
            tool('run_control',dict(args,command_id='go',action='continue'))
            for _ in range(100):
                if tool('run_inspect',{'run_id':'mcp'})['state']=='completed':break
                time.sleep(.01)
            self.assertEqual(tool('run_result',{'run_id':'mcp'})['status']['tick'],2)
        finally:
            p.stdin.close();p.wait(timeout=10)
            p.stdout.close();p.stderr.close()
