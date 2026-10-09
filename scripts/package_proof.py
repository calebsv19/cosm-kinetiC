"""Run a local package proof in a fresh retained capsule; never prune results."""
import argparse
import fcntl
import json
import os
from pathlib import Path
import re
import shutil
import sys
import uuid

from build_owner import inherited_descriptors
from clean_outputs import no_symlinks
from desktop_replace import inventory, write_record, INVENTORY_LIMITS
from package_outputs import plan
from package_transaction import input_identity
from contract_proof import execute, IncompleteTeardown, ExecutionFailure


def run(repo, root, name, inputs, values, mappings, command, *, wall_cap=900, log_cap=67108864, run_command=execute):
    repo=repo.resolve();root=Path(os.path.abspath(root))
    if not 0<wall_cap<=3600:raise ValueError('Invalid package proof wall cap')
    if type(log_cap) is not int or not 1<=log_cap<=67108864:raise ValueError('Invalid package proof log cap')
    if not re.fullmatch('[a-z0-9][a-z0-9_-]{0,63}',name):raise ValueError('Invalid package proof name')
    # Reuse package namespace admission without reserving or creating a final artifact.
    plan(repo,root,[root/'.package-proofs'],[])
    for variable,relative in mappings.items():
        if not re.fullmatch('[A-Z][A-Z0-9_]*',variable):raise ValueError('Invalid proof variable')
        path=Path(relative)
        if path.is_absolute() or '..' in path.parts:raise ValueError('Proof path escapes capsule')
    inputs=list(inputs)+[Path(__file__).resolve(),Path(sys.executable).resolve(),repo/'makefile',repo/'make',repo/'scripts',repo/'tools/packaging']
    executable=shutil.which(command[0])
    if executable is None:raise ValueError('Proof command is unavailable')
    inputs.append(Path(executable).resolve())
    identity,source=input_identity(inputs,values)
    locks=repo/'tmp/locks';no_symlinks(locks,repo);locks.mkdir(parents=True,exist_ok=True)
    fd=os.open(locks/'clean.lock',os.O_CREAT|os.O_RDWR|os.O_NOFOLLOW,0o600)
    capsule=None;record=None;package_fd=None
    try:
        try:fcntl.flock(fd,fcntl.LOCK_SH|fcntl.LOCK_NB)
        except BlockingIOError:raise ValueError('Package proof held by active cleanup')
        owner=root/'.package-transactions/owner.lock'
        no_symlinks(owner,repo)
        if owner.exists():
            package_fd=os.open(owner,os.O_RDONLY|os.O_NOFOLLOW)
            try:fcntl.flock(package_fd,fcntl.LOCK_SH|fcntl.LOCK_NB)
            except BlockingIOError:raise ValueError('Package proof held by active package assembly/recovery')
        parent=root/'.package-proofs';no_symlinks(parent,repo);parent.mkdir(parents=True,exist_ok=True)
        capsule=parent/(name+'-'+uuid.uuid4().hex);capsule.mkdir()
        work=capsule/'work';work.mkdir()
        record={'schema':'physics_sim_package_proof_v1','artifact_class':'package_proof',
                'name':name,'state':'running','input_identity':identity,'input_inventory':source,
                'command':command,'work':str(work),'release_authority_granted':False,
                'installed_or_published_state_verified':False,'automatic_removal':False,
                'wall_cap_s':wall_cap,'combined_log_cap_bytes':log_cap,'inventory_limits':INVENTORY_LIMITS,'input_limit_scope':'aggregate_per_identity_pass',
                'all_external_descendants_verified_terminal':False,'terminal_verification_scope':'Owned command outcome/anchor and local operations only; complete descendants unverified','terminal_processes_verified':False,'exit_code':None,
                'stdout':str(capsule/'command.stdout'),'stderr':str(capsule/'command.stderr')}
        write_record(capsule/'receipt.json',record)
        assigned=['PACKAGE_PROOF_DIR='+str(work)]+[key+'='+str(work/relative) for key,relative in mappings.items()]
        descriptors=[fd]+([package_fd] if package_fd is not None else [])
        owner=os.environ.get('PHYSICS_SIM_BUILD_OWNER_ROOT')
        if owner:descriptors+=list(inherited_descriptors(repo,Path(owner)))
        match=re.search(r'--jobserver-(?:auth|fds)=(\d+),(\d+)',os.environ.get('MAKEFLAGS',''))
        if match:
            for value in match.groups():
                candidate=int(value)
                try:os.fstat(candidate)
                except OSError:continue
                descriptors.append(candidate)
        execution=run_command(command+assigned,repo,capsule,'command',tuple(set(descriptors)),wall_cap,log_cap)
        record['execution']=execution;record['exit_code']=execution['exit_code']
        record['terminal_processes_verified']=True
        record['work_inventory']=inventory(work)
        if input_identity(inputs,values)[0]!=identity:raise ValueError('Package inputs changed during proof')
        record['state']='passed';write_record(capsule/'receipt.json',record)
        return {'status':'passed','receipt':str(capsule/'receipt.json'),'log':str(capsule/'command.stdout'),'stderr':str(capsule/'command.stderr'),'work':str(work)}
    except BaseException as error:
        if capsule is not None and record is not None:
            record['state']='failed_retained';record['failure']=str(error)
            record['terminal_processes_verified']=not isinstance(error,IncompleteTeardown)
            if isinstance(error,ExecutionFailure):record['exit_code']=error.exit_code
            if record['terminal_processes_verified']:
                try:record['work_inventory']=inventory(work)
                except (ValueError,OSError,RecursionError) as inventory_error:
                    record['work_inventory_error']=str(inventory_error)
            write_record(capsule/'receipt.json',record)
            if isinstance(error,(ValueError,OSError,RecursionError)):
                raise ValueError(str(error)+'; retained '+str(capsule)) from error
        raise
    finally:
        if package_fd is not None:os.close(package_fd)
        os.close(fd)


def main():
    # Recursive Make recipes execute even under -n; planning must have no effects.
    flags=os.environ.get('MAKEFLAGS','').split()
    if flags and not flags[0].startswith('-') and '=' not in flags[0] and 'n' in flags[0]:
        print('Package dry-run: '+ ' '.join(sys.argv[1:]))
        return
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,required=True)
    parser.add_argument('--name',required=True)
    parser.add_argument('--wall-cap',type=int,default=900)
    parser.add_argument('--log-cap',type=int,default=67108864)
    parser.add_argument('--input',type=Path,action='append',default=[])
    parser.add_argument('--identity',action='append',default=[])
    parser.add_argument('--map',action='append',default=[])
    parser.add_argument('command',nargs=argparse.REMAINDER)
    args=parser.parse_args();mappings={}
    for value in args.map:
        key,separator,path=value.partition('=')
        if not separator or key in mappings:parser.error('Map requires a unique VARIABLE=relative-path')
        mappings[key]=path
    command=args.command[1:] if args.command[:1]==['--'] else args.command
    if not command:parser.error('A proof command is required')
    try:print(json.dumps(run(Path.cwd(),args.root,args.name,args.input,args.identity,mappings,command,wall_cap=args.wall_cap,log_cap=args.log_cap)))
    except (ValueError,OSError,RecursionError) as error:parser.exit(2,str(error)+'\n')


if __name__=='__main__':main()
