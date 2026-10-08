"""Compile and run an ordinary native contract in a fresh retained evidence capsule."""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import time
import uuid

from build_owner import inherited_descriptors, run as owned_run
from cfd_evidence import admitted_path, experiment_root, file_fingerprint, sha, seal_bundle, read_json, verify_bundle


from agent_session.owned_command import execute, ExecutionFailure, IncompleteTeardown


def run(repo, build, parent, name, command, wall_cap=900, run_command=execute, *, runtime_args=(), assessor=None, jsonl_name=None, series=None, companion_sources=(), assessment_python_path=None):
    repo=repo.resolve();build=admitted_path(build if build.is_absolute() else repo/build)
    parent=experiment_root(repo,parent if parent.is_absolute() else repo/parent)
    if not build.is_relative_to(repo/'build'):raise ValueError('Contract build root must be inside checkout/build')
    if not re.fullmatch('[a-z0-9][a-z0-9-]{0,79}',name):raise ValueError('Invalid contract proof name')
    if not 1<=wall_cap<=3600:raise ValueError('Invalid contract wall cap')
    if command.count('-o')!=1:raise ValueError('Contract compiler requires one output')
    index=command.index('-o')+1
    if index>=len(command):raise ValueError('Missing contract output')
    hint=admitted_path(Path(command[index]) if Path(command[index]).is_absolute() else repo/command[index])
    if not hint.is_relative_to(build):raise ValueError('Contract output hint must be inside selected build root')
    selected=shutil.which(command[0])
    if selected is None:raise ValueError('Contract compiler unavailable')
    compiler=Path(selected).resolve();compiler_identity=file_fingerprint(compiler)
    if (not isinstance(runtime_args,(tuple,list)) or len(runtime_args)>16
            or any(not isinstance(v,str) or len(v)>4096 for v in runtime_args)):
        raise ValueError('Invalid contract runtime arguments')
    if jsonl_name is not None and (not isinstance(jsonl_name,str) or not re.fullmatch(r'[a-z][a-z0-9-]*\.jsonl',jsonl_name)):
        raise ValueError('Invalid retained JSONL filename')
    if series not in (None,'native-mixed','refined-matrix'):raise ValueError('Invalid retained resolution series')
    if series and (runtime_args or jsonl_name):raise ValueError('Series has its own runtime arguments and JSON output paths')
    if companion_sources and series!='refined-matrix':raise ValueError('Native companion requires matrix series')
    companions=[admitted_path(Path(p) if Path(p).is_absolute() else repo/p) for p in companion_sources]
    if any(not p.is_relative_to(repo) or p.suffix!='.c' for p in companions):raise ValueError('Native companions require checkout C sources')
    assessment_python=None;assessment_identity=None;assessment_binary=None;environment_config=None;environment_identity=None
    if assessor is not None:
        assessor=admitted_path(assessor if assessor.is_absolute() else repo/assessor)
        if not assessor.is_relative_to(repo/'scripts') or assessor.suffix!='.py' or (jsonl_name is None and series!='refined-matrix'):
            raise ValueError('Assessment requires checkout/scripts Python source and retained JSONL')
        assessment_python=(Path(os.path.abspath(assessment_python_path)) if assessment_python_path
                           else Path(sys.executable).resolve())
        admitted_path(assessment_python.parent)
        assessment_binary=assessment_python.resolve();assessment_identity=file_fingerprint(assessment_binary)
        environment_config=assessment_python.parent.parent/'pyvenv.cfg'
        if environment_config.exists():environment_identity=file_fingerprint(environment_config)
    sources={}
    for item in command:
        if item.endswith(('.c','.h')):
            path=admitted_path(Path(item) if Path(item).is_absolute() else repo/item)
            if not path.is_relative_to(repo):raise ValueError('Contract source must be inside checkout')
            sources[path]=file_fingerprint(path)
    if not sources:raise ValueError('Contract requires declared source files')
    if assessor is not None:sources[assessor]=file_fingerprint(assessor)
    for p in companions:sources[p]=file_fingerprint(p)
    # Freeze public headers and headers adjacent to each declared source. External
    # SDK/library and undeclared generated dependencies remain separately unqualified.
    headers=set((repo/'include').rglob('*.h')) if (repo/'include').exists() else set()
    for source in list(sources):headers.update(source.parent.glob('*.h'))
    for header in headers:sources[header]=file_fingerprint(header)
    pending=list(sources)
    while pending:
        source=pending.pop()
        if sources[source]['bytes']>2097152:raise ValueError('Contract source exceeds capture bound')
        for included in re.findall(r'^\s*#\s*include\s*"([^"\n]+)"',source.read_text(errors='replace'),re.MULTILINE):
            for candidate in (source.parent/included,repo/'include'/included):
                candidate=admitted_path(candidate)
                if not candidate.is_relative_to(repo):raise ValueError('Quoted include escapes checkout')
                if candidate.exists():
                    if candidate not in sources:
                        sources[candidate]=file_fingerprint(candidate);pending.append(candidate)
                    break
    controls={Path(__file__).resolve().parent/name:None for name in
        ('contract_proof.py','agent_session/owned_command.py','cfd_evidence.py','build_owner.py','build_outputs.py','clean_outputs.py','check_clean_root.py')}
    controls={path:file_fingerprint(path) for path in controls}
    parent.mkdir(parents=True,exist_ok=True);capsule=parent/(name+'-'+uuid.uuid4().hex);capsule.mkdir()
    state={'schema':'physics_sim_contract_proof_v1','artifact_class':'retained_contract_proof','status':'running',
        'name':name,'requested_command':command,'runtime_args':list(runtime_args),'jsonl_name':jsonl_name,'series':series,
        'assessor':str(assessor) if assessor else None,'assessment_python_sha256':assessment_identity['sha256'] if assessment_identity else None,
        'assessment_python_invocation':str(assessment_python) if assessment_python else None,
        'assessment_environment_config_sha256':environment_identity['sha256'] if environment_identity else None,'legacy_output_hint':str(hint),'legacy_outputs_modified':False,
        'compiler':str(compiler),'compiler_sha256':compiler_identity['sha256'],
        'sources_sha256':{str(p.relative_to(repo)):row['sha256'] for p,row in sources.items()},
        'control_sha256':{p.name:row['sha256'] for p,row in controls.items()},
        'complete_dependency_capture':False,'physical_accuracy_certified':False,'all_external_descendants_verified_terminal':False,'terminal_verification_scope':'Owned command outcome/anchor and local operations only; complete descendants unverified','terminal_processes_verified':False,'commands':[]}
    (capsule/'request.json').write_text(json.dumps(state,indent=2)+'\n')
    print('Contract proof retained: '+str(capsule),flush=True)
    frozen_identities={}
    try:
        if environment_identity:
            frozen_environment=capsule/'assessment-pyvenv.cfg'
            shutil.copy2(environment_config,frozen_environment)
            if sha(frozen_environment)!=environment_identity['sha256']:
                raise ValueError('Assessment environment changed during capture')
            frozen_identities[frozen_environment]=file_fingerprint(frozen_environment)
        for control,identity in controls.items():
            frozen=capsule/'control'/control.name;frozen.parent.mkdir(exist_ok=True)
            shutil.copy2(control,frozen)
            if sha(frozen)!=identity['sha256']:raise ValueError('Contract control changed during capture')
            frozen_identities[frozen]=file_fingerprint(frozen)
        for source,identity in sources.items():
            frozen=capsule/'source'/source.relative_to(repo);frozen.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(source,frozen)
            if sha(frozen)!=identity['sha256']:raise ValueError('Contract source changed during capture')
            frozen_identities[frozen]=file_fingerprint(frozen)
        argv=[str(compiler),*command[1:]];argv[index]=str(capsule/'contract')
        for i,item in enumerate(argv):
            path=Path(item) if Path(item).is_absolute() else repo/item
            if path in sources:argv[i]=str(capsule/'source'/path.relative_to(repo))
            elif item=='-Iinclude':argv[i]='-I'+str(capsule/'source/include')
        descriptors=inherited_descriptors(repo,build)
        state['commands'].append(run_command(argv,repo,capsule,'compile',descriptors,wall_cap))
        binary=capsule/'contract';binary_identity=file_fingerprint(binary)
        state['binary_sha256']=binary_identity['sha256']
        data_identities={}
        if series:
            if companions:
                native_command=[v for v in command if not v.endswith(('.c','.h'))]
                at=native_command.index('-o');native_command[at+1]=str(build/'cfd_refined_mixed_test')
                native_command[at:at]=[str(p) for p in companions]
                native=run(repo,build,capsule/'native-proof','native-mixed',native_command,wall_cap,run_command,series='native-mixed')
                state['native_companion']=native
                if native['status']!='passed':
                    if native['readback']['status']=='held':raise IncompleteTeardown('Native companion teardown unverified')
                    raise ValueError('Fresh native companion failed: '+str(native.get('failure')))
                for n in (8,16,32):
                    source=Path(native['capsule'])/('native-mixed-'+str(n)+'.json');destination=capsule/source.name
                    with source.open('rb') as inp,destination.open('xb') as out:shutil.copyfileobj(inp,out)
                    if sha(source)!=sha(destination):raise ValueError('Native companion copy drift')
                    data_identities[destination]=file_fingerprint(destination)
            for n in (8,16,32):
                output=capsule/(('native-mixed-' if series=='native-mixed' else 'matrix-')+str(n)+'.json')
                if output.exists():raise ValueError('Series output already exists')
                arguments=[str(n),str(output)] if series=='native-mixed' else [str(n)]
                tag='run-'+str(n)
                state['commands'].append(run_command([str(binary),*arguments],capsule,capsule,tag,descriptors,wall_cap))
                if series=='refined-matrix':
                    with (capsule/(tag+'.stdout')).open('rb') as inp,output.open('xb') as out:shutil.copyfileobj(inp,out)
                row=read_json(output,67108864)
                keys=('u','v','p') if series=='native-mixed' else ('cells','faces','entries')
                if not isinstance(row,dict) or set(row)!=set(keys) or any(not isinstance(row[k],list) or not row[k] for k in keys):
                    raise ValueError('Invalid retained series JSON')
                data_identities[output]=file_fingerprint(output)
        else:
            state['commands'].append(run_command([str(binary),*runtime_args],capsule,capsule,'run',descriptors,wall_cap))
        if jsonl_name is not None:
            with (capsule/jsonl_name).open('xb') as out, (capsule/'run.stdout').open('rb') as inp:
                shutil.copyfileobj(inp,out);out.flush();os.fsync(out.fileno())
            data_identity=file_fingerprint(capsule/jsonl_name);state['jsonl_sha256']=data_identity['sha256'];data_identities[capsule/jsonl_name]=data_identity
        if assessor is not None:
            frozen=capsule/'source'/assessor.relative_to(repo)
            arguments=['--root',str(capsule)] if series else [str(capsule/jsonl_name)]
            state['commands'].append(run_command([str(assessment_python),'-E','-B',str(frozen),*arguments],capsule,capsule,'assess',descriptors,wall_cap))
            if assessment_python.resolve()!=assessment_binary or file_fingerprint(assessment_binary)!=assessment_identity:
                raise ValueError('Assessment interpreter changed during proof')
            if environment_identity and file_fingerprint(environment_config)!=environment_identity:raise ValueError('Assessment environment configuration changed')
            if not environment_identity and environment_config.exists():raise ValueError('Assessment environment configuration appeared during proof')
        if any(file_fingerprint(p)!=r for p,r in data_identities.items()):raise ValueError('Assessment data changed during proof')
        state['data_sha256']={p.name:r['sha256'] for p,r in data_identities.items()}
        if any(file_fingerprint(p)!=r for p,r in frozen_identities.items()):raise ValueError('Frozen contract input changed during proof')
        if state.get('native_companion'):verify_bundle(Path(state['native_companion']['capsule']))
        if file_fingerprint(compiler)!=compiler_identity or any(file_fingerprint(p)!=row for p,row in sources.items()):
            raise ValueError('Contract source/compiler changed during proof')
        if any(file_fingerprint(p)!=row for p,row in controls.items()):raise ValueError('Contract control changed during proof')
        if file_fingerprint(binary)!=binary_identity:raise ValueError('Contract binary changed during proof')
        state.update(status='passed',terminal_processes_verified=True)
    except (Exception,KeyboardInterrupt) as error:
        state.update(status='failed',failure=str(error),terminal_processes_verified=not isinstance(error,IncompleteTeardown))
    (capsule/'receipt.json').write_text(json.dumps(state,indent=2)+'\n')
    readback=seal_bundle(capsule) if state.get('terminal_processes_verified',True) else {'status':'held','reason':'Unverified owned process teardown; attempt remains unsealed'}
    return {'status':state['status'],'capsule':str(capsule),'readback':readback,'failure':state.get('failure')}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-root',required=True,type=Path);parser.add_argument('--parent',required=True,type=Path)
    parser.add_argument('--name',required=True);parser.add_argument('--wall-cap',type=int,default=900)
    parser.add_argument('--run-arg',action='append',default=[])
    parser.add_argument('--series',choices=('native-mixed','refined-matrix'))
    parser.add_argument('--native-source',type=Path,action='append',default=[])
    parser.add_argument('--assessment-python',type=Path)
    parser.add_argument('--assessor',type=Path);parser.add_argument('--jsonl-name')
    parser.add_argument('command',nargs=argparse.REMAINDER);args=parser.parse_args()
    command=args.command[1:] if args.command[:1]==['--'] else args.command
    try:
        repo=Path.cwd().resolve();build=admitted_path(args.build_root)
        if not inherited_descriptors(repo,build):
            raise SystemExit(owned_run(build,[sys.executable,'-B',str(Path(__file__).resolve()),*sys.argv[1:]]))
        result=run(repo,build,args.parent,args.name,command,args.wall_cap,runtime_args=args.run_arg,assessor=args.assessor,jsonl_name=args.jsonl_name,series=args.series,companion_sources=args.native_source,assessment_python_path=args.assessment_python);print(json.dumps(result))
        raise SystemExit(0 if result['status']=='passed' else 2)
    except (ValueError,OSError) as error:parser.exit(2,'Contract proof held: '+str(error)+'\n')


if __name__=='__main__':main()
