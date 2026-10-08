"""Bounded read-only first-party writer discovery; static signals are not acceptance."""
import argparse
import ast
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import time
from cfd_evidence import admitted_path
from physics_status import read_local_bytes

ROOTS=('scripts','tests','src','include','make','tools')
ROOT_FILES=('makefile','Makefile','rm_frames')
EXTENSIONS={'.py':'python','.c':'native','.h':'native','.sh':'shell','.mk':'make'}
WRITE={'write_text','write_bytes','write','writelines','dump','save','savez','savez_compressed','savetxt','tofile','copy','copy2','copytree','move','replace','rename','mkdir','makedirs','link','symlink','mkstemp','mkdtemp'}
DELETE={'unlink','remove','rmdir','rmtree'}
LAUNCH={'run','Popen','call','check_call','check_output','execv','execve','system','spawn'}
ADAPTERS={'reference_ownership','worker_execution','owned_worker','fixture_session','publish_reference_receipt','publish_factor_record','transaction','atomic_output','publish','owned_command','execute','acquire','operation_guard'}


def dotted(node):
    if isinstance(node,ast.Name):return node.id
    if isinstance(node,ast.Attribute):return dotted(node.value)+'.'+node.attr
    return '<expression>'


def python_signals(text):
    tree=ast.parse(text);signals=[];adapters=set();lines=text.splitlines()
    for node in ast.walk(tree):
        if not isinstance(node,ast.Call):continue
        name=dotted(node.func);leaf=name.rsplit('.',1)[-1]
        if leaf in ADAPTERS:adapters.add(name)
        kind=None
        if leaf in DELETE:kind='delete'
        elif leaf in WRITE:kind='write_or_publish'
        elif leaf in LAUNCH:kind='launch_possible_writer'
        elif leaf in ('open','fdopen','FileType'):
            mode=next((k.value for k in node.keywords if k.arg=='mode'),node.args[1] if len(node.args)>1 else None)
            if leaf=='FileType':mode=node.args[0] if node.args else None
            if isinstance(mode,ast.Constant) and isinstance(mode.value,str):
                if any(x in mode.value for x in ('w','a','x','+')):kind='write_open'
            elif mode is not None:kind='dynamic_open_mode'
            elif name in ('os.open','os.fdopen'):kind='dynamic_open_mode'
        if kind:signals.append({'line':node.lineno,'operation':name,'kind':kind,'basis':'python_ast','excerpt':lines[node.lineno-1].strip()[:200]})
    return signals,sorted(adapters)


def native_arguments(snippet):
    start=snippet.find('(');depth=1;quoted=None;escaped=False;args=[];begin=start+1
    for index in range(start+1,len(snippet)):
        char=snippet[index]
        if quoted:
            if escaped:escaped=False
            elif char=='\\':escaped=True
            elif char==quoted:quoted=None
        elif char in ('"',"'"):quoted=char
        elif char=='(':depth+=1
        elif char==')':
            depth-=1
            if depth==0:return args+[snippet[begin:index].strip()]
        elif char==',' and depth==1:args.append(snippet[begin:index].strip());begin=index+1
    return None


