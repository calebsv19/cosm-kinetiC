"""Actual detached-runner controls hold escaping IDs and linked metadata."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
class RunnerPaths(unittest.TestCase):
    def setUp(self):
        selected=os.environ.get('PHYSICS_SIM_JOB_RUNNER_BIN')
        if not selected:raise RuntimeError('PHYSICS_SIM_JOB_RUNNER_BIN required')
        self.binary=Path(selected);self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.base=Path(self.temp.name).resolve();self.jobs=self.base/'jobs';self.jobs.mkdir()
    def invoke(self,mode='status',id='job-1',root=None):
        return subprocess.run([str(self.binary),mode,'--job-id',id,'--jobs-root',str(root or self.jobs)],capture_output=True,text=True,timeout=5)
    def status(self):
        job=self.jobs/'job-1';job.mkdir(exist_ok=True)
        row={'job_id':'job-1','state':'completed','stage':'completed','progress_path':str(job/'run_progress.json'),
             'summary_path':str(job/'result_summary.json'),'stdout_path':str(job/'stdout.log'),'stderr_path':str(job/'stderr.log')}
        path=job/'job_status.json';path.write_text(json.dumps(row));return job,path,row
    def test_invalid_ids_do_not_escape_or_allocate(self):
        victim=self.base/'victim';victim.mkdir();sentinel=victim/'job_status.json';sentinel.write_bytes(b'held')
        for id in ('../victim','../../victim','/absolute','a/b','.','..','.hidden','a'*96,'job space','job\nline','é'):
            for mode in ('status','cancel'):
                result=self.invoke(mode,id);self.assertNotEqual(result.returncode,0);self.assertIn('job path admission held',result.stderr)
        self.assertEqual(sentinel.read_bytes(),b'held');self.assertEqual(list(self.jobs.iterdir()),[])
    def test_valid_local_id_shapes_reach_missing_status_gate(self):
        for id in ('a','Job_1.test-2','9','a'*95):
            result=self.invoke(id=id);self.assertNotEqual(result.returncode,0);self.assertIn('job status file not found',result.stderr)
        self.assertEqual(list(self.jobs.iterdir()),[])
    def test_linked_roots_and_job_slots_are_held(self):
        foreign=self.base/'foreign';foreign.mkdir();link=self.base/'linked';link.symlink_to(foreign)
        self.assertNotEqual(self.invoke(root=link).returncode,0)
        (self.jobs/'job-1').symlink_to(foreign)
        for mode in ('status','cancel'):self.assertNotEqual(self.invoke(mode).returncode,0)
        self.assertEqual(list(foreign.iterdir()),[])
    def test_every_fixed_metadata_slot_holds_links_without_foreign_mutation(self):
        names=('job_request.json','job_status.json','job.json','output/report.json','run_progress.json','stdout.log','stderr.log','pid.txt','cancel_requested.flag','result_summary.json')
        foreign=self.base/'foreign';foreign.write_bytes(b'held');job=self.jobs/'job-1';job.mkdir()
        for name in names:
            slot=job/name;slot.parent.mkdir(exist_ok=True);slot.symlink_to(foreign)
            for mode in ('status','cancel'):
                result=self.invoke(mode);self.assertNotEqual(result.returncode,0);self.assertIn('job path admission held',result.stderr)
            slot.unlink();self.assertEqual(foreign.read_bytes(),b'held')
    def test_special_and_hardlinked_slots_are_held(self):
        job=self.jobs/'job-1';job.mkdir();slot=job/'job_status.json';os.mkfifo(slot)
        for mode in ('status','cancel'):
            result=self.invoke(mode);self.assertNotEqual(result.returncode,0);self.assertIn('job path admission held',result.stderr)
        slot.unlink();foreign=self.base/'foreign';foreign.write_bytes(b'held');os.link(foreign,slot)
        self.assertNotEqual(self.invoke().returncode,0);self.assertEqual(foreign.read_bytes(),b'held')
    def test_protected_checkout_and_oversized_roots_hold_without_allocation(self):
        repo=self.base/'repo';repo.mkdir();(repo/'.git').write_text('fixture')
        for root in (repo,repo/'src/jobs',Path('/usr/native-job-test'),self.base/('/'.join(['a'*60]*18))):
            self.assertNotEqual(self.invoke(root=root).returncode,0)
        self.assertEqual(sorted(p.name for p in repo.iterdir()),['.git'])
    def test_redirected_status_references_are_held_before_read_or_cancel(self):
        job,path,row=self.status();fifo=self.base/'external';os.mkfifo(fifo)
        for field,value in (('job_id','other'),('progress_path',str(fifo)),('summary_path',str(fifo)),('stdout_path',str(fifo)),('stderr_path',str(fifo))):
            mutated=dict(row);mutated[field]=value;path.write_text(json.dumps(mutated));before=path.read_bytes()
            for mode in ('status','cancel'):
                result=self.invoke(mode);self.assertNotEqual(result.returncode,0)
            self.assertEqual(path.read_bytes(),before);self.assertFalse((job/'cancel_requested.flag').exists())
    def test_duplicate_status_fields_hold_without_cancel_or_refresh(self):
        job,path,row=self.status();path.write_text(json.dumps(row)[:-1]+',"job_id":"job-1"}')
        before=path.read_bytes()
        for mode in ('status','cancel'):self.assertNotEqual(self.invoke(mode).returncode,0)
        self.assertEqual(path.read_bytes(),before);self.assertFalse((job/'cancel_requested.flag').exists())
    def test_oversized_status_is_held_without_read_allocation(self):
        job,path,row=self.status()
        with path.open('wb') as stream:stream.truncate(16*1024*1024+1)
        for mode in ('status','cancel'):self.assertNotEqual(self.invoke(mode).returncode,0)
        self.assertEqual(path.stat().st_size,16*1024*1024+1);self.assertFalse((job/'cancel_requested.flag').exists())
    def test_ambiguous_or_linked_request_cannot_allocate_a_job(self):
        request=self.base/'request.json';request.write_text('{"runtime_scene_path":"fixture.json","output_root":"output","frames":1,"frames":2}')
        link=self.base/'linked-request.json';link.symlink_to(request)
        for selected in (request,link):
            result=subprocess.run([str(self.binary),'submit','--request',str(selected),'--jobs-root',str(self.jobs)],capture_output=True,text=True,timeout=5)
            self.assertNotEqual(result.returncode,0)
        self.assertEqual(list(self.jobs.iterdir()),[])
if __name__=='__main__':unittest.main()
