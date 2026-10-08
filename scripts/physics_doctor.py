"""Read-only local prerequisite and lifecycle preflight; never installs or builds."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import sys

from physics_status import status, read_local_bytes
from cfd_evidence import admitted_path, experiment_root
from build_outputs import verified

SOURCE_BASELINE = (
    "VERSION", "WORKER_VERSION", "makefile", "make/config.mk", "make/rules-build.mk", "make/rules-tools.mk",
    "src/tools/cli/physics_sim_headless.c",
    "third_party/codework_shared/core/core_base/include/core_base.h",
    "third_party/codework_shared/core/core_headless_job/include/core_headless_job.h",
)
from check_clean_root import unique_object, finite_constant, read_json


from tool_probe import probe


def selected_command(value):
    command = shlex.split(value)
    if not command:
        raise ValueError('Empty tool selection')
    executable = shutil.which(command[0])
    if executable is None:
        raise ValueError('Tool unavailable: '+command[0])
    command[0] = str(Path(executable).resolve())
    return command


def layout(repo, roots):
    paths = {name: admitted_path(path) for name, path in roots.items()}
    if not paths['build'].is_relative_to(repo/'build'):
        raise ValueError('Build root must be inside checkout/build')
    if not paths['test'].is_relative_to(repo/'tmp'):
        raise ValueError('Test root must be inside checkout/tmp')
    experiment_root(repo, paths['experiments'])
    protected = [repo/name for name in ('src','include','scripts','tests','docs','make','config','third_party','build','tmp','.git','.agents','.codex','export','dist')]
    if any(paths['tools']==path or paths['tools'].is_relative_to(path) or path.is_relative_to(paths['tools']) for path in protected):
        raise ValueError('Tools root overlaps protected checkout storage')
    for name, path in paths.items():
        if path.exists() and not path.is_dir():
            raise ValueError(name+' root must be a directory')
        for other, target in paths.items():
            if name != other and (path == target or path.is_relative_to(target)):
                raise ValueError('Configured roots overlap: '+name+' and '+other)
    return paths


def pins(repo, amg=True):
    requirements = {}
    for file in (('requirements-cfd-reference.txt','requirements-cfd-reference-amg.txt') if amg else ('requirements-cfd-reference.txt',)):
        for line in read_local_bytes(repo/'scripts'/file, repo, 16384).decode('utf-8').splitlines():
            line = line.strip()
            if not line or line.startswith('#'): continue
            pair = line.split('==')
            if len(pair) != 2 or not all(pair):
                raise ValueError('Reference doctor requires exact distribution pins')
            if pair[0] in requirements and requirements[pair[0]] != pair[1]:
                raise ValueError('Conflicting reference distribution pins')
            requirements[pair[0]] = pair[1]
    return requirements


def reference_environment(repo, interpreter, run_probe, amg=True, inspect_setup=True):
    expected = pins(repo, amg)
    modules = {'scikit-fem':'skfem', 'numpy':'numpy', 'scipy':'scipy', 'pyamg':'pyamg'}
    if not amg: modules.pop('pyamg')
    if set(expected) != set(modules):
        raise ValueError('Reference distribution set requires doctor mapping update')
    # A venv's interpreter leaf is intentionally a system-runtime symlink.
    interpreter = interpreter if interpreter.is_absolute() else repo/interpreter
    interpreter=admitted_path(interpreter.parent)/interpreter.name
    marker = interpreter.parent.parent/'.reference-setup.json'
    if inspect_setup and (marker.exists() or marker.is_symlink()):
        state=read_json(marker,16384)
        if (not isinstance(state,dict) or state.get('schema')!='physics_sim_reference_setup_state_v1'
                or state.get('status')!='passed' or state.get('target')!=str(interpreter.parent.parent)
                or not isinstance(state.get('expected'),dict)
                or any(state['expected'].get(name)!=version for name,version in expected.items())):
            return {'status':'held','path':str(interpreter),'expected':expected,
                    'reason':'Environment setup is incomplete, failed, mismatched or unclassified; retain it and use a fresh profile'}
    if not interpreter.is_file():
        return {'status':'missing', 'path':str(interpreter), 'expected':expected,
                'action':'Create the declared reference environment explicitly; doctor does not install it'}
    code = ("import importlib,importlib.metadata,json; rows={}; "
            "exec(\"for dist,module in "+repr(list(modules.items()))+":\\n try:\\n  imported=importlib.import_module(module); rows[dist]={'version':importlib.metadata.version(dist),'runtime_version':getattr(imported,'__version__',None),'module_path':getattr(imported,'__file__',None),'imported':True}\\n except Exception as error: rows[dist]={'imported':False,'error':str(error)}\"); print(json.dumps(rows))")
    result = run_probe([str(interpreter),'-I','-B','-c',code])
    row = {'path':str(interpreter), 'expected':expected, 'probe':result, 'numerical_qualification_verified':False}
    if result['status'] != 'passed':return {**row,'status':'unverified'}
    try:
        installed=json.loads(result['stdout'],object_pairs_hook=unique_object,parse_constant=finite_constant)
        passed = (isinstance(installed,dict) and set(installed)==set(expected)
            and all(isinstance(installed[name],dict) and installed[name].get('imported') is True
                    and installed[name].get('version')==version and installed[name].get('runtime_version')==version for name,version in expected.items()))
        return {**row,'status':'ready_for_reference_attempt' if passed else 'mismatch', 'installed':installed}
    except (ValueError,TypeError,RecursionError) as error:
        return {**row,'status':'unverified','reason':str(error)}


def doctor(repo, roots, compiler='clang', pkg_config='pkg-config', target_arch='', profile='headless', reference_python=None, run_probe=probe, executables=None, json_compat_prefix=None):
    repo=repo.resolve()
    roots={name: repo/value if not value.is_absolute() else value for name,value in roots.items()}
    report={'schema':'physics_sim_local_doctor_v1','repo':str(repo),'profile':profile,
            'actions_performed':['read_only_layout_inspection'], 'mutations_performed':[], 'build_verified':False,'test_verified':False,
            'installed_or_published_state_verified':False,'physical_qualification_verified':False,
            'checks':{},'next_actions':[]}
    try:
        roots=layout(repo,roots);report['checks']['layout']={'status':'passed','roots':{k:str(v) for k,v in roots.items()},'write_access_verified':False}
    except (OSError,ValueError) as error:
        report['checks']['layout']={'status':'held','reason':str(error)}
        report['status']='attention_required';report['next_actions'].append('Correct root selections before probing or writing outputs')
        return report
    report['actions_performed'].append('read_only_metadata_and_tool_probes')
    sources={}
    for name in SOURCE_BASELINE:
        try:
            path=admitted_path(repo/name)
            sources[name]={'status':'present' if path.is_file() else 'missing'}
        except (OSError,ValueError) as error:sources[name]={'status':'unverified','reason':str(error)}
    report['checks']['source_checkout']={'status':'passed' if all(row['status']=='present' for row in sources.values()) else 'unverified',
        'files':sources,'full_build_graph_verified':False,'scope':'portable source checkout baseline'}
    report['local_status']=status(repo,roots['build'],roots['test'],roots['experiments'],roots['tools'],executables)
    report['checks']['local_metadata']={'status':'passed' if not report['local_status']['diagnostics'] else 'unverified',
                                        'diagnostics':report['local_status']['diagnostics']}
    report['checks']['python']={'status':'passed' if sys.version_info >= (3,11) and hasattr(hashlib,'file_digest') else 'missing',
                               'path':sys.executable,'version':sys.version.split()[0]}
    for name,value in (('compiler',compiler),('make','make'),('pkg_config',pkg_config)):
        try:
            command=selected_command(value);result=run_probe(command+['--version'])
            report['checks'][name]={'selection':command,**result}
        except (OSError,ValueError) as error: report['checks'][name]={'status':'missing','reason':str(error)}
    dependency_env=dict(os.environ)
    if os.uname().sysname=='Darwin':
        helper=repo/'make/desktop_release_target_contract.sh'
        env=dict(dependency_env)
        if target_arch:env['TARGET_ARCH']=target_arch
        target=run_probe(['bash',str(helper),'get','homebrew_prefix'],env=env)
        report['checks']['target_selection']=target
        if target['status']=='passed' and target.get('stdout') in ('/opt/homebrew','/usr/local'):
            prefix=Path(target['stdout']);dependency_env['PKG_CONFIG_LIBDIR']=str(prefix/'lib/pkgconfig')+':'+str(prefix/'share/pkgconfig')
            report['checks']['sdk']=run_probe(['xcrun','--show-sdk-path'])
        else:
            report['status']='attention_required';report['next_actions'].append('Resolve the existing target-selection contract')
            return report
    dependencies={}
    if report['checks']['pkg_config']['status']=='passed':
        for package in ('sdl2','SDL2_ttf','json-c','vulkan'):
            dependencies[package]=run_probe(report['checks']['pkg_config']['selection']+['--modversion',package],env=dependency_env)
        if os.uname().sysname=='Darwin' and dependencies['json-c'].get('stdout')=='0.19':
            compat=Path(json_compat_prefix or os.environ.get('PHYSICS_SIM_JSON_COMPAT_PREFIX',str(prefix/'Cellar/json-c/0.18')))/'lib/libjson-c.a'
            dependencies['json-c']['compatibility_archive']={'path':str(compat),'exists':compat.is_file()}
            if not compat.is_file():dependencies['json-c']['status']='missing_compatible_archive'
    report['checks']['dependencies']={'status':'passed' if len(dependencies)==4 and all(row['status']=='passed' for row in dependencies.values()) else 'unverified',
                                      'packages':dependencies,'pkg_config_libdir':dependency_env.get('PKG_CONFIG_LIBDIR'),
                                      'abi_or_link_qualification_verified':False}
    try:
        report['reference_environment']=reference_environment(repo,reference_python or roots['tools']/'cfd-reference-venv/bin/python',run_probe)
    except (OSError, ValueError, UnicodeError, RecursionError) as error:
        report['reference_environment']={'status':'unverified','reason':str(error),'numerical_qualification_verified':False}
    report['output_ownership']={}
    for name,row in report['local_status']['executables'].items():
        if not row['exists']:continue
        try: report['output_ownership'][name]={'status':'verified_disposable_bytes',**verified(repo,Path(row['path'])),'profile_identity_verified':False}
        except (OSError,ValueError,RecursionError) as error:report['output_ownership'][name]={'status':'unverified','reason':str(error),'profile_identity_verified':False}
    passed=all(row['status']=='passed' for row in report['checks'].values())
    if profile=='cfd-reference':passed=passed and report['reference_environment']['status']=='ready_for_reference_attempt'
    if report['local_status']['cleanup']['status']=='held':
        passed=False;report['next_actions'].append('Use a fresh isolated build root; preserve the held original contents')
    report['status']='ready_for_build_attempt' if passed else 'attention_required'
    if not passed:report['next_actions'].append('Resolve the failed or unverified prerequisite checks; then run the supported build and fixtures')
    if not report['local_status']['backup_covers_all_current_evidence']:
        report['next_actions'].append('Retained evidence has incomplete independent backup coverage; reconcile exact coverage before pruning')
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile',choices=('headless','cfd-reference'),default='headless')
    parser.add_argument('--compiler',default='clang');parser.add_argument('--pkg-config',default='pkg-config')
    parser.add_argument('--json-compat-prefix')
    parser.add_argument('--executable',type=Path,action='append')
    parser.add_argument('--target-arch',default='');parser.add_argument('--reference-python',type=Path)
    defaults={'build':'build','test':'tmp/tests','experiments':'data/experiments','tools':'data/tools'}
    for name,value in defaults.items():parser.add_argument('--'+name+'-root',type=Path,default=Path(value))
    args=parser.parse_args()
    try:
        row=doctor(Path.cwd(),{name:getattr(args,name+'_root') for name in defaults},args.compiler,args.pkg_config,args.target_arch,args.profile,args.reference_python,executables=args.executable,json_compat_prefix=args.json_compat_prefix)
    except (OSError,ValueError,RecursionError) as error:
        parser.exit(2,'Doctor held: '+str(error)+'\n')
    print(json.dumps(row,indent=2));raise SystemExit(0 if row['status']=='ready_for_build_attempt' else 2)


if __name__=='__main__':main()