def lexical_signals(text,language):
    signals=[];edges=[];target=None
    if language=='native':
        # Comments are blanked to retain original one-based line numbers. Calls
        # in strings/declarations can still be false positives and remain review.
        code=re.sub(r'/\*.*?\*/|//[^\n]*',lambda m:''.join('\n' if c=='\n' else ' ' for c in m.group()),text,flags=re.S)
        pattern=r'\b(fopen|freopen|open|creat|mkdir|rename|remove|unlink|rmdir|fwrite|fprintf|fputs|write|[A-Za-z_][A-Za-z_0-9]*(?:_write|_save))\s*\('
        for match in re.finditer(pattern,code):
            name=match.group(1);snippet=code[match.start():match.start()+400]
            args=native_arguments(snippet)
            if name in ('fopen','freopen') and args and len(args)>1 and args[1] in ('"r"','"rb"','"rt"'):continue
            kind='delete' if name in ('remove','unlink','rmdir') else 'native_possible_write'
            signals.append({'line':code.count('\n',0,match.start())+1,'operation':name,'kind':kind,'basis':'native_lexical','excerpt':snippet.split(';',1)[0][:200]})
        return signals,edges
    for number,line in enumerate(text.splitlines(),1):
        if not line.strip() or line.lstrip().startswith('#'):continue
        if language=='make':
            if not line.startswith('\t'):
                match=re.match(r'^([^:#=]+):(?![=])',line)
                if match and not match.group(1).startswith('.'):target=match.group(1).strip()
                binding=re.match(r'^([A-Za-z_][A-Za-z_0-9]*)\s*[:?+]?=',line)
                if binding:
                    for reference in re.finditer(r'(?:scripts|tests|tools)/[A-Za-z0-9_./-]+',line):
                        edges.append({'line':number,'target':None,'script':reference.group(0),'variable':binding.group(1)})
                continue
        operations=[]
        if re.search(r'(^|[ ;&|])(?:rm|rmdir)(?:\s|$)',line):operations.append(('delete','shell_remove'))
        if re.search(r'(^|[ ;&|])(?:cp|mv|mkdir|install|tee|touch)(?:\s|$)',line):operations.append(('write_or_publish','shell_file_command'))
        if re.search(r'(?<![<])(?:>>?|&>)',line):operations.append(('possible_redirect_write','redirect'))
        if re.search(r'(?:\$\((?:CC|CLANG|CXX)\)|\b(?:clang|gcc|cc|ar)\b)',line):operations.append(('build_possible_writer','compiler_recipe'))
        for kind,name in operations:signals.append({'line':number,'operation':name,'kind':kind,'basis':language+'_lexical','target':target,'excerpt':line.strip()[:200]})
        for match in re.finditer(r'(?:scripts|tests|tools)/[A-Za-z0-9_./-]+',line):edges.append({'line':number,'target':target,'script':match.group(0)})
    return signals,edges


