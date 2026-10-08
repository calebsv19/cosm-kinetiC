"""Read-only receipt recovery planning and explicit owned no-replace publication."""
import argparse
from contextlib import contextmanager
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import cfd_run_support as c


def context(value):
    if not isinstance(value,dict) or set(value)!={'command','source_sha256','artifact_paths'}:raise ValueError('Recovery context fields')
    command=value['command'];hashes=value['source_sha256'];paths=value['artifact_paths']
    if not isinstance(command,list) or not 1<=len(command)<=4096 or any(not isinstance(x,str) or not 1<=len(x)<=8192 for x in command):raise ValueError('Recovery command bound')
    if not isinstance(hashes,dict) or not 1<=len(hashes)<=256:raise ValueError('Recovery source inventory')
    for name,digest in hashes.items():
        if not isinstance(name,str) or Path(name).name!=name or name in ('.','..') or not isinstance(digest,str) or not re.fullmatch('[0-9a-f]{64}',digest):raise ValueError('Recovery source identity')
    if not isinstance(paths,list) or not 1<=len(paths)<=10000 or any(not isinstance(x,str) for x in paths):raise ValueError('Recovery artifact inventory')
    return command,hashes,paths


@contextmanager
def observe_existing(root,data):
    _,paths=c.reference_owner_paths(root,data);serial=c.reference_serial_paths(root,data)
    all_paths=paths+serial;exclusive={len(paths)-1,len(all_paths)-1};fds=[]
    try:
        for index,path in enumerate(all_paths):
            c.reference_path(path)
            fd=os.open(path,os.O_RDWR|os.O_NOFOLLOW|os.O_NONBLOCK);fds.append(fd)
            info=os.fstat(fd);named=path.lstat()
            if not stat.S_ISREG(info.st_mode) or info.st_nlink!=1 or (info.st_dev,info.st_ino)!=(named.st_dev,named.st_ino):raise ValueError('Recovery lock identity')
            try:fcntl.flock(fd,(fcntl.LOCK_EX if index in exclusive else fcntl.LOCK_SH)|fcntl.LOCK_NB)
            except BlockingIOError:raise ValueError('Recovery namespace active or held')
        yield
    finally:
        for fd in fds:os.close(fd)


def inspect_owned(root,data,path,attempt,value):
    command,hashes,paths=context(value)
    selected,_=c.reference_owner_paths(root,data);path=c.reference_path(path);attempt=c.reference_path(attempt)
    if not path.parent.is_relative_to(Path(data)) or attempt.parent!=path.parent/'.receipt-attempts' or not re.fullmatch('[0-9a-f]{32}',attempt.name):raise ValueError('Recovery attempt scope')
    directory=path.parent
    digest=hashlib.sha256(json.dumps(hashes,sort_keys=True).encode()).hexdigest()
    if directory.name!=digest:raise ValueError('Recovery source partition mismatch')
    for name,expected in hashes.items():
        if c.factor_digest(directory/'source'/name)!=expected:raise ValueError('Recovery frozen source changed')
    request=attempt/'request.json';candidate=attempt/'receipt.candidate.json'
    request_hash=c.factor_digest(request,16384);row=c.factor_json(request,16384)
    if (set(row)!={'schema','destination','sha256','bytes','replacement_allowed'} or row['schema']!='physics_sim_reference_receipt_publication_v1' or row['destination']!=path.name or row['replacement_allowed'] is not False or type(row['bytes']) is not int or not 0<row['bytes']<=16777216 or not isinstance(row['sha256'],str) or not re.fullmatch('[0-9a-f]{64}',row['sha256'])):raise ValueError('Recovery request unadmitted')
    if candidate.lstat().st_size!=row['bytes'] or c.factor_digest(candidate,row['bytes'])!=row['sha256']:raise ValueError('Recovery candidate bytes mismatch')
    receipt=c.reference_receipt(candidate,directory,command,hashes,expected_paths=paths)
    supervision=receipt['supervision']
    if any(supervision.get(key) is not True for key in ('direct_child_reaped','group_cleanup_identity_anchored','terminal_processes_verified')):raise ValueError('Recovery terminal scope unverified')
    try:path.lstat()
    except FileNotFoundError:status='ready_to_publish'
    else:
        if c.factor_digest(path,row['bytes'])!=row['sha256']:raise ValueError('Recovery final predecessor differs')
        status='already_published'
    if c.factor_digest(request,16384)!=request_hash or c.factor_digest(candidate,row['bytes'])!=row['sha256']:raise ValueError('Recovery request/candidate changed during inspection')
    return {'status':status,'receipt':str(path),'candidate':str(candidate),'sha256':row['sha256'],'bytes':row['bytes'],'mutation_performed':False,'all_external_descendants_verified_terminal':False}


def plan(root,data,path,attempt,value):
    context(value)
    with observe_existing(root,data):return inspect_owned(root,data,path,attempt,value)


def apply(root,data,path,attempt,value,expected_sha256):
    if not isinstance(expected_sha256,str) or not re.fullmatch('[0-9a-f]{64}',expected_sha256):raise ValueError('Recovery apply requires exact planned candidate digest')
    initial=plan(root,data,path,attempt,value)
    if initial['sha256']!=expected_sha256:raise ValueError('Recovery planned digest changed')
    with c.reference_ownership(root,data):
        if not c.reference_owner_descriptors(Path(path).parent):raise ValueError('Recovery ownership missing')
        current=inspect_owned(root,data,path,attempt,value)
        if current['sha256']!=expected_sha256:raise ValueError('Recovery candidate changed before apply')
        if current['status']=='already_published':return current
        candidate=Path(current['candidate']);target=Path(path)
        fd=os.open(candidate,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
        try:os.fsync(fd)
        finally:os.close(fd)
        c.reference_sync_directory(candidate.parent);c.reference_sync_directory(candidate.parent.parent)
        if c.factor_digest(candidate,current['bytes'])!=expected_sha256:raise ValueError('Recovery candidate changed before publication')
        os.link(candidate,target,follow_symlinks=False);c.reference_sync_directory(target.parent)
        verified=inspect_owned(root,data,path,attempt,value)
        if verified['sha256']!=expected_sha256 or verified['status']!='already_published':raise ValueError('Recovery publication readback unverified')
        return {**verified,'status':'published_and_verified','mutation_performed':True}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('repo','data','receipt','attempt','context'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--apply',action='store_true');parser.add_argument('--expected-sha256')
    args=parser.parse_args()
    try:
        value=c.factor_json(args.context,16777216)
        result=apply(args.repo,args.data,args.receipt,args.attempt,value,args.expected_sha256) if args.apply else plan(args.repo,args.data,args.receipt,args.attempt,value)
    except (ValueError,OSError) as error:print(json.dumps({'status':'held','reason':str(error),'mutation_performed':'unconfirmed' if args.apply else False}));return 2
    print(json.dumps(result,indent=2));return 0

if __name__=='__main__':raise SystemExit(main())
