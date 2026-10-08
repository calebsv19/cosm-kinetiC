"""Retained local package assembly and verified reuse, without release authority."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import uuid

from build_owner import inherited_descriptors
from check_clean_root import read_json
from clean_outputs import no_symlinks
from desktop_replace import digest, inventory, rename_exclusive, write_record, InventoryBudget, INVENTORY_LIMITS
from package_outputs import plan
from contract_proof import execute, IncompleteTeardown, ExecutionFailure


def input_identity(inputs, values, *, budget=None):
    budget=budget or InventoryBudget();rows={}
    for path in inputs:
        budget.check();path=Path(os.path.abspath(path))
        if path.resolve()!=path:raise ValueError('Package input symlink is unclassified: '+str(path))
        if path.exists():rows[str(path)]=inventory(path,budget=budget)
        else:budget.entry();rows[str(path)]=None
    value={'inputs':rows,'values':values,'environment':{name:os.environ.get(name) for name in ('LANG','LC_ALL','TZ','TAR_OPTIONS','GZIP','COPYFILE_DISABLE','SOURCE_DATE_EPOCH','PYTHONPATH')}}
    return hashlib.sha256(json.dumps(value,sort_keys=True).encode()).hexdigest(),value


def output_inventory(root, outputs, *, budget=None):
    budget=budget or InventoryBudget();rows={}
    for row in outputs:
        path=root/row['relative']
        if row['kind']=='directory' and (not path.is_dir() or path.is_symlink()):raise ValueError('Missing package directory: '+str(path))
        if row['kind']=='file' and (not path.is_file() or path.is_symlink()):raise ValueError('Missing package file: '+str(path))
        rows[row['relative']]=inventory(path,budget=budget)
    return rows


def parse_assignments(values):
    result={}
    for value in values:
        name,separator,path=value.partition('=')
        if not separator or not re.fullmatch('[A-Z][A-Z0-9_]*',name) or name in result:
            raise ValueError('Package path mapping must be a unique VARIABLE=path')
        result[name]=Path(os.path.abspath(path))
    return result


def completed_evidence(receipt, record, repo):
    if record.get('terminal_processes_verified') is False:raise ValueError('Package transaction has unverified assembly teardown')
    if record.get('state')=='completed':return str(receipt)
    for proof in sorted((receipt.parent/'recovery').glob('*/receipt.json')):
        no_symlinks(proof,repo);row=read_json(proof,16*1024*1024)
        if not isinstance(row,dict):raise ValueError('Invalid package recovery receipt')
        if row.get('terminal_processes_verified') is False:raise ValueError('Package recovery has unverified copy teardown')
        if (row.get('schema')=='physics_sim_package_recovery_v1' and row.get('state')=='completed'
                and row.get('attempt_receipt_sha256')==digest(receipt)
                and row.get('output_root')==record.get('output_root')
                and row.get('identity')==record.get('identity')
                and row.get('output_inventory')==record.get('output_inventory')):
            return str(proof)
    return None


def recovery_plan(repo, attempt):
    repo=repo.resolve();attempt=Path(os.path.abspath(attempt));no_symlinks(attempt,repo)
    if not attempt.is_dir() or attempt.parent.name!='.package-transactions':
        raise ValueError('Recovery requires an exact retained package attempt directory')
    receipt=attempt/'receipt.json';row=read_json(receipt,16*1024*1024)
    if isinstance(row,dict) and row.get('terminal_processes_verified') is False:
        raise ValueError('Package recovery held by unverified assembly teardown')
    if not isinstance(row,dict) or row.get('schema')!='physics_sim_package_transaction_v1':
        raise ValueError('Recovery requires a typed package transaction')
    if not isinstance(row.get('output_root'),str) or not Path(row['output_root']).is_absolute():
        raise ValueError('Recovery journal has an invalid output root')
    root=Path(row['output_root'])
    if attempt.parent!=root/'.package-transactions':raise ValueError('Recovery requires an exact retained package attempt')
    for proof in (attempt/'recovery').glob('*/receipt.json'):
        no_symlinks(proof,repo);previous=read_json(proof,16*1024*1024)
        if not isinstance(previous,dict):raise ValueError('Invalid package recovery receipt')
        if previous.get('terminal_processes_verified') is False:
            raise ValueError('Package recovery held by unverified copy teardown')
    contract=row.get('contract')
    if not isinstance(contract,dict) or not isinstance(contract.get('outputs'),list) or not contract['outputs']:
        raise ValueError('Package recovery has no declared outputs')
    if hashlib.sha256(json.dumps(contract,sort_keys=True).encode()).hexdigest()!=row.get('selection'):
        raise ValueError('Package recovery contract identity mismatch')
    outputs=contract['outputs'];directories=[];files=[]
    for output in outputs:
        if not isinstance(output,dict) or output.get('kind') not in ('directory','file') or not isinstance(output.get('relative'),str):
            raise ValueError('Invalid package recovery output')
        relative=Path(output['relative'])
        if relative.is_absolute() or '..' in relative.parts or relative==Path('.') or relative.parts[0] in ('.package-transactions','.package-reservations'):
            raise ValueError('Recovery output escapes package namespace')
        (directories if output['kind']=='directory' else files).append(root/relative)
    for output in outputs:
        if any(Path(other['relative']).is_relative_to(output['relative']) for other in outputs if other!=output):
            raise ValueError('Package recovery outputs must not overlap')
    admission=plan(repo,root,directories,files)
    for selected in admission['outputs']:
        reservation=read_json(Path(selected['reservation']),1024*1024)
        if not isinstance(reservation,dict) or reservation.get('attempt_id')!=attempt.name or reservation.get('output')!=selected['path']:
            raise ValueError('Recovery output reservation ownership mismatch')
    expected=row.get('output_inventory')
    if not isinstance(expected,dict) or not expected:raise ValueError('Assembly never reached verified staging; select a fresh controlled root')
    stage=attempt/'stage';no_symlinks(stage,repo)
    if output_inventory(stage,outputs)!=expected:raise ValueError('Retained package stage inventory mismatch')
    source=row.get('source_identity')
    if not isinstance(source,dict) or not isinstance(source.get('inputs'),dict) or not isinstance(source.get('values'),list):
        raise ValueError('Invalid package source identity')
    identity,current=input_identity([Path(name) for name in source['inputs']],source['values'])
    if identity!=row.get('identity') or current!=source:raise ValueError('Package recovery inputs changed')
    missing=[];final_budget=InventoryBudget()
    for output in outputs:
        final=root/output['relative'];no_symlinks(final,repo)
        if final.exists():
            observed=output_inventory(root,[output],budget=final_budget)[output['relative']]
            if observed!=expected.get(output['relative']):raise ValueError('Recovery refuses changed published output: '+str(final))
        else:missing.append(output['relative'])
    copy_tool=row.get('directory_copy_tool')
    if copy_tool not in (None,'/usr/bin/ditto'):raise ValueError('Unclassified recovery directory copy tool')
    if sys.platform=='darwin' and any(o['kind']=='directory' and o['relative'] in missing for o in outputs):
        if copy_tool!='/usr/bin/ditto' or copy_tool not in source['inputs']:
            raise ValueError('Recovery requires bound macOS metadata copy tool')
    return {'schema':'physics_sim_package_recovery_plan_v1','attempt':str(attempt),'output_root':str(root),
            'attempt_receipt_sha256':digest(receipt),'identity':identity,'outputs':outputs,
            'directory_copy_tool':copy_tool,'output_inventory':expected,'missing':missing,'action':'publish_missing' if missing else 'reconcile_all',
            'release_authority_granted':False,'authentication_verified':False}


def recover(repo, attempt, apply=False):
    planned=recovery_plan(repo,attempt)
    if not apply:return dict(planned,status='plan_only')
    repo=repo.resolve();attempt=Path(planned['attempt']);root=Path(planned['output_root'])
    descriptors=[];journal=None;record=None
    try:
        for path,mode in ((repo/'tmp/locks/clean.lock',fcntl.LOCK_SH),(root/'.package-transactions/owner.lock',fcntl.LOCK_EX)):
            no_symlinks(path,repo);fd=os.open(path,os.O_RDWR|os.O_NOFOLLOW);descriptors.append(fd)
            try:fcntl.flock(fd,mode|fcntl.LOCK_NB)
            except BlockingIOError:raise ValueError('Package recovery held by active cleanup or package owner')
        if recovery_plan(repo,attempt)!=planned:raise ValueError('Package recovery plan changed before application')
        original=read_json(attempt/'receipt.json',16*1024*1024)
        proof=completed_evidence(attempt/'receipt.json',original,repo)
        if proof and not planned['missing']:return dict(planned,status='already_completed',completion_receipt=proof)
        parent=attempt/'recovery';no_symlinks(parent,repo);parent.mkdir(exist_ok=True)
        journal=parent/str(uuid.uuid4());journal.mkdir()
        record=dict(planned,schema='physics_sim_package_recovery_v1',artifact_class='local_package_staging',state='copying',published=[],terminal_processes_verified=False,all_external_descendants_verified_terminal=False,terminal_verification_scope='Owned copy commands/local operations only; complete descendants unverified')
        write_record(journal/'receipt.json',record)
        copy_budget=InventoryBudget()
        for index,output in enumerate(planned['outputs']):
            if output['relative'] not in planned['missing']:continue
            staged=attempt/'stage'/output['relative'];copied=journal/'publish'/output['relative']
            copied.parent.mkdir(parents=True,exist_ok=True)
            if output['kind']=='directory' and planned['directory_copy_tool']:
                execute([planned['directory_copy_tool'],'--rsrc','--extattr','--acl',str(staged),str(copied)],
                        repo,journal,'recovery_copy_'+str(index),tuple(descriptors),1800,67108864)
            elif output['kind']=='directory':shutil.copytree(staged,copied,symlinks=True)
            else:shutil.copy2(staged,copied)
            if output_inventory(journal/'publish',[output],budget=copy_budget)[output['relative']]!=planned['output_inventory'][output['relative']]:
                raise ValueError('Recovery publication copy mismatch')
        record['terminal_processes_verified']=True;write_record(journal/'receipt.json',record)
        if recovery_plan(repo,attempt)!=planned:raise ValueError('Package recovery inputs changed during copying')
        record['state']='publishing';write_record(journal/'receipt.json',record)
        for output in planned['outputs']:
            if output['relative'] not in planned['missing']:continue
            final=root/output['relative'];final.parent.mkdir(parents=True,exist_ok=True);no_symlinks(final,repo)
            rename_exclusive(journal/'publish'/output['relative'],final)
            record['published'].append(output['relative']);write_record(journal/'receipt.json',record)
        final_plan=recovery_plan(repo,attempt)
        if final_plan['missing']:raise ValueError('Package recovery remains incomplete')
        record['state']='completed';write_record(journal/'receipt.json',record)
        return dict(planned,status='completed',completion_receipt=str(journal/'receipt.json'))
    except BaseException as error:
        if journal is not None and record is not None:
            record['last_state']=record['state'];record['state']='failed_retained';record['failure']=str(error)
            record['terminal_processes_verified']=not isinstance(error,IncompleteTeardown)
            write_record(journal/'receipt.json',record)
        raise
    finally:
        for fd in descriptors:os.close(fd)


def run(repo, root, directories, files, mappings, inputs, values, command, tools=(), *, wall_cap=1800, log_cap=67108864, run_command=execute):
    repo=repo.resolve();root=Path(os.path.abspath(root))
    if type(wall_cap) not in (int,float) or not 0<wall_cap<=3600:raise ValueError('Invalid package assembly wall cap')
    if type(log_cap) is not int or not 1<=log_cap<=67108864:raise ValueError('Invalid package assembly log cap')
    planned=plan(repo,root,list(directories.values()),list(files.values()))
    outputs=[]
    for kind,group in (('directory',directories),('file',files)):
        for name,path in group.items():
            if path==root:raise ValueError('Package root itself cannot be an output')
            outputs.append({'name':name,'kind':kind,'relative':str(path.relative_to(root))})
    for row in outputs:
        if Path(row['relative']).parts[0] in ('.package-transactions','.package-reservations'):
            raise ValueError('Package output overlaps transaction control storage')
        if any(Path(other['relative']).is_relative_to(row['relative']) for other in outputs if other!=row):
            raise ValueError('Package transaction outputs must not overlap')
    mapped=dict(mappings,**directories,**files)
    for name,path in mapped.items():
        no_symlinks(path,repo)
        if not path.is_relative_to(root):raise ValueError('Package mapping escapes selected root: '+name)
    inputs=list(inputs)
    copy_tool='/usr/bin/ditto' if sys.platform=='darwin' and directories else None
    for tool in [command[0],sys.executable]+list(tools)+([copy_tool] if copy_tool else []):
        selected=shutil.which(tool)
        if selected is None:raise ValueError('Required package tool is unavailable: '+tool)
        inputs.append(Path(selected).resolve())
    identity,source=input_identity(inputs,values)
    contract={'outputs':outputs,'mappings':{k:str(v.relative_to(root)) for k,v in mapped.items()},'command':command}
    selection=hashlib.sha256(json.dumps(contract,sort_keys=True).encode()).hexdigest()
    control=root/'.package-transactions';no_symlinks(control,repo)
    locks=repo/'tmp/locks';no_symlinks(locks,repo);locks.mkdir(parents=True,exist_ok=True)
    descriptors=[];attempt=None;record=None
    try:
        # Standalone use participates in cleanup exclusion too; an outer Make
        # already owns its shared descriptor, which is forwarded to nested Make.
        global_fd=os.open(locks/'clean.lock',os.O_CREAT|os.O_RDWR|os.O_NOFOLLOW,0o600)
        descriptors.append(global_fd)
        try:fcntl.flock(global_fd,fcntl.LOCK_SH|fcntl.LOCK_NB)
        except BlockingIOError:raise ValueError('Package transaction held by active cleanup')
        control.mkdir(parents=True,exist_ok=True)
        owner_fd=os.open(control/'owner.lock',os.O_CREAT|os.O_RDWR|os.O_NOFOLLOW,0o600)
        descriptors.append(owner_fd)
        for fd,mode in ((global_fd,fcntl.LOCK_SH),(owner_fd,fcntl.LOCK_EX)):
            try:fcntl.flock(fd,mode|fcntl.LOCK_NB)
            except BlockingIOError:raise ValueError('Package transaction held by cleanup or another package assembly')
        for receipt in control.glob('*/receipt.json'):
            no_symlinks(receipt,repo);previous=read_json(receipt,16*1024*1024)
            if not isinstance(previous,dict):raise ValueError('Unclassified package transaction receipt')
            completion=completed_evidence(receipt,previous,repo)
            if previous.get('selection')==selection and completion:
                if previous.get('identity') != identity:
                    raise ValueError('Completed package inputs changed; select a fresh release-controlled root')
                current=output_inventory(root,outputs)
                if input_identity(inputs,values)[0] != identity:
                    raise ValueError('Package inputs changed during reuse readback')
                if current != previous.get('output_inventory'):
                    raise ValueError('Completed package readback changed; retained outputs were not replaced')
                return {'status':'verified_reuse','receipt':str(receipt),'completion_receipt':completion,'identity':identity}
        if plan(repo,root,list(directories.values()),list(files.values())) != planned:
            raise ValueError('Package output admission changed before assembly')
        if not planned['fresh']:
            raise ValueError('Existing or reserved package outputs lack an exact reusable completed transaction')
        attempt=control/str(uuid.uuid4());attempt.mkdir()
        stage=attempt/'stage';stage.mkdir()
        record={'schema':'physics_sim_package_transaction_v1','artifact_class':'local_package_staging',
                'selection':selection,'contract':contract,'identity':identity,'source_identity':source,
                'state':'assembling','output_root':str(root),'published':[],
                'release_authority_granted':False,'authentication_verified':False,
                'wall_cap_s':wall_cap,'combined_log_cap_bytes':log_cap,
                'directory_copy_tool':copy_tool,'identity_pass_limits':INVENTORY_LIMITS,'identity_pass_limit_scope':'aggregate_per_input_or_output_pass',
                'all_external_descendants_verified_terminal':False,'terminal_verification_scope':'Owned command outcome/anchor and local operations only; complete descendants unverified','terminal_processes_verified':False,'assembly_exit_code':None,
                'assembly_stdout':str(attempt/'assembly.stdout'),'assembly_stderr':str(attempt/'assembly.stderr')}
        write_record(attempt/'receipt.json',record)
        # Reservations protect final paths before any publication. No final
        # directory is created until its verified staged counterpart is ready.
        for row in planned['outputs']:
            reservation=Path(row['reservation']);reservation.parent.mkdir(parents=True,exist_ok=True)
            with reservation.open('x') as stream:
                json.dump({'schema':'physics_sim_artifact_owner_v1','artifact_class':'local_package_staging',
                           'attempt_id':attempt.name,'output':row['path'],'state':'transaction_reserved'},stream)
        assigned=[name+'='+str(stage/path.relative_to(root)) for name,path in mapped.items()]
        argv=command+['RELEASE_ROOT='+str(stage),'RELEASE_DIR='+str(stage)]+assigned
        passed=list(descriptors)
        owner_root=os.environ.get('PHYSICS_SIM_BUILD_OWNER_ROOT')
        if owner_root:passed+=list(inherited_descriptors(repo,Path(owner_root)))
        match=re.search(r'--jobserver-(?:auth|fds)=(\d+),(\d+)',os.environ.get('MAKEFLAGS',''))
        if match:
            for value in match.groups():
                fd=int(value)
                try:os.fstat(fd)
                except OSError:continue
                passed.append(fd)
        execution=run_command(argv,repo,attempt,'assembly',tuple(set(passed)),wall_cap,log_cap)
        record.update(execution=execution,assembly_exit_code=execution['exit_code'],terminal_processes_verified=True)
        assembled=output_inventory(stage,outputs)
        if input_identity(inputs,values)[0] != identity:raise ValueError('Package inputs changed during assembly')
        record.update(state='assembled_verified',output_inventory=assembled)
        write_record(attempt/'receipt.json',record)
        publish=attempt/'publish';publish.mkdir()
        for index,row in enumerate(outputs):
            source_path=stage/row['relative'];copy=publish/row['relative'];copy.parent.mkdir(parents=True,exist_ok=True)
            if row['kind']=='directory' and copy_tool:
                execute([copy_tool,'--rsrc','--extattr','--acl',str(source_path),str(copy)],
                        repo,attempt,'publish_copy_'+str(index),tuple(set(passed)),wall_cap,log_cap)
            elif row['kind']=='directory':shutil.copytree(source_path,copy,symlinks=True)
            else:shutil.copy2(source_path,copy)
        if output_inventory(publish,outputs)!=assembled:raise ValueError('Package publication copy mismatch')
        record['state']='publishing';write_record(attempt/'receipt.json',record)
        for row in outputs:
            final=root/row['relative'];final.parent.mkdir(parents=True,exist_ok=True)
            no_symlinks(final,repo)
            rename_exclusive(publish/row['relative'],final)
            record['published'].append(row['relative']);write_record(attempt/'receipt.json',record)
        if output_inventory(root,outputs)!=assembled:raise ValueError('Published package readback mismatch')
        if input_identity(inputs,values)[0] != identity:raise ValueError('Package inputs changed during publication')
        record['state']='completed';write_record(attempt/'receipt.json',record)
        return {'status':'completed','receipt':str(attempt/'receipt.json'),'identity':identity}
    except BaseException as error:
        if attempt is not None and record is not None:
            record['last_state']=record['state'];record['state']='failed_retained';record['failure']=str(error)
            record['terminal_processes_verified']=not isinstance(error,IncompleteTeardown)
            if isinstance(error,ExecutionFailure):record['assembly_exit_code']=error.exit_code
            write_record(attempt/'receipt.json',record)
        raise
    finally:
        for fd in descriptors:os.close(fd)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path)
    parser.add_argument('--recover-attempt',type=Path)
    parser.add_argument('--apply',action='store_true')
    parser.add_argument('--wall-cap',type=int)
    parser.add_argument('--log-cap',type=int)
    for name in ('directory','file','map','identity'):
        parser.add_argument('--'+name,action='append',default=[])
    parser.add_argument('--input',type=Path,action='append',default=[])
    parser.add_argument('--tool',action='append',default=[])
    parser.add_argument('command',nargs=argparse.REMAINDER)
    args=parser.parse_args()
    command=args.command[1:] if args.command[:1]==['--'] else args.command
    if args.recover_attempt:
        if args.root or command or args.wall_cap is not None or args.log_cap is not None or any((args.directory,args.file,args.map,args.input,args.tool,args.identity)):
            parser.error('Recovery uses only the exact retained attempt and optional apply')
    elif not command or args.root is None or args.apply:
        parser.error('Assembly requires root and command; apply is recovery-only')
    try:
        if args.recover_attempt:
            print(json.dumps(recover(Path.cwd(),args.recover_attempt,args.apply)))
        else:
            print(json.dumps(run(Path.cwd(),args.root,parse_assignments(args.directory),parse_assignments(args.file),
                parse_assignments(args.map),args.input,args.identity,command,args.tool,
                wall_cap=1800 if args.wall_cap is None else args.wall_cap,log_cap=67108864 if args.log_cap is None else args.log_cap)))
    except (ValueError,OSError,subprocess.CalledProcessError,RecursionError) as error:
        parser.exit(2,str(error)+'\n')


if __name__=='__main__':main()
