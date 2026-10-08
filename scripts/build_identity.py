"""Refresh a disposable build configuration stamp only when inputs change."""
import argparse
import hashlib
import json
import os
import re
from pathlib import Path
import shlex
import stat
import shutil
import subprocess
import tempfile

from clean_outputs import no_symlinks
from build_outputs import record, verified, fingerprint, checked_path, receipt_path, dependency_syntax, dependency_input_snapshot, link_input_snapshot, LinkInputsChanged, validate_link_selection
from check_clean_root import read_json, build_witness
from tool_probe import probe

KEYS = ('RUNTIME_SCENE_EMITTER_DIAG_TOOL_CFLAGS', 'VF2D_PACK_TOOL_CFLAGS', 'VF2D_DATASET_TOOL_CFLAGS', 'PHYSICS_TRACE_TOOL_CFLAGS', 'CC', 'CLANG', 'CFLAGS', 'LDFLAGS', 'LIBS', 'CSTD', 'TARGET_ARCH',
        'FISICS', 'FISICS_FLAGS', 'FISICS_CFLAGS', 'FISICS_COMPILE_FLAGS',
        'FISICS_MEMCHECK_LINK_LIBS', 'CFD_BUILD_OPT', 'HEADLESS_WORKER_LIBS',
        'TARGET', 'CLANG_TARGET', 'FISICS_TARGET', 'SESSION_WORKER_BIN',
        'PHYSICS_SIM_HEADLESS_TOOL_BIN', 'PHYSICS_SIM_JOB_RUNNER_TOOL_BIN',
        'VF2D_PACK_TOOL_BIN', 'VF2D_DATASET_TOOL_BIN', 'PHYSICS_TRACE_TOOL_BIN',
        'RUNTIME_SCENE_EMITTER_DIAG_TOOL_BIN', 'SHAPE_SANITY_TOOL_BIN',
        'SHAPE_MASK_TOOL_BIN', 'SHAPE_ASSET_TOOL_BIN',
        'PASSIVE3D_WORKER', 'ATMOSPHERE3D_WORKER', 'OPEN_ATMOSPHERE3D_WORKER',
        'JSON_CFLAGS', 'JSON_LIBS')


# Implicit include/link/toolchain settings used outside explicit Make flags.
# Absent and present-empty values remain distinct, and path order is preserved.
COMPILER_ENVIRONMENT_KEYS = (
    'CPATH', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'OBJC_INCLUDE_PATH',
    'LIBRARY_PATH', 'GCC_EXEC_PREFIX', 'COMPILER_PATH', 'SDKROOT',
    'DEVELOPER_DIR', 'TOOLCHAINS', 'MACOSX_DEPLOYMENT_TARGET',
    'IPHONEOS_DEPLOYMENT_TARGET', 'TVOS_DEPLOYMENT_TARGET',
    'WATCHOS_DEPLOYMENT_TARGET', 'XROS_DEPLOYMENT_TARGET',
    'CCC_OVERRIDE_OPTIONS', 'CCC_ADD_ARGS', 'CLANG_CONFIG_FILE_SYSTEM_DIR',
    'CLANG_CONFIG_FILE_USER_DIR', 'SOURCE_DATE_EPOCH', 'ZERO_AR_DATE',
    'LANG', 'LC_ALL', 'LC_CTYPE', 'LC_MESSAGES',
)


def compiler_environment():
    values={key:os.environ.get(key) for key in COMPILER_ENVIRONMENT_KEYS}
    if any(value is not None and len(value.encode())>65536 for value in values.values()):
        raise ValueError('Compiler environment value byte bound exceeded')
    if sum(len(value.encode()) for value in values.values() if value is not None)>1024*1024:
        raise ValueError('Compiler environment aggregate byte bound exceeded')
    return values


def digest(path):
    return fingerprint(path)['sha256']


def checked_probe(command, timeout=15, limit=65536):
    result = probe(command, timeout=timeout, limit=limit)
    if result['status'] != 'passed':
        raise ValueError('Cannot establish tool identity: '+str(command)+': '+result.get('reason', result.get('stderr', 'nonzero exit')))
    return result


def compiler(value):
    command = shlex.split(value)
    if not command:
        return None
    selected = shutil.which(command[0])
    if selected is None:
        return {'command': command, 'available': False}
    executable = Path(selected).resolve()
    before = fingerprint(executable)
    row = executable.stat()
    # Version output and resolved file identity bind tool selection. No shell.
    result = checked_probe([str(executable), *command[1:], '--version'])
    if fingerprint(executable) != before:
        raise ValueError('Compiler changed during identity probe: '+str(executable))
    return {'command': command, 'path': str(executable), 'bytes': row.st_size,
            'mtime_ns': row.st_mtime_ns, 'sha256': before['sha256'], 'version': result['stdout']}


