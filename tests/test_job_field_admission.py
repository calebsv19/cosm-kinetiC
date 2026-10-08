"""Known native metadata fields refuse silent numeric and string conversion."""
import json
import fcntl
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class FieldAdmission(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.t=tempfile.TemporaryDirectory();base=Path(cls.t.name);cls.binary=base/'probe';source=base/'probe.c'
        source.write_text(r"""#include "app/physics_sim_job_json.h"
#include <stdio.h>
int main(int argc,char **argv){if(argc!=2)return 3;json_object *root=physics_sim_job_json_read(argv[1]);if(!root)return 4;int number=123;
PhysicsSimJobJsonField fields[]={{"integer",PHYSICS_JOB_INT,0,2147483647},{"flag",PHYSICS_JOB_BOOL,0,0},{"text",PHYSICS_JOB_STRING,0,8}};
int converted=physics_sim_job_json_integer(root,"integer",&number);int admitted=physics_sim_job_json_fields(root,fields,3);printf("%d %d %d\n",converted,admitted,number);json_object_put(root);return 0;}
""")
        flags=shlex.split(subprocess.check_output([os.environ.get('PKG_CONFIG','pkg-config'),'--cflags','--libs','json-c'],text=True))
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),str(source),str(ROOT/'src/app/physics_sim_job_json.c'),*flags,'-o',str(cls.binary)],capture_output=True,check=True)
    @classmethod
    def tearDownClass(cls):cls.t.cleanup()
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.base=Path(self.t.name).resolve();self.jobs=self.base/'jobs';self.jobs.mkdir()
    def runner(self,*args):
        binary=os.environ.get('PHYSICS_SIM_JOB_RUNNER_BIN');self.assertTrue(binary,'Actual runner required')
        return subprocess.run([binary,*args,'--jobs-root',str(self.jobs)],capture_output=True,text=True,timeout=5)
    def probe(self,row):
        path=self.base/'fields.json';path.write_text(json.dumps(row));result=subprocess.run([str(self.binary),str(path)],capture_output=True,text=True,timeout=3)
        self.assertEqual(result.returncode,0,result.stderr);return result.stdout.split()[:2]
    def status(self):
        job=self.jobs/'job-1';job.mkdir(exist_ok=True);row={'job_id':'job-1','state':'completed','stage':'completed','progress_path':str(job/'run_progress.json'),'summary_path':str(job/'result_summary.json'),'stdout_path':str(job/'stdout.log'),'stderr_path':str(job/'stderr.log')}
        output=self.base/'output';output.mkdir(exist_ok=True);st=output.stat();(output/'.physics-sim-headless-owner').write_text(f'physics_sim_headless_output_v1 completed {st.st_dev} {st.st_ino}\n');row['output_root']=str(output)
        path=job/'job_status.json';path.write_text(json.dumps(row));return job,path,row
    def paired_status(self):
        job,path,base=self.status()
        base.update(request_path=str(job/'job_request.json'),frames_requested=1,
                    frames_completed=1,frame_index=0,sim_steps_per_frame=1,
                    submitted_at_utc='2026-10-07T00:00:00Z',updated_at_utc='2026-10-07T00:00:00Z')
        path.write_text(json.dumps(base));report=job/'output/report.json'
        if report.exists():report.unlink()
        (job/'result_summary.json').write_text(json.dumps({'status':'passed','frames_requested':1,'frames_completed':1,'sim_steps_per_frame':1,'result_code':0}))
        result=self.runner('status','--job-id','job-1');self.assertEqual(result.returncode,0,result.stderr)
        return job,path,json.loads(path.read_text()),report,json.loads(report.read_text())
    def request(self):
        return {'runtime_scene_path':str(ROOT/'tests/fixtures/runtime_scene_primitive_retained.json'),'output_root':str(self.base/'output'),'frames':1,'sim_steps_per_frame':1,'progress_interval':1,'skip_present':True}
    def test_integer_getter_accepts_exact_bounds_and_rejects_coercion(self):
        for value in (0,2147483647):self.assertEqual(self.probe({'integer':value}),['1','1'])
        for value in (1.0,1.5,2147483648,2**64-1,-2147483649,None,True,'1'):
            self.assertEqual(self.probe({'integer':value}),['0','0'])
    def test_oversized_integer_lexeme_is_held_by_reader_before_conversion(self):
        path=self.base/'fields.json';path.write_text(json.dumps({'integer':10**70}))
        result=subprocess.run([str(self.binary),str(path)],capture_output=True,text=True,timeout=3)
        self.assertEqual(result.returncode,4);self.assertEqual(result.stdout,'')

    def test_optional_fields_remain_absent_but_present_boolean_and_string_are_strict(self):
        self.assertEqual(self.probe({}),['0','1']);self.assertEqual(self.probe({'flag':False,'text':'12345678'}),['0','1'])
        for row in ({'flag':1},{'flag':'true'},{'text':9},{'text':'123456789'},{'text':None}):self.assertEqual(self.probe(row),['0','0'])
    def test_request_numeric_fields_hold_before_job_allocation(self):
        for field in ('frames','sim_steps_per_frame','progress_interval','volume_export_start_frame','volume_export_stride','volume_export_max_frames'):
            for value in (1.0,2**40,None,True,-1):
                row=self.request();row[field]=value;path=self.base/'request.json';path.write_text(json.dumps(row));result=self.runner('submit','--request',str(path))
                self.assertNotEqual(result.returncode,0);self.assertIn('field type/range held',result.stderr);self.assertEqual(list(self.jobs.iterdir()),[])
        self.assertFalse((self.base/'output').exists())
    def test_request_boolean_and_bounded_string_fields_hold_before_allocation(self):
        for field,value in [('overwrite',1),('skip_present','true'),('save_volume_frames',None),('grid',9),('schema_version','x'*64)]:
            row=self.request();row[field]=value;path=self.base/'request.json';path.write_text(json.dumps(row));result=self.runner('submit','--request',str(path))
            self.assertNotEqual(result.returncode,0);self.assertIn('field type/range held',result.stderr);self.assertEqual(list(self.jobs.iterdir()),[])
    def test_status_integer_types_and_ranges_hold_without_refresh_or_cancel(self):
        job,path,base=self.status()
        for field,value in [('pid',1.5),('pid',2**40),('pid',-1),('exit_code',256),('frames_completed',-1),('frame_index',True),('sim_steps_per_frame',None)]:
            row=dict(base);row[field]=value;path.write_text(json.dumps(row));before=path.read_bytes()
            for mode in ('status','cancel'):
                result=self.runner(mode,'--job-id','job-1');self.assertNotEqual(result.returncode,0);self.assertEqual(path.read_bytes(),before)
            self.assertFalse((job/'cancel_requested.flag').exists())
    def test_status_string_truncation_cannot_make_identity_or_paths_match(self):
        job,path,base=self.status()
        for field,value in [('job_id','job-1'+'x'*256),('stage','x'*256),('diagnostics','x'*512),('progress_path',base['progress_path']+'x'*4096)]:
            row=dict(base);row[field]=value;path.write_text(json.dumps(row));before=path.read_bytes()
            result=self.runner('status','--job-id','job-1');self.assertNotEqual(result.returncode,0);self.assertEqual(path.read_bytes(),before)
    def test_invalid_progress_is_held_before_any_record_publication(self):
        job,path,base=self.status();before=path.read_bytes()
        for row in ({'frames_completed':1.0},{'frame_index':-1},{'sim_steps_total_in_frame':2**40},{'status':'unknown'},{'stage':'x'*256}):
            (job/'run_progress.json').write_text(json.dumps(row))
            for mode in ('status','cancel'):
                result=self.runner(mode,'--job-id','job-1');self.assertNotEqual(result.returncode,0);self.assertIn('job refresh held',result.stderr);self.assertEqual(path.read_bytes(),before)
            self.assertFalse((job/'cancel_requested.flag').exists())
    def test_starting_worker_empty_sidecar_reservations_are_not_corrupt_metadata(self):
        job,path,row=self.status();row.update(state='starting',stage='starting',pid=os.getpid());path.write_text(json.dumps(row));before=path.read_bytes()
        (job/'run_progress.json').write_bytes(b'');(job/'result_summary.json').write_bytes(b'')
        result=self.runner('status','--job-id','job-1');self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout)['state'],'starting');self.assertEqual(path.read_bytes(),before)

    def test_unknown_or_nonstring_summary_status_is_held(self):
        job,path,base=self.status();before=path.read_bytes()
        for value in ('unknown',9,None,'x'*256):
            (job/'result_summary.json').write_text(json.dumps({'status':value}))
            result=self.runner('status','--job-id','job-1');self.assertNotEqual(result.returncode,0);self.assertIn('job refresh held',result.stderr);self.assertEqual(path.read_bytes(),before)
    def test_unknown_persisted_state_holds_status_and_cancel(self):
        job,path,base=self.status()
        for state in ('', 'unknown', 'passed', 'canceled', 'RUNNING'):
            row=dict(base,state=state);path.write_text(json.dumps(row));before=path.read_bytes()
            for mode in ('status','cancel'):
                result=self.runner(mode,'--job-id','job-1');self.assertNotEqual(result.returncode,0);self.assertEqual(path.read_bytes(),before)
            self.assertFalse((job/'cancel_requested.flag').exists())
    def test_malformed_status_timestamps_hold_before_publication(self):
        job,path,base=self.status()
        invalid=('2026-02-29T01:02:03Z','2024-04-31T01:02:03Z','2026-00-01T00:00:00Z',
                 '2026-10-07T24:00:00Z','2026-10-07T00:60:00Z','2026-10-07T00:00:60Z',
                 '2026-1-7T00:00:00Z','2026-10-07T00:00:00Zjunk','2026-10-07T00:00:00+00:00',
                 '0000-10-07T00:00:00Z','2026-10-07T00:00:00X','9999999999-1-1T0:0:0Z')
        for field in ('submitted_at_utc','started_at_utc','updated_at_utc','finished_at_utc'):
            for timestamp in invalid:
                row=dict(base);row[field]=timestamp;path.write_text(json.dumps(row));before=path.read_bytes()
                for mode in ('status','cancel'):
                    result=self.runner(mode,'--job-id','job-1');self.assertNotEqual(result.returncode,0);self.assertEqual(path.read_bytes(),before)
                self.assertFalse((job/'cancel_requested.flag').exists())
    def test_exact_calendar_dates_and_empty_optional_timestamps_are_admitted(self):
        job,path,base=self.status()
        base.update(frames_requested=1,frames_completed=1,frame_index=0,sim_steps_per_frame=1,exit_code=0,diagnostics='simulation completed',finished_at_utc='2026-10-07T00:00:00Z')
        (job/'result_summary.json').write_text(json.dumps({'status':'passed','frames_requested':1,'frames_completed':1,'sim_steps_per_frame':1,'result_code':0}))
        for timestamp in ('','2024-02-29T23:59:59Z','2000-02-29T00:00:00Z','2026-10-07T00:00:00Z','1969-12-31T23:59:59Z'):
            row=dict(base,updated_at_utc=timestamp);path.write_text(json.dumps(row))
            result=self.runner('status','--job-id','job-1');self.assertEqual(result.returncode,0,result.stderr)
    def test_malformed_progress_timestamp_is_held_without_refresh(self):
        job,path,base=self.status();before=path.read_bytes()
        (job/'run_progress.json').write_text(json.dumps({'updated_at_utc':'2026-02-30T00:00:00Z'}))
        for mode in ('status','cancel'):
            result=self.runner(mode,'--job-id','job-1');self.assertNotEqual(result.returncode,0);self.assertIn('job refresh held',result.stderr);self.assertEqual(path.read_bytes(),before)
        self.assertFalse((job/'cancel_requested.flag').exists())
    def test_inconsistent_status_counters_are_held_before_cancel(self):
        job,path,base=self.status();base.update(state='running',pid=os.getpid(),frames_requested=2,frames_completed=0,frame_index=0,sim_steps_per_frame=3,sim_steps_total_in_frame=3,sim_steps_completed_in_frame=0)
        for changes in ({'frames_completed':3},{'frame_index':2},{'sim_steps_completed_in_frame':4},{'sim_steps_total_in_frame':4},{'state':'completed','frames_completed':1}):
            row=dict(base,**changes);path.write_text(json.dumps(row));before=path.read_bytes()
            for mode in ('status','cancel'):
                result=self.runner(mode,'--job-id','job-1');self.assertNotEqual(result.returncode,0);self.assertEqual(path.read_bytes(),before)
            self.assertFalse((job/'cancel_requested.flag').exists())
    def test_present_status_identity_tags_and_request_path_are_bound(self):
        job,path,base=self.status()
        for field,value in [('schema_version','unknown'),('schema_version',None),('program','other'),('tool','other'),('artifact_class','disposable_build'),('request_path',str(self.base/'foreign.json')),('overwrite_policy','unknown')]:
            row=dict(base);row[field]=value;path.write_text(json.dumps(row));before=path.read_bytes()
            for mode in ('status','cancel'):
                result=self.runner(mode,'--job-id','job-1');self.assertNotEqual(result.returncode,0);self.assertEqual(path.read_bytes(),before)
            self.assertFalse((job/'cancel_requested.flag').exists())
    def test_progress_cannot_change_requested_work_or_run_identity(self):
        job,path,base=self.status();base.update(state='running',pid=os.getpid(),frames_requested=2,frames_completed=0,frame_index=0,sim_steps_per_frame=3,sim_steps_total_in_frame=3,sim_steps_completed_in_frame=0,output_root=str(self.base/'output'))
        path.write_text(json.dumps(base));before=path.read_bytes()
        for row in ({'frames_requested':3},{'sim_steps_per_frame':4},{'output_root':str(self.base/'foreign')},{'schema':'other'},{'artifact_class':None},{'frames_completed':3},{'frame_index':2},{'sim_steps_completed_in_frame':4},{'sim_steps_total_in_frame':4},{'status':'passed','frames_completed':1}):
            (job/'run_progress.json').write_text(json.dumps(row))
            for mode in ('status','cancel'):
                result=self.runner(mode,'--job-id','job-1');self.assertNotEqual(result.returncode,0);self.assertIn('job refresh held',result.stderr);self.assertEqual(path.read_bytes(),before)
            self.assertFalse((job/'cancel_requested.flag').exists())
    def test_progress_frame_boundaries_and_terminal_zero_steps_are_admitted(self):
        job,path,base=self.status();base.update(state='running',pid=os.getpid(),frames_requested=2,frames_completed=0,frame_index=0,sim_steps_per_frame=3,sim_steps_total_in_frame=3,sim_steps_completed_in_frame=0)
        base.update(submitted_at_utc='2026-10-07T00:00:00Z',started_at_utc='2026-10-07T00:00:00Z',updated_at_utc='2026-10-07T00:00:00Z')
        for progress in ({'status':'running','frames_completed':1,'frame_index':1,'sim_steps_completed_in_frame':3,'sim_steps_total_in_frame':3},
                         {'status':'passed','frames_completed':2,'frame_index':1,'sim_steps_completed_in_frame':0,'sim_steps_total_in_frame':0},
                         {'status':'canceled','frames_completed':2,'frame_index':2,'sim_steps_completed_in_frame':0,'sim_steps_total_in_frame':0}):
            if progress['status'] in ('passed','canceled'):(job/'result_summary.json').write_text(json.dumps({'status':progress['status']}))
            # Each branch starts an independent legacy predecessor, not a stale pair.
            report=job/'output/report.json'
            if report.exists():report.unlink()
            path.write_text(json.dumps(base));(job/'run_progress.json').write_text(json.dumps(progress))
            result=self.runner('status','--job-id','job-1');self.assertEqual(result.returncode,0,result.stderr);row=json.loads(result.stdout);self.assertEqual(row['frames_completed'],progress['frames_completed'])
    def test_terminal_summary_conflicts_hold_status_and_cancel(self):
        job,path,base=self.status()
        for state,summary in [('completed','failed'),('completed','canceled'),('failed','passed'),('cancelled','passed')]:
            row=dict(base,state=state);path.write_text(json.dumps(row));before=path.read_bytes()
            (job/'result_summary.json').write_text(json.dumps({'status':summary}))
            for mode in ('status','cancel'):
                result=self.runner(mode,'--job-id','job-1');self.assertNotEqual(result.returncode,0);self.assertEqual(path.read_bytes(),before)
            self.assertFalse((job/'cancel_requested.flag').exists())
    def test_summary_identity_work_and_result_code_are_admitted(self):
        job,path,base=self.status();base.update(frames_requested=2,frames_completed=2,frame_index=1,sim_steps_per_frame=3,output_root=str(self.base/'output'));path.write_text(json.dumps(base));before=path.read_bytes()
        for changes in ({'schema':'other'},{'artifact_class':'disposable_build'},{'output_root':str(self.base/'foreign')},{'frames_requested':3},{'frames_completed':1},{'frames_completed':3},{'sim_steps_per_frame':4},{'frames_completed':1.0},{'result_code':True},{'result_code':1}):
            (job/'result_summary.json').write_text(json.dumps(dict(status='passed',**changes)))
            for mode in ('status','cancel'):
                result=self.runner(mode,'--job-id','job-1');self.assertNotEqual(result.returncode,0);self.assertEqual(path.read_bytes(),before)
            self.assertFalse((job/'cancel_requested.flag').exists())
    def test_terminal_record_cannot_regress_from_live_progress(self):
        job,path,base=self.status();before=path.read_bytes()
        (job/'run_progress.json').write_text(json.dumps({'status':'running'}))
        result=self.runner('status','--job-id','job-1');self.assertNotEqual(result.returncode,0);self.assertEqual(path.read_bytes(),before)
    def test_valid_summary_recovers_completed_counters_without_final_progress(self):
        job,path,base=self.status();base.update(state='running',frames_requested=2,frames_completed=1,frame_index=1,sim_steps_per_frame=3,sim_steps_total_in_frame=3,sim_steps_completed_in_frame=1,submitted_at_utc='2026-10-07T00:00:00Z',started_at_utc='2026-10-07T00:00:00Z',updated_at_utc='2026-10-07T00:00:00Z')
        path.write_text(json.dumps(base));(job/'result_summary.json').write_text(json.dumps({'status':'passed','frames_requested':2,'frames_completed':2,'sim_steps_per_frame':3,'result_code':0}))
        result=self.runner('status','--job-id','job-1');self.assertEqual(result.returncode,0,result.stderr);row=json.loads(result.stdout)
        self.assertEqual(row['state'],'completed');self.assertEqual(row['frames_completed'],2);self.assertEqual(row['sim_steps_total_in_frame'],0)
    def test_passed_summary_without_completed_output_receipt_holds_state(self):
        job,path,base=self.status();base.update(state='running',frames_requested=2,frames_completed=1,frame_index=1,sim_steps_per_frame=3,sim_steps_total_in_frame=3,sim_steps_completed_in_frame=1,submitted_at_utc='2026-10-07T00:00:00Z',started_at_utc='2026-10-07T00:00:00Z',updated_at_utc='2026-10-07T00:00:00Z')
        before=json.dumps(base);output=self.base/'output';marker=output/'.physics-sim-headless-owner';valid=marker.read_bytes()
        (job/'result_summary.json').write_text(json.dumps({'status':'passed','frames_requested':2,'frames_completed':2,'sim_steps_per_frame':3,'result_code':0}))
        for kind in ('missing','running','wrong-root','hardlink'):
            with self.subTest(kind=kind):
                path.write_text(before)
                if marker.exists():marker.unlink()
                if kind=='running':marker.write_bytes(valid.replace(b'completed',b'running'))
                elif kind=='wrong-root':marker.write_bytes(b'physics_sim_headless_output_v1 completed 0 0\n')
                elif kind=='hardlink':marker.write_bytes(valid);os.link(marker,self.base/'alias')
                for mode in ('status','cancel'):
                    result=self.runner(mode,'--job-id','job-1');self.assertNotEqual(result.returncode,0)
                    self.assertEqual(path.read_text(),before)
                    self.assertFalse((job/'cancel_requested.flag').exists())
                if kind=='hardlink':(self.base/'alias').unlink()
        marker.write_bytes(valid);path.write_text(before)
        result=self.runner('status','--job-id','job-1');self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(result.stdout)['state'],'completed')

    def test_completed_record_or_progress_without_summary_holds_original(self):
        for source in ('record','progress'):
            with self.subTest(source=source):
                job,path,base=self.status();base.update(frames_requested=2,frames_completed=2,frame_index=1,sim_steps_per_frame=3)
                if source=='progress':
                    base.update(state='running',stage='running',pid=os.getpid(),frames_completed=1)
                    (job/'run_progress.json').write_text(json.dumps({'status':'passed','frames_requested':2,'frames_completed':2,'frame_index':1,'sim_steps_completed_in_frame':0,'sim_steps_total_in_frame':0}))
                else:
                    progress=job/'run_progress.json'
                    if progress.exists():progress.unlink()
                summary=job/'result_summary.json'
                if summary.exists():summary.unlink()
                before=json.dumps(base);path.write_text(before)
                for mode in ('status','cancel'):
                    result=self.runner(mode,'--job-id','job-1');self.assertNotEqual(result.returncode,0)
                    self.assertEqual(path.read_text(),before);self.assertFalse((job/'cancel_requested.flag').exists())

    def test_shared_report_validation_failure_cannot_partially_replace_status(self):
        for field in ('submitted_at_utc','stage'):
            with self.subTest(field=field):
                job,path,base=self.status()
                base.update(frames_requested=1,frames_completed=1,frame_index=0,sim_steps_per_frame=1,
                            submitted_at_utc='2026-10-07T00:00:00Z',updated_at_utc='2026-10-07T00:00:00Z')
                base[field]='';before=json.dumps(base);path.write_text(before)
                (job/'result_summary.json').write_text(json.dumps({'status':'passed','frames_requested':1,'frames_completed':1,'sim_steps_per_frame':1,'result_code':0}))
                report=job/'output/report.json';report.parent.mkdir(exist_ok=True);report.write_bytes(b'previous report')
                for mode in ('status','cancel'):
                    path.write_text(before)
                    result=self.runner(mode,'--job-id','job-1');self.assertNotEqual(result.returncode,0)
                    self.assertEqual(path.read_text(),before);self.assertEqual(report.read_bytes(),b'previous report')
                    self.assertFalse((job/'cancel_requested.flag').exists())

    def test_invalid_existing_report_holds_changed_and_unchanged_refresh(self):
        for predecessor in (b'previous report', b'', b'[]', b'{} trailing', b'{"x":1,"x":2}'):
            for complete in (False, True):
                with self.subTest(predecessor=predecessor, unchanged=complete):
                    job,path,base=self.status()
                    base.update(request_path=str(job/'job_request.json'),frames_requested=1,
                                frames_completed=1,frame_index=0,sim_steps_per_frame=1,
                                submitted_at_utc='2026-10-07T00:00:00Z',
                                updated_at_utc='2026-10-07T00:00:00Z')
                    if complete:
                        base.update(exit_code=0,finished_at_utc='2026-10-07T00:00:00Z',diagnostics='simulation completed')
                    before=json.dumps(base);path.write_text(before)
                    (job/'result_summary.json').write_text(json.dumps({'status':'passed','frames_requested':1,'frames_completed':1,'sim_steps_per_frame':1,'result_code':0}))
                    report=job/'output/report.json';report.parent.mkdir(exist_ok=True);report.write_bytes(predecessor)
                    for mode in ('status','cancel','status'):
                        result=self.runner(mode,'--job-id','job-1');self.assertNotEqual(result.returncode,0)
                        self.assertEqual(path.read_text(),before);self.assertEqual(report.read_bytes(),predecessor)
                        self.assertFalse((job/'cancel_requested.flag').exists())

    def test_report_stage_lock_failure_preserves_status_and_report(self):
        job,path,base,report,original=self.paired_status()
        before=path.read_bytes();report_before=report.read_bytes()
        (job/'run_progress.json').write_text(json.dumps({
            'status':'passed','stage':base['stage'],'frames_requested':1,'frames_completed':1,
            'frame_index':0,'sim_steps_per_frame':1,'updated_at_utc':base['updated_at_utc']}))
        with (report.parent/'.physics-sim-job-metadata.lock').open('wb') as lock:
            fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
            for mode in ('status','cancel'):
                with self.subTest(mode=mode):
                    result=self.runner(mode,'--job-id','job-1');self.assertNotEqual(result.returncode,0)
                    self.assertEqual(path.read_bytes(),before);self.assertEqual(report.read_bytes(),report_before)
                    self.assertFalse((job/'cancel_requested.flag').exists())
        result=self.runner('status','--job-id','job-1');self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(json.loads(path.read_text())['exit_code'],0)
        self.assertEqual(json.loads(report.read_text()).get('state'),'succeeded')

    def test_pending_pair_evidence_holds_actual_status_and_cancel(self):
        job,path,base=self.status();before=path.read_bytes()
        hold=job/'.physics-sim-job-publication.pending';outside=self.base/'outside';outside.write_bytes(b'keep')
        for kind in ('empty','directory','symlink','hardlink'):
            with self.subTest(kind=kind):
                if kind=='empty':hold.write_bytes(b'')
                elif kind=='directory':hold.mkdir()
                elif kind=='symlink':hold.symlink_to(outside)
                else:os.link(outside,hold)
                for mode in ('status','cancel'):
                    result=self.runner(mode,'--job-id','job-1');self.assertNotEqual(result.returncode,0)
                    self.assertIn('operation',result.stderr)
                    self.assertEqual(path.read_bytes(),before);self.assertFalse((job/'cancel_requested.flag').exists())
                self.assertEqual(outside.read_bytes(),b'keep');self.assertEqual(list(job.glob('.job-pair-*')),[])
                if kind=='directory':hold.rmdir()
                else:hold.unlink()

    def test_report_scalar_or_schema_mismatch_holds_unchanged_status_and_cancel(self):
        job,path,base,report,original=self.paired_status();before=path.read_bytes()
        for field,value in [('job_id','another-job'),('program','another-program'),('state','failed'),
                            ('stage','different-stage'),('created_at','2026-10-06T00:00:00Z'),
                            ('started_at','2026-10-07T00:00:00Z'),('updated_at','2026-10-06T00:00:00Z'),
                            ('finished_at',''),('schema_family','other'),('schema_variant','other'),
                            ('job_id',9),('stage','x'*4096),('job_id','job-1\0foreign')]:
            with self.subTest(field=field,value=value):
                row=dict(original);row[field]=value;encoded=json.dumps(row).encode();report.write_bytes(encoded)
                for mode in ('status','cancel'):
                    result=self.runner(mode,'--job-id','job-1');self.assertNotEqual(result.returncode,0)
                    self.assertEqual(path.read_bytes(),before);self.assertEqual(report.read_bytes(),encoded)
                    self.assertFalse((job/'cancel_requested.flag').exists())
        for row in ({},dict(original,unexpected=1),{k:v for k,v in original.items() if k!='created_at'}):
            report.write_text(json.dumps(row));self.assertNotEqual(self.runner('status','--job-id','job-1').returncode,0)
            self.assertEqual(path.read_bytes(),before)

    def test_report_artifact_roles_and_paths_are_bound_before_status_or_cancel(self):
        job,path,base,report,original=self.paired_status();before=path.read_bytes()
        artifacts=original['artifacts'];wrong=json.loads(json.dumps(artifacts));wrong[0]['path']=str(self.base/'outside')
        unknown=json.loads(json.dumps(artifacts));unknown[0]['type']='unknown'
        bad_field=json.loads(json.dumps(artifacts));bad_field[0]['path']=9
        extra=json.loads(json.dumps(artifacts));extra[0]['extra']='unadmitted'
        for altered in ([],artifacts[:2],artifacts+[artifacts[0]],wrong,unknown,bad_field,extra,None):
            with self.subTest(artifacts=altered):
                row=dict(original,artifacts=altered);encoded=json.dumps(row).encode();report.write_bytes(encoded)
                for mode in ('status','cancel'):
                    result=self.runner(mode,'--job-id','job-1');self.assertNotEqual(result.returncode,0)
                    self.assertEqual(path.read_bytes(),before);self.assertEqual(report.read_bytes(),encoded)
                    self.assertFalse((job/'cancel_requested.flag').exists())

    def test_matching_report_accepts_reordering_and_optional_artifact_growth(self):
        job,path,base,report,original=self.paired_status();before=path.read_bytes()
        (Path(base['output_root'])/'volume_frames').mkdir()
        self.assertEqual(self.runner('status','--job-id','job-1').returncode,0)
        row=dict(original);row['artifacts']=list(reversed(original['artifacts']))+[
            {'type':'volume_frames','path':str(Path(base['output_root'])/'volume_frames')},
            {'type':'render_frames','path':str(Path(base['output_root'])/'render_frames')}]
        report.write_text(json.dumps(row))
        for mode in ('status','cancel'):self.assertEqual(self.runner(mode,'--job-id','job-1').returncode,0)
        self.assertEqual(path.read_bytes(),before)

if __name__=='__main__':unittest.main()
