"""Read-only digest-bound cache recovery plan and explicit retained rollback."""
import argparse
from contextlib import contextmanager
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import time
from cfd_evidence import admitted_path, file_identity
from cfd_run_support import admitted_json, factor_digest


def path(value):
    raw=Path(value)
    if '..' in raw.parts or any(x in raw.parts for x in ('.git','.ssh','.aws')):raise ValueError('Cache recovery path syntax')
    p=admitted_path(raw)
    if len(str(p).encode())>=1024:raise ValueError('Cache recovery path bound')
    for parent in p.parents:
        if parent.exists() and not parent.is_dir():raise ValueError('Cache recovery ancestor kind')
    return p


def regular(p,cap):
    p=path(p);st=p.lstat()
    if not stat.S_ISREG(st.st_mode) or st.st_nlink!=1 or not 0<=st.st_size<=cap:raise ValueError('Cache recovery regular input bound')
    return st


def read(p,cap):
    before=regular(p,cap);fd=os.open(p,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
    try:
        if file_identity(os.fstat(fd))!=file_identity(before):raise ValueError('Cache recovery input changed')
        data=os.read(fd,cap+1)
        if len(data)!=before.st_size or file_identity(os.fstat(fd))!=file_identity(before) or file_identity(p.lstat())!=file_identity(before):raise ValueError('Cache recovery input changed')
        return data
    finally:os.close(fd)


@contextmanager
def owned(project,exclusive=False):
    project=path(project)
    if not project.is_dir() or project==Path('/') or any(project==Path(x) or project.is_relative_to(x) for x in ('/System','/usr','/bin','/sbin','/private/etc','/Library','/Applications','/dev','/proc','/sys')):raise ValueError('Cache recovery protected project')
    lock=project/'physics_sim/.cache-publication.lock';before=regular(lock,0)
    fd=os.open(lock,os.O_RDWR|os.O_NOFOLLOW|os.O_NONBLOCK)
    try:
        if file_identity(os.fstat(fd))!=file_identity(before):raise ValueError('Cache owner changed')
        try:fcntl.flock(fd,(fcntl.LOCK_EX if exclusive else fcntl.LOCK_SH)|fcntl.LOCK_NB)
        except BlockingIOError:raise ValueError('Cache publisher/reader is active')
        yield project
        if file_identity(lock.lstat())!=file_identity(before):raise ValueError('Cache owner changed')
    finally:os.close(fd)


def observe(p,budget):
    p=path(p)
    try:root=p.lstat()
    except FileNotFoundError:return None
    rows={};stack=[(p,0)];witnesses=[]
    while stack:
        selected,depth=stack.pop();selected=path(selected);info=selected.lstat()
        budget['entries']+=1
        if budget['entries']>10000 or depth>16 or time.monotonic()-budget['started']>120:raise ValueError('Cache recovery observation bound')
        witnesses.append((selected,file_identity(info)));name=str(selected.relative_to(p))
        if stat.S_ISDIR(info.st_mode):
            rows[name]={'kind':'directory','identity':file_identity(info)}
            fd=os.open(selected,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
            try:
                if file_identity(os.fstat(fd))!=file_identity(info):raise ValueError('Cache directory changed')
                with os.scandir(fd) as children:
                    for child in children:
                        if len(stack)+budget['entries']>=10000:raise ValueError('Cache recovery enumeration bound')
                        stack.append((selected/child.name,depth+1))
            finally:os.close(fd)
        elif stat.S_ISREG(info.st_mode) and info.st_nlink==1 and 0<=info.st_size<=8*1024**3:
            budget['bytes']+=info.st_size
            if budget['bytes']>32*1024**3:raise ValueError('Cache recovery aggregate byte bound')
            rows[name]={'kind':'file','identity':file_identity(info),'bytes':info.st_size,'sha256':factor_digest(selected,min(8*1024**3,info.st_size))}
        else:raise ValueError('Cache recovery linked/special/hardlinked entry')
    if any(file_identity(q.lstat())!=identity for q,identity in witnesses):raise ValueError('Cache recovery tree changed')
    return rows


def inspect(project):
    parent=project/'physics_sim';pending=parent/'.cache-publication.pending'
    raw=read(pending,1023).decode('utf-8');pending_identity=file_identity(regular(pending,1023));attempt=path(raw)
    if attempt.parent!=parent or not re.fullmatch(r'\.cache-publication-attempt-[A-Za-z0-9]{6}',attempt.name) or not attempt.is_dir():raise ValueError('Cache recovery attempt scope')
    planbytes=read(attempt/'plan.json',65536);journal_identity=file_identity(regular(attempt/'plan.json',65536));record,_=admitted_json(planbytes)
    if set(record)!={'schema','targets'} or record['schema']!='physics-cache-publication-v1' or not isinstance(record['targets'],list) or len(record['targets'])!=7:raise ValueError('Cache recovery journal schema')
    targets=record['targets'];run=None;expected=None
    for i,row in enumerate(targets):
        if not isinstance(row,dict) or set(row)!={'path','existed'} or not isinstance(row['path'],str) or type(row['existed']) is not bool:raise ValueError('Cache recovery journal target')
        target=path(row['path'])
        if i==0:
            run=target.name
            if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,62}',run):raise ValueError('Cache recovery run identifier')
            expected=[project/f'assets/vf3d/runs/{run}',project/'assets/vf3d/active',project/f'assets/physics/runs/{run}',project/'assets/physics/active',parent/'active_cache_manifest.json',parent/'cache_manifest.json',parent/f'runs/{run}/cache_manifest.json']
        if target!=expected[i]:raise ValueError('Cache recovery target outside fixed layout')
    allowed={'plan.json','complete','rolled-back'}|{f'{prefix}-{i}' for prefix in ('new','prior','rollback-new') for i in range(7)}
    with os.scandir(attempt) as children:
        for entry in children:
            if entry.name not in allowed:raise ValueError('Cache recovery unknown attempt entry')
    budget={'entries':0,'bytes':0,'started':time.monotonic()};slots=[]
    for i,row in enumerate(targets):
        target=expected[i];new=observe(attempt/f'new-{i}',budget);prior=observe(attempt/f'prior-{i}',budget);current=observe(target,budget);displaced=observe(attempt/f'rollback-new-{i}',budget)
        for tree in (new,prior,current,displaced):
            if tree is not None and tree['.']['kind']!=('directory' if i<4 else 'file'):raise ValueError('Cache recovery slot kind')
        if row['existed'] and prior is None and current is None:raise ValueError('Cache recovery original missing')
        if not row['existed'] and prior is not None:raise ValueError('Cache recovery unexpected predecessor')
        if new is not None and current is not None and (prior is not None or not row['existed']):raise ValueError('Cache recovery ambiguous installed slot')
        if displaced is not None and not ((row['existed'] and prior is None and current is not None) or (row['existed'] and prior is not None and current is None) or (not row['existed'] and current is None)):raise ValueError('Cache recovery interrupted rollback requires inspection')
        slots.append({'target':str(target),'existed':row['existed'],'current':current,'staged':new,'prior':prior,'rollback_new':displaced})
    for i,row in enumerate(slots):
        for key,base in (('current',Path(row['target'])),('staged',attempt/f'new-{i}'),('prior',attempt/f'prior-{i}'),('rollback_new',attempt/f'rollback-new-{i}')):
            tree=row[key]
            if tree is None:
                try:base.lstat()
                except FileNotFoundError:continue
                raise ValueError('Cache recovery absent slot appeared')
            if any(file_identity(path(base/name).lstat())!=tuple(entry['identity']) for name,entry in tree.items()):raise ValueError('Cache recovery whole-plan changed')
    terminal={}
    for name in ('complete','rolled-back'):
        marker=attempt/name
        try:terminal[name]=read(marker,0).hex()
        except FileNotFoundError:terminal[name]=None
    if read(pending,1023).decode('utf-8')!=raw or read(attempt/'plan.json',65536)!=planbytes:raise ValueError('Cache recovery journal changed')
    if file_identity(regular(pending,1023))!=pending_identity or file_identity(regular(attempt/'plan.json',65536))!=journal_identity:raise ValueError('Cache recovery record identity changed')
    result={'pending_identity':pending_identity,'journal_identity':journal_identity,'schema':'physics-cache-recovery-plan-v1','project':str(project),'attempt':str(attempt),'pending_sha256':hashlib.sha256(raw.encode()).hexdigest(),'journal_sha256':hashlib.sha256(planbytes).hexdigest(),'terminal':terminal,'slots':slots,'entries':budget['entries'],'bytes':budget['bytes'],'action':'retain_new_and_restore_predecessors','mutation_performed':False,'forward_promotion_allowed':False}
    result['plan_sha256']=hashlib.sha256(json.dumps(result,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    return result


def plan(project):
    with owned(project) as selected:return inspect(selected)


def sync(directory):
    fd=os.open(path(directory),os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
    try:os.fsync(fd)
    finally:os.close(fd)


def rollback(project,expected):
    if not re.fullmatch('[0-9a-f]{64}',expected or ''):raise ValueError('Rollback requires exact read-only plan digest')
    with owned(project,True) as selected:
        state=inspect(selected)
        if state['plan_sha256']!=expected:raise ValueError('Rollback plan changed')
        lock=selected/'physics_sim/.cache-publication.lock';lock_identity=file_identity(regular(lock,0))
        attempt=Path(state['attempt']);budget={'entries':0,'bytes':0,'started':time.monotonic()}
        for i,row in enumerate(state['slots']):
            if file_identity(regular(lock,0))!=lock_identity:raise ValueError('Rollback owner namespace changed')
            target=Path(row['target']);prior=attempt/f'prior-{i}';displaced=attempt/f'rollback-new-{i}'
            if observe(target,budget)!=row['current'] or observe(prior,budget)!=row['prior']:raise ValueError('Rollback slot changed')
            if row['prior'] is not None or not row['existed']:
                if row['current'] is not None:
                    if displaced.exists():raise ValueError('Rollback displacement already exists')
                    os.rename(target,displaced);sync(target.parent);sync(attempt)
                if row['prior'] is not None:os.rename(prior,target);sync(target.parent);sync(attempt)
        # Re-observe all restored slots before releasing the hold. No output is deleted.
        budget={'entries':0,'bytes':0,'started':time.monotonic()}
        for i,row in enumerate(state['slots']):
            current=observe(Path(row['target']),budget)
            original=row['prior'] if row['prior'] is not None else row['current'] if row['existed'] else None
            def content(tree):return None if tree is None else {name:{k:v for k,v in entry.items() if k!='identity'} for name,entry in tree.items()}
            if content(current)!=content(original):raise ValueError('Rollback restored content unverified')
        marker=attempt/'rolled-back'
        if not marker.exists():
            fd=os.open(marker,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600)
            try:os.fsync(fd)
            finally:os.close(fd)
        sync(attempt)
        pending=selected/'physics_sim/.cache-publication.pending'
        if file_identity(regular(pending,1023))!=tuple(state['pending_identity']) or hashlib.sha256(read(pending,1023)).hexdigest()!=state['pending_sha256']:raise ValueError('Rollback pending hold changed')
        if file_identity(regular(lock,0))!=lock_identity:raise ValueError('Rollback owner namespace changed')
        pending.unlink();sync(pending.parent)
        return {'status':'rollback_content_verified','attempt':str(attempt),'plan_sha256':expected,'mutation_performed':True,'new_output_retained':True,'archive_or_prune_performed':False}


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--project',type=Path,required=True)
    parser.add_argument('--rollback',action='store_true');parser.add_argument('--expected-plan-sha256');args=parser.parse_args()
    try:result=rollback(args.project,args.expected_plan_sha256) if args.rollback else plan(args.project)
    except (OSError,ValueError,UnicodeError) as error:print(json.dumps({'status':'held','reason':str(error),'mutation_performed':'unconfirmed' if args.rollback else False}));return 2
    print(json.dumps(result,indent=2));return 0

if __name__=='__main__':raise SystemExit(main())