def predecessor(repo, path):
    no_symlinks(path, repo)
    return verified(repo, path) if path.exists() else None


def active_selection(repo, path):
    previous = predecessor(repo, path)
    if previous is None:
        if path.parent.exists() and any(path.parent.iterdir()):
            raise ValueError('Missing active selection beside retained configuration state; use a fresh build root')
        return {}, None
    row = read_json(path, 16384)
    if (not isinstance(row, dict) or set(row) != {'digest', 'generation'}
            or not isinstance(row['digest'], str) or not re.fullmatch('[0-9a-f]{64}', row['digest'])
            or type(row['generation']) is not int or not 1 <= row['generation'] <= 1000000000):
        raise ValueError('Invalid build configuration selection: '+str(path))
    if predecessor(repo, path) != previous:
        raise ValueError('Build configuration changed during inspection')
    return row, previous


def revalidate(repo, predecessors):
    for path, previous in predecessors.items():
        if predecessor(repo, path) != previous:
            raise ValueError('Build configuration predecessor changed: '+str(path))


def publish(repo, path, content):
    descriptor, temporary = tempfile.mkstemp(prefix='.config-', dir=path.parent)
    try:
        with os.fdopen(descriptor, 'w') as stream:
            stream.write(content); stream.flush(); os.fsync(stream.fileno())
        os.replace(temporary, path)
        record(repo, path, 'configuration')
    finally:
        Path(temporary).unlink(missing_ok=True)


