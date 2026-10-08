"""Native contract proof lifetime keeps reruns and failed logs separate."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from contract_proof import run, execute, IncompleteTeardown
from cfd_evidence import verify_bundle
from clean_outputs import plan

TARGETS=('test-cfd-obstacle3d-box','test-cfd-obstacle3d-box-sanitize',
    'test-cfd-obstacle3d-box-material','test-cfd-obstacle3d-box-material-sanitize',
    'test-cfd-3d-box-session','test-cfd-3d-box-session-sanitize')


class ContractProof(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.repo=Path(self.temp.name).resolve();(self.repo/'tests').mkdir();(self.repo/'include').mkdir()
        self.source=self.repo/'tests/probe.c';self.source.write_text('#include "proof.h"\n#include "implementation.c"\n#include <stdio.h>\nint main(void){puts("contract passed");return result();}\n')
        (self.repo/'tests/implementation.c').write_text('int result(void){return RESULT;}\n')
        (self.repo/'include/proof.h').write_text('#define RESULT 0\n')
        self.build=self.repo/'build/profile';self.parent=self.repo/'data/experiments/contracts'
        self.command=['clang','-Iinclude',str(self.source),'-o',str(self.build/'legacy')]

    def test_real_compiler_reruns_freeze_sources_and_preserve_legacy_bytes(self):
        self.build.mkdir(parents=True);legacy=self.build/'legacy';legacy.write_bytes(b'old binary')
        log=self.build/'old.jsonl';log.write_bytes(b'old numerical evidence')
        before=(legacy.read_bytes(),log.read_bytes())
        results=[run(self.repo,self.build,self.parent,'probe',self.command) for _ in range(2)]
        self.assertNotEqual(results[0]['capsule'],results[1]['capsule'])
        for result in results:
            self.assertEqual(result['status'],'passed',result)
            capsule=Path(result['capsule']);verify_bundle(capsule)
            self.assertIn('contract passed',(capsule/'run.stdout').read_text())
            self.assertEqual((capsule/'source/include/proof.h').read_bytes(),(self.repo/'include/proof.h').read_bytes())
            receipt=json.loads((capsule/'receipt.json').read_text())
            self.assertIn('/source/tests/probe.c',receipt['commands'][0]['command'][2])
            self.assertFalse(receipt['complete_dependency_capture'])
        self.assertEqual((legacy.read_bytes(),log.read_bytes()),before)

    def test_runtime_and_compile_failures_retain_sealed_attempts(self):
        (self.repo/'include/proof.h').write_text('#define RESULT 7\n')
        result=run(self.repo,self.build,self.parent,'failure',self.command)
        self.assertEqual(result['status'],'failed');verify_bundle(Path(result['capsule']))
        self.assertIn('exited 7',result['failure'])
        self.source.write_text('invalid C;\n')
        result=run(self.repo,self.build,self.parent,'failure',self.command)
        self.assertEqual(result['status'],'failed');verify_bundle(Path(result['capsule']))
        self.assertTrue((Path(result['capsule'])/'compile.stderr').read_text())

    def test_protected_and_symlink_admission_without_capsule(self):
        for parent in (self.repo/'src/contracts',self.build/'contracts'):
            with self.assertRaises(ValueError):run(self.repo,self.build,parent,'probe',self.command)
        (self.repo/'include/alias.h').symlink_to(self.source)
        with self.assertRaises(ValueError):run(self.repo,self.build,self.parent,'probe',self.command)
        self.assertFalse(self.parent.exists())

    def test_source_drift_is_terminal_failed_and_cleanup_holds_proof_class(self):
        def mutate(command,cwd,directory,tag,descriptors,wall_cap):
            result=execute(command,cwd,directory,tag,descriptors,wall_cap)
            if tag=='run':self.source.write_text('changed source')
            return result
        result=run(self.repo,self.build,self.parent,'drift',self.command,run_command=mutate)
        self.assertEqual(result['status'],'failed');self.assertIn('changed during proof',result['failure'])
        verify_bundle(Path(result['capsule']))
        self.build.mkdir(parents=True);(self.build/'receipt.json').write_text('{"artifact_class":"retained_contract_proof"}')
        with self.assertRaises(ValueError):plan(self.repo,self.build,[],[])

    def test_wall_and_log_caps_retain_logs(self):
        directory=self.repo/'logs';directory.mkdir()
        with self.assertRaisesRegex(ValueError,'wall cap'):
            execute([sys.executable,'-c','import time;time.sleep(10)'],self.repo,directory,'wait',(),.1)
        with self.assertRaisesRegex(ValueError,'log cap'):
            execute([sys.executable,'-c','print("x"*10000)'],self.repo,directory,'noisy',(),1,64)
        self.assertTrue((directory/'noisy.stdout').exists())

    def test_jsonl_runtime_arguments_and_frozen_assessment_reruns(self):
        (self.repo/'scripts').mkdir();assessor=self.repo/'scripts/assess.py'
        self.source.write_text('#include <stdio.h>\nint main(int argc,char **argv){(void)argv;puts(argc>1 ? "{\\\"small\\\":1}" : "{\\\"full\\\":1}");return 0;}\n')
        assessor.write_text('import json,sys\nfrom pathlib import Path\nassert json.loads(Path(sys.argv[1]).read_text())=={"small":1}\nprint("assessment passed")\n')
        results=[run(self.repo,self.build,self.parent,'jsonl',self.command,runtime_args=('small',),assessor=assessor,jsonl_name='data.jsonl') for _ in range(2)]
        self.assertNotEqual(results[0]['capsule'],results[1]['capsule'])
        for result in results:
            self.assertEqual(result['status'],'passed',result);capsule=Path(result['capsule']);verify_bundle(capsule)
            self.assertEqual((capsule/'data.jsonl').read_bytes(),(capsule/'run.stdout').read_bytes())
            self.assertEqual(json.loads((capsule/'data.jsonl').read_text()),{'small':1})
            self.assertIn('assessment passed',(capsule/'assess.stdout').read_text())
            receipt=json.loads((capsule/'receipt.json').read_text());self.assertIn('/source/scripts/assess.py',receipt['commands'][2]['command'][3]);self.assertIn('-E',receipt['commands'][2]['command'])
        assessor.write_text('raise SystemExit(7)\n')
        result=run(self.repo,self.build,self.parent,'reject',self.command,assessor=assessor,jsonl_name='data.jsonl')
        self.assertEqual(result['status'],'failed');self.assertIn('assess exited 7',result['failure']);verify_bundle(Path(result['capsule']))
        self.assertTrue((Path(result['capsule'])/'data.jsonl').is_file())
    def test_jsonl_path_and_assessor_admission(self):
        for name in ('../results.jsonl','compile.stdout','/results.jsonl'):
            with self.assertRaises(ValueError):run(self.repo,self.build,self.parent,'bad',self.command,jsonl_name=name)
        with self.assertRaises(ValueError):run(self.repo,self.build,self.parent,'bad',self.command,assessor=self.repo/'tests/assess.py',jsonl_name='data.jsonl')
        self.assertFalse(self.parent.exists())
    def test_teardown_refusal_never_accepts_success(self):
        directory=self.repo/'refused';directory.mkdir()
        with patch('contract_proof.os.killpg',side_effect=PermissionError('controlled refusal')):
            with self.assertRaisesRegex(ValueError,'teardown unverified'):
                execute([sys.executable,'-c','print("done")'],self.repo,directory,'refused',())

    def test_unverified_process_teardown_never_seals_attempt(self):
        def held(*args):raise IncompleteTeardown('controlled unfinished process')
        result=run(self.repo,self.build,self.parent,'held',self.command,run_command=held)
        self.assertEqual(result['status'],'failed');self.assertEqual(result['readback']['status'],'held')
        capsule=Path(result['capsule']);self.assertFalse((capsule/'bundle_manifest.json').exists());self.assertTrue((capsule/'receipt.json').is_file())
        self.assertFalse(json.loads((capsule/'receipt.json').read_text())['terminal_processes_verified'])

    def series_sources(self):
        native=self.repo/'tests/native.c'
        native.write_text('#include <stdio.h>\n#include <stdlib.h>\nint main(int argc,char **argv){if(argc!=3)return 8;int n=atoi(argv[1]);FILE *f=fopen(argv[2],"wx");if(!f)return 9;fprintf(f,"{\\\"u\\\":[%d],\\\"v\\\":[%d],\\\"p\\\":[%d]}",n,n,n);return fclose(f);}\n'.replace('\\\"','\\"'))
        self.source.write_text('#include <stdio.h>\n#include <stdlib.h>\nint main(int argc,char **argv){if(argc!=2)return 8;int n=atoi(argv[1]);printf("{\\\"cells\\\":[%d],\\\"faces\\\":[%d],\\\"entries\\\":[%d]}",n,n,n);return 0;}\n'.replace('\\\"','\\"'))
        (self.repo/'scripts').mkdir()
        assessor=self.repo/'scripts/assess.py'
        assessor.write_text('import json,sys\nfrom pathlib import Path\nassert sys.argv[1]=="--root"\nroot=Path(sys.argv[2])\nfor n in (8,16,32):\n assert json.loads((root/f"native-mixed-{n}.json").read_text())=={"u":[n],"v":[n],"p":[n]}\n assert json.loads((root/f"matrix-{n}.json").read_text())=={"cells":[n],"faces":[n],"entries":[n]}\nassert sys.flags.optimize==0\nprint("fresh companion assessment passed")\n')
        return native,assessor

    def test_series_reruns_use_fresh_native_companions_and_immutable_inputs(self):
        native,assessor=self.series_sources()
        self.build.mkdir(parents=True);legacy=self.build/'native-mixed-8.json';legacy.write_text('historical sentinel')
        results=[run(self.repo,self.build,self.parent,'series',self.command,series='refined-matrix',companion_sources=(native,),assessor=assessor) for _ in range(2)]
        self.assertNotEqual(results[0]['capsule'],results[1]['capsule'])
        for result in results:
            self.assertEqual(result['status'],'passed',result);capsule=Path(result['capsule']);verify_bundle(capsule)
            receipt=json.loads((capsule/'receipt.json').read_text())
            nested=Path(receipt['native_companion']['capsule']);verify_bundle(nested)
            self.assertEqual(len(receipt['data_sha256']),6)
            for n in (8,16,32):
                self.assertEqual((capsule/f'native-mixed-{n}.json').read_bytes(),(nested/f'native-mixed-{n}.json').read_bytes())
                self.assertEqual((capsule/f'matrix-{n}.json').read_bytes(),(capsule/f'run-{n}.stdout').read_bytes())
            self.assertIn('fresh companion',(capsule/'assess.stdout').read_text())
        self.assertEqual(legacy.read_text(),'historical sentinel')

    def test_native_series_failure_retains_partial_json_and_stops_later_sizes(self):
        native,_=self.series_sources()
        native.write_text(native.read_text().replace('FILE *f=', 'if(n==16)return 7;FILE *f='))
        command=['clang',str(native),'-o',str(self.build/'native')]
        result=run(self.repo,self.build,self.parent,'partial',command,series='native-mixed')
        self.assertEqual(result['status'],'failed',result);self.assertIn('run-16 exited 7',result['failure'])
        capsule=Path(result['capsule']);verify_bundle(capsule)
        self.assertTrue((capsule/'native-mixed-8.json').is_file())
        self.assertTrue((capsule/'run-16.stderr').is_file())
        self.assertFalse((capsule/'run-32.stdout').exists())
        self.assertTrue(json.loads((capsule/'receipt.json').read_text())['terminal_processes_verified'])

    def test_native_companion_unverified_teardown_holds_outer_attempt(self):
        native,assessor=self.series_sources()
        def hold(command,cwd,directory,tag,descriptors,wall_cap):
            if directory.parent.name=='native-proof':raise IncompleteTeardown('unfinished native companion')
            return execute(command,cwd,directory,tag,descriptors,wall_cap)
        result=run(self.repo,self.build,self.parent,'held-series',self.command,series='refined-matrix',companion_sources=(native,),assessor=assessor,run_command=hold)
        self.assertEqual(result['status'],'failed',result);self.assertEqual(result['readback']['status'],'held')
        capsule=Path(result['capsule']);receipt=json.loads((capsule/'receipt.json').read_text())
        self.assertFalse((capsule/'bundle_manifest.json').exists())
        self.assertFalse((Path(receipt['native_companion']['capsule'])/'bundle_manifest.json').exists())
        self.assertFalse((capsule/'run-8.stdout').exists())

    def test_series_assessment_data_drift_retains_failed_attempt(self):
        native,assessor=self.series_sources()
        assessor.write_text(assessor.read_text()+'(root/"matrix-8.json").write_text("{}")\n')
        result=run(self.repo,self.build,self.parent,'drift-series',self.command,series='refined-matrix',companion_sources=(native,),assessor=assessor)
        self.assertEqual(result['status'],'failed',result);self.assertIn('Assessment data changed',result['failure']);verify_bundle(Path(result['capsule']))

    def test_series_argument_and_companion_admission_without_allocation(self):
        for options in ({'series':'unknown'},{'series':'native-mixed','runtime_args':('small',)},
                {'series':'native-mixed','jsonl_name':'data.jsonl'},
                {'series':'native-mixed','companion_sources':(self.source,)},
                {'series':'refined-matrix','companion_sources':(self.repo/'../outside.c',)}):
            with self.assertRaises(ValueError):run(self.repo,self.build,self.parent,'invalid-series',self.command,**options)
        self.assertFalse(self.parent.exists())

    def test_frozen_source_and_nested_native_drift_cannot_pass_series(self):
        native,assessor=self.series_sources()
        for family in ('source','nested'):
            def mutate(command,cwd,directory,tag,descriptors,wall_cap):
                result=execute(command,cwd,directory,tag,descriptors,wall_cap)
                if tag=='assess':
                    target=(directory/'source/tests/probe.c' if family=='source' else next((directory/'native-proof').glob('native-mixed-*/native-mixed-8.json')))
                    target.write_text('changed retained input')
                return result
            result=run(self.repo,self.build,self.parent,'frozen-drift',self.command,series='refined-matrix',companion_sources=(native,),assessor=assessor,run_command=mutate)
            self.assertEqual(result['status'],'failed',result)
            self.assertIn('changed',result['failure'].lower());verify_bundle(Path(result['capsule']))

    def test_explicit_venv_prefix_capture_and_environment_drift(self):
        native,assessor=self.series_sources();environment=self.repo/'data/tools/assessment-env'
        subprocess.run([sys.executable,'-m','venv','--without-pip',str(environment)],check=True,timeout=30,capture_output=True)
        assessor.write_text(assessor.read_text()+f'assert Path(sys.prefix)==Path({str(environment)!r})\n')
        result=run(self.repo,self.build,self.parent,'venv-series',self.command,series='refined-matrix',companion_sources=(native,),assessor=assessor,assessment_python_path=environment/'bin/python')
        self.assertEqual(result['status'],'passed',result);capsule=Path(result['capsule']);verify_bundle(capsule)
        self.assertEqual((capsule/'assessment-pyvenv.cfg').read_bytes(),(environment/'pyvenv.cfg').read_bytes())
        def mutate(command,cwd,directory,tag,descriptors,wall_cap):
            result=execute(command,cwd,directory,tag,descriptors,wall_cap)
            if tag=='assess':
                config=environment/'pyvenv.cfg';config.write_text(config.read_text()+'# changed after execution\n')
            return result
        result=run(self.repo,self.build,self.parent,'venv-drift',self.command,series='refined-matrix',companion_sources=(native,),assessor=assessor,assessment_python_path=environment/'bin/python',run_command=mutate)
        self.assertEqual(result['status'],'failed',result);self.assertIn('environment configuration changed',result['failure']);verify_bundle(Path(result['capsule']))

    def test_actual_six_make_recipe_bodies_use_unique_proofs(self):
        (self.repo/'scripts').mkdir()
        for name in ('contract_proof.py','build_owner.py','build_outputs.py','clean_outputs.py','check_clean_root.py','cfd_evidence.py'):
            shutil.copy2(ROOT/'scripts'/name,self.repo/'scripts'/name)
        (self.repo/'scripts/agent_session').mkdir()
        shutil.copy2(ROOT/'scripts/agent_session/owned_command.py',self.repo/'scripts/agent_session/owned_command.py')
        for name in ('cfd_obstacle3d_box_contract_test.c','cfd_obstacle3d_box_material_test.c','cfd_3d_box_session_test.c'):
            (self.repo/'tests'/name).write_text('int main(void){return 0;}\n')
        compiler=self.repo/'compiler.py'
        compiler.write_text('import pathlib,sys\nout=pathlib.Path(sys.argv[sys.argv.index("-o")+1]);out.write_text("#!/bin/sh\\necho proof\\n");out.chmod(0o755)\n')
        bodies=[];active=False
        for line in (ROOT/'make/rules-tools.mk').read_text().splitlines():
            if line and not line.startswith(('\t',' ','#')):
                active=line.split(':')[0] in TARGETS
                if active:bodies.append(line)
            elif active:bodies.append(line)
        (self.repo/'Makefile').write_text('BUILD_DIR=build/profile\nEXPERIMENT_DIR=data/experiments\nCC='+sys.executable+' -B compiler.py\n'+ '\n'.join(bodies)+'\n')
        env={k:v for k,v in os.environ.items() if k not in ('MAKEFLAGS','MFLAGS','MAKEOVERRIDES') and not k.startswith('PHYSICS_SIM_')}
        result=subprocess.run(['make',*TARGETS],cwd=self.repo,env=env,text=True,capture_output=True,timeout=40)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        capsules=list((self.repo/'data/experiments/contract-proofs').iterdir());self.assertEqual(len(capsules),6)
        for capsule in capsules:verify_bundle(capsule)
        self.assertFalse((self.build/'c3d-box').exists())


if __name__=='__main__':unittest.main()