def inventory(repo,*,file_limit=4194304,total_limit=134217728,entry_limit=20000,wall_cap=120):
    repo=admitted_path(repo)
    if not repo.is_dir():raise ValueError('Inventory checkout missing')
    if any(type(n) is not int or n<=0 for n in (file_limit,total_limit,entry_limit)) or not 0<wall_cap<=120:raise ValueError('Inventory bounds invalid')
    pending=[(repo/name,0) for name in ROOTS if (repo/name).exists() or (repo/name).is_symlink()]
    pending+=[(repo/name,0) for name in ROOT_FILES if (repo/name).exists() or (repo/name).is_symlink()]
    files={};gaps=[];count=0;bytes_read=0;started=time.monotonic()
    while pending:
        path,depth=pending.pop();count+=1;relative=str(path.relative_to(repo))
        if count>entry_limit or time.monotonic()-started>wall_cap:
            gaps.append({'path':relative,'reason':'aggregate entry/time bound; remaining paths uninspected'});break
        try:
            info=path.lstat()
            if stat.S_ISLNK(info.st_mode):raise ValueError('linked source entry refused')
            if stat.S_ISDIR(info.st_mode):
                if depth>=16:raise ValueError('source depth bound')
                children=[]
                with os.scandir(path) as entries:
                    for entry in entries:
                        if len(children)+len(pending)+count>=entry_limit:raise ValueError('source entry allocation bound')
                        children.append(entry)
                children.sort(key=lambda e:e.name)
                pending.extend((Path(e.path),depth+1) for e in reversed(children));continue
            language='make' if path.name in ('makefile','Makefile') else EXTENSIONS.get(path.suffix)
            if language is None and not path.suffix:
                # Extensionless launchers are source; do not decode or execute
                # arbitrary binaries. Header probing is bounded and read-only.
                if not stat.S_ISREG(info.st_mode):raise ValueError('extensionless source candidate not regular')
                fd=os.open(path,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
                try:
                    probe=os.fstat(fd)
                    if (probe.st_dev,probe.st_ino,probe.st_size,probe.st_mtime_ns,probe.st_ctime_ns)!=(info.st_dev,info.st_ino,info.st_size,info.st_mtime_ns,info.st_ctime_ns):raise ValueError('source changed before header read')
                    header=os.read(fd,256).split(b'\n',1)[0]
                    after_probe=os.fstat(fd)
                    if (probe.st_dev,probe.st_ino,probe.st_size,probe.st_mtime_ns,probe.st_ctime_ns)!=(after_probe.st_dev,after_probe.st_ino,after_probe.st_size,after_probe.st_mtime_ns,after_probe.st_ctime_ns):raise ValueError('source changed during header read')
                finally:os.close(fd)
                if header.startswith(b'#!'):
                    if re.search(rb'(?:/|\s)(?:ba|da|k|z)?sh(?:\s|$)',header):language='shell'
                    elif re.search(rb'(?:/|\s)python(?:[0-9.]*)(?:\s|$)',header):language='python'
                    else:raise ValueError('unsupported extensionless source interpreter')
            if language is None:continue
            if not stat.S_ISREG(info.st_mode):raise ValueError('source input not regular')
            if info.st_size>file_limit or bytes_read+info.st_size>total_limit:raise ValueError('source byte bound')
            data=read_local_bytes(path,repo,min(file_limit,total_limit-bytes_read));bytes_read+=len(data)
            after=path.lstat();identity=lambda v:(v.st_dev,v.st_ino,v.st_size,v.st_mtime_ns,v.st_ctime_ns)
            if identity(info)!=identity(after):raise ValueError('source changed during read')
            text=data.decode('utf-8')
            if language=='python':signals,adapters=python_signals(text);edges=[]
            else:signals,edges=lexical_signals(text,language);adapters=[]
            files[relative]={'language':language,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'signals':signals,'explicit_script_edges':edges,'observed_adapter_calls':adapters,'behavior_coverage_verified':False}
        except (OSError,ValueError,SyntaxError,UnicodeError,RecursionError) as error:gaps.append({'path':relative,'reason':str(error)[:400]})
    by_language={}
    for row in files.values():by_language[row['language']]=by_language.get(row['language'],0)+1
    return {'schema':'physics_sim_writer_discovery_v1','repo':str(repo),'scope_roots':[*ROOTS,*ROOT_FILES],'files':files,'gaps':gaps,'summary':{'files':len(files),'source_bytes':bytes_read,'by_language':by_language,'signal_count':sum(len(x['signals']) for x in files.values()),'files_with_signals':sum(bool(x['signals']) for x in files.values()),'explicit_script_edges':sum(len(x['explicit_script_edges']) for x in files.values())},'discovery_complete_for_selected_scan':not gaps,'behavior_coverage_verified':False,'deletion_authorized':False,'limitations':['Static signals require owner/root/overwrite/failure review.','Aliases, wrappers, macros, dynamically resolved Make/shell commands and external libraries can hide writers.','Tests, headers, declarations and strings may contain false positives.','Build/generated/third-party/external source trees are excluded; root files are explicitly selected.', 'Extensionless source uses a bounded 256-byte shell/Python shebang probe; other interpreters are explicit gaps.']}


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--repo',type=Path,default=Path.cwd());args=parser.parse_args()
    try:result=inventory(args.repo)
    except (OSError,ValueError) as error:print(json.dumps({'status':'held','reason':str(error)}));return 2
    print(json.dumps(result,indent=2,sort_keys=True));return 0 if result['discovery_complete_for_selected_scan'] else 2

if __name__=='__main__':raise SystemExit(main())