def admit_dependencies(repo,root,paths):
    if len(paths)>10000:raise ValueError('Generated dependency count bound exceeded')
    admitted=[];total=0
    for raw in paths:
        path=checked_path(raw,repo)
        if not path.is_relative_to(root) or path.suffix!='.d':
            raise ValueError('Generated dependency namespace mismatch')
        try:info=path.lstat()
        except FileNotFoundError:
            if path.with_suffix('.o').exists():raise ValueError('Missing generated dependency beside object')
            continue
        if not stat.S_ISREG(info.st_mode) or info.st_nlink!=1 or info.st_size>1024*1024:
            raise ValueError('Unclassified generated dependency')
        total+=info.st_size
        if total>64*1024*1024:raise ValueError('Generated dependency aggregate byte bound exceeded')
        admitted.append((path,info.st_size))
    witnesses={};rebuild=[];input_budget={'entries':0,'hash_bytes':0}
    for path,size in admitted:
        receipt=receipt_path(repo,path);receipt_before=build_witness(receipt.lstat())
        before=verified(repo,path)
        ownership=read_json(receipt,16384)
        if ownership.get('producer')!='dependency':
            raise ValueError('Generated dependency producer mismatch')
        fd=os.open(path,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
        try:
            info=os.fstat(fd)
            if not stat.S_ISREG(info.st_mode) or info.st_nlink!=1 or info.st_size!=size:
                raise ValueError('Generated dependency changed before read')
            with os.fdopen(fd,'rb',closefd=False) as stream:data=stream.read(size+1)
            if len(data)!=size:raise ValueError('Generated dependency changed during read')
        finally:os.close(fd)
        text=data.decode('utf-8');dependency_syntax(repo,path,text)
        object_path=path.with_suffix('.o')
        if object_path.exists():
            verified(repo,object_path)
            object_owner=read_json(receipt_path(repo,object_path),16384)
            saved=ownership.get('input_snapshot')
            current=dependency_input_snapshot(repo,path,text,input_budget)
            current['scope']='compiler_boundary_observation'
            if saved is not None and (not isinstance(saved,dict) or set(saved)!=set(current)
                    or saved.get('schema')!=current['schema'] or saved.get('scope') not in ('postcompiler_observation','compiler_boundary_observation')
                    or type(saved.get('count')) is not int or not 1<=saved['count']<=10000
                    or not isinstance(saved.get('sha256'),str) or not re.fullmatch('[0-9a-f]{64}',saved['sha256'])):
                raise ValueError('Invalid generated dependency input snapshot')
            if saved!=current or object_owner.get('dependency_sha256')!=before['sha256']:
                name=str(object_path.relative_to(repo))
                if re.search(r'[\s$;|%=:*?\\#]',name):raise ValueError('Unsupported dependency rebuild target')
                rebuild.append(name)
        if verified(repo,path)!=before:raise ValueError('Generated dependency changed during admission')
        if build_witness(receipt.lstat())!=receipt_before:raise ValueError('Generated dependency receipt changed during admission')
        witnesses[path]=(before,receipt,receipt_before)
    for path,(before,receipt,receipt_before) in witnesses.items():
        if verified(repo,path)!=before or build_witness(receipt.lstat())!=receipt_before:
            raise ValueError('Generated dependency changed during complete admission')
    return sorted(set(rebuild))


def admit_links(repo, root, outputs):
    if len(outputs)>1000: raise ValueError('Link output count bound exceeded')
    rebuilt=[]; witnesses=[]; receipt_witnesses=[]; budget={'entries':0,'hash_bytes':0}; metadata=0
    for raw in sorted(set(outputs)):
        output=checked_path(raw,repo)
        if not output.is_relative_to(root): raise ValueError('Link output namespace mismatch')
        manifest=output.with_name(output.name+'.link-inputs.json')
        owner=None; original=None
        if output.exists():
            original=verified(repo,output)
            output_receipt=receipt_path(repo,output)
            receipt_witnesses.append((output_receipt,build_witness(output_receipt.lstat())))
            owner=read_json(output_receipt,16384)
            if owner.get('producer')!='compiler': raise ValueError('Link output producer mismatch')
        if not manifest.exists():
            if owner is not None:
                if owner.get('link_inputs_sha256') is not None: raise ValueError('Missing bound linker manifest')
                rebuilt.append(output)
            continue
        info=manifest.lstat(); metadata+=info.st_size
        if not stat.S_ISREG(info.st_mode) or info.st_nlink!=1 or info.st_size>1024*1024 or metadata>64*1024*1024:
            raise ValueError('Link manifest metadata bound exceeded')
        manifest_before=verified(repo,manifest)
        receipt=receipt_path(repo,manifest); receipt_before=build_witness(receipt.lstat())
        receipt_witnesses.append((receipt,receipt_before))
        if read_json(receipt,16384).get('producer')!='linker-inputs': raise ValueError('Link manifest producer mismatch')
        row=read_json(manifest,1024*1024)
        if not isinstance(row,dict) or set(row)!= {'schema','selection','snapshot'} or row['schema']!='physics_sim_link_manifest_v1':
            raise ValueError('Invalid link manifest schema')
        validate_link_selection(row['selection'])
        saved=row['snapshot']
        if (not isinstance(saved,dict) or set(saved)!= {'schema','scope','selected_count','absent_count','sha256'}
                or saved['schema']!='physics_sim_link_inputs_v1' or saved['scope']!='linker_boundary_observation'
                or type(saved['selected_count']) is not int or not 1<=saved['selected_count']<=10000
                or type(saved['absent_count']) is not int or not 0<=saved['absent_count']<=10000
                or not isinstance(saved['sha256'],str) or not re.fullmatch('[0-9a-f]{64}',saved['sha256'])):
            raise ValueError('Invalid link input snapshot')
        if saved['selected_count']!=len(row['selection']['selected']) or saved['absent_count']!=len(row['selection']['absent']):
            raise ValueError('Link input snapshot count mismatch')
        if owner is not None:
            if owner.get('link_inputs_sha256')!=manifest_before['sha256']:
                raise ValueError('Executable and linker manifest binding mismatch')
            try: current=link_input_snapshot(row['selection'],budget)
            except LinkInputsChanged: current=None
            if current!=saved: rebuilt.append(output)
            witnesses.append((output,original))
        witnesses.append((manifest,manifest_before))
        if build_witness(receipt.lstat())!=receipt_before: raise ValueError('Link manifest receipt changed')
    for path,before in witnesses:
        if verified(repo,path)!=before: raise ValueError('Link inputs metadata changed during admission')
    for path,before in receipt_witnesses:
        if build_witness(path.lstat())!=before: raise ValueError('Link ownership receipt changed during admission')
    names=[]
    for output in rebuilt:
        name=str(output.relative_to(repo))
        if re.search(r'[\s$;|%=:*?\\#]',name): raise ValueError('Unsupported linker rebuild target')
        names.append(name)
    return sorted(set(names))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--digest-only', action='store_true')
    parser.add_argument('--expected-digest')
    parser.add_argument('--selection-root', type=Path)
    parser.add_argument('--link-output', type=Path, action='append', default=[])
    parser.add_argument('--dependency', type=Path, action='append', default=[])
    parser.add_argument('--makefile', type=Path, action='append', default=[])
    args = parser.parse_args()
    values = {key: os.environ.get('PHYSICS_BUILD_'+key, '').strip() for key in KEYS}
    repo = Path.cwd().resolve()
    if args.selection_root is None:
        parser.error('--selection-root is required')
    if args.selection_root:
        repo = Path.cwd().resolve()
        root = Path(os.path.abspath(args.selection_root))
        no_symlinks(root, repo)
        if not root.is_relative_to(repo/'build'):
            raise ValueError('Compilation output must be inside checkout/build')
        for key in ('TARGET', 'CLANG_TARGET', 'FISICS_TARGET', 'SESSION_WORKER_BIN',
                    'PHYSICS_SIM_HEADLESS_TOOL_BIN', 'PHYSICS_SIM_JOB_RUNNER_TOOL_BIN',
                    'VF2D_PACK_TOOL_BIN', 'VF2D_DATASET_TOOL_BIN', 'PHYSICS_TRACE_TOOL_BIN',
                    'RUNTIME_SCENE_EMITTER_DIAG_TOOL_BIN', 'SHAPE_SANITY_TOOL_BIN',
                    'SHAPE_MASK_TOOL_BIN', 'SHAPE_ASSET_TOOL_BIN',
                    'PASSIVE3D_WORKER', 'ATMOSPHERE3D_WORKER', 'OPEN_ATMOSPHERE3D_WORKER'):
            if not values[key]:
                continue
            output = Path(os.path.abspath(values[key]))
            no_symlinks(output, repo)
            if not output.is_relative_to(root):
                raise ValueError('Undeclared compilation output '+key+': '+str(output))
    rebuild=admit_dependencies(repo,root,args.dependency)
    rebuild+=admit_links(repo,root,args.link_output)
    active_path = root/'.configuration/active.json'
    active, active_previous = active_selection(repo, active_path)
    tools = {key: compiler(values[key]) for key in ('CC', 'CLANG')}
    # fisiCs is optional until that profile is requested; file identity still binds it.
    fisics = Path(values['FISICS'])
    if values['FISICS'] and fisics.is_file():
        tools['FISICS'] = {'path': str(fisics.resolve()), 'sha256': digest(fisics)}
    config = {'schema': 'physics_sim_build_configuration_v1', 'configuration': values,
              'compiler_environment': compiler_environment(),
              'tools': tools, 'makefiles': {str(p): digest(p) for p in args.makefile},
              'control_tools': {str(p): digest(p) for p in (
                  Path('scripts/build_identity.py'), Path('scripts/build_owner.py'), Path('scripts/atomic_output.py'), Path('scripts/tool_probe.py'),
                  Path('scripts/clean_outputs.py'), Path('scripts/check_clean_root.py'), Path('scripts/build_outputs.py')) if p.is_file()}}
    if os.uname().sysname == 'Darwin':
        selected = shutil.which('xcrun')
        if selected is None:
            raise ValueError('Cannot establish SDK tool identity: xcrun unavailable')
        xcrun = Path(selected).resolve()
        before = fingerprint(xcrun)
        for label, arguments in (('sdk', ['--show-sdk-path']), ('clang', ['--find', 'clang'])):
            result = checked_probe([str(xcrun), *arguments])
            config[label] = result['stdout']
        if fingerprint(xcrun) != before:
            raise ValueError('SDK tool changed during identity probes')
        config['sdk_tool'] = {'path': str(xcrun), 'sha256': before['sha256']}
    content = json.dumps(config, indent=2, sort_keys=True)+'\n'
    configuration_digest = hashlib.sha256(content.encode()).hexdigest()
    generation = active.get('generation', 0) + (active.get('digest') != configuration_digest)
    if generation > 1000000000:
        raise ValueError('Build configuration generation exhausted; use a fresh build root')
    selection = configuration_digest+'-'+str(generation)
    selected_output = root/'.configuration'/(selection+'.json')
    selected_previous = predecessor(repo, selected_output)
    if selected_previous is not None and selected_output.read_text() != content:
        raise ValueError('Existing configuration content does not match selected identity')
    if args.digest_only:
        revalidate(repo, {active_path: active_previous, selected_output: selected_previous})
        print(' '.join([selection,*rebuild]))
        return
    if args.output is None:
        parser.error('--output is required unless --digest-only')
    if args.expected_digest and selection != args.expected_digest:
        raise ValueError('Build configuration changed after graph selection')
    output = Path(os.path.abspath(args.output))
    no_symlinks(output, repo)
    if output != root/'.configuration'/(selection+'.json'):
        raise ValueError('Configuration output must be the exact selected identity path')
    output_previous = selected_previous
    revalidate(repo, {output: output_previous, active_path: active_previous})
    output.parent.mkdir(parents=True, exist_ok=True)
    if output_previous is None:
        publish(repo, output, content)
    active_content = json.dumps({'digest': configuration_digest, 'generation': generation})+'\n'
    if active != {'digest': configuration_digest, 'generation': generation}:
        # Stamp publication does not authorize replacing a concurrently changed selection.
        revalidate(repo, {active_path: active_previous})
        publish(repo, active_path, active_content)


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, subprocess.SubprocessError, RecursionError) as error:
        raise SystemExit('Build configuration held: '+str(error))
