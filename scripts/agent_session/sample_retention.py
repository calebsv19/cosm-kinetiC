"""Bounded sample-history preservation; age never grants pending-request removal."""
import hashlib
import json
import os
from pathlib import Path
import stat

MAX_REQUESTS = 4096

LIMITS = {'request.json':65536, 'result.json':4194304, 'receipt.json':65536}


def regular(path, limit):
    # Refuse symlink components rather than resolving outside the run namespace.
    for parent in (path, *path.parents):
        if parent.is_symlink():
            raise ValueError('Sample history symlink held')
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        before = os.fstat(fd)
        if not stat.S_ISREG(before.st_mode) or before.st_size > limit:
            raise ValueError('Sample history file type/size held')
        with os.fdopen(fd, 'rb', closefd=False) as stream:
            data = stream.read(limit + 1)
        after = os.fstat(fd)
        fields = ('st_dev','st_ino','st_size','st_mtime_ns','st_ctime_ns')
        if len(data) > limit or any(getattr(before,key)!=getattr(after,key) for key in fields):
            raise ValueError('Sample history input changed')
        current = path.lstat()
        if any(getattr(after,key)!=getattr(current,key) for key in fields):
            raise ValueError('Sample history path changed')
        return data
    finally:
        os.close(fd)


def flush_directory(path):
    if path.is_symlink():raise ValueError('Sample history directory symlink held')
    fd=os.open(path,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
    try:os.fsync(fd)
    finally:os.close(fd)


def retain(path, data):
    if path.exists() or path.is_symlink():
        if regular(path, max(len(data),65536)) != data:
            raise ValueError('Sample history predecessor mismatch')
        return
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
    with os.fdopen(fd, 'wb') as stream:
        stream.write(data); stream.flush(); os.fsync(stream.fileno())
    if regular(path, max(len(data),65536)) != data:
        raise ValueError('Sample history readback mismatch')


def location(run, request_id):
    if not request_id or any(c not in 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-' for c in request_id) or len(request_id)>64:
        raise ValueError('Invalid retained sample ID')
    root = run / 'sample_history'; selected = root / request_id
    for path in (run,root,selected):
        if path.is_symlink():raise ValueError('Sample history directory symlink held')
    return selected


def verify(run, request_id):
    selected = location(run,request_id)
    receipt = json.loads(regular(selected/'receipt.json',LIMITS['receipt.json']))
    if (not isinstance(receipt,dict) or receipt.get('schema')!='physics_sim_sample_history_v1'
            or receipt.get('request_id')!=request_id or receipt.get('artifact_class')!='operational_job'
            or not isinstance(receipt.get('files'),dict) or 'request.json' not in receipt['files']
            or set(receipt['files'])-{'request.json','result.json'}):
        raise ValueError('Invalid sample history receipt')
    names=set()
    with os.scandir(selected) as entries:
        for entry in entries:
            names.add(entry.name)
            if len(names)>3:raise ValueError('Unknown sample history content held')
    if names != set(receipt['files'])|{'receipt.json'}:
        raise ValueError('Unknown sample history content held')
    for name, expected in receipt['files'].items():
        data = regular(selected/name,LIMITS[name])
        if expected != {'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}:
            raise ValueError('Sample history integrity mismatch')
    return selected


def retire(run, request_id):
    selected = location(run,request_id)
    pending = run/'sample_requests'/f'{request_id}.json'
    if pending.exists() or pending.is_symlink():
        raise ValueError('Pending sample cannot be retired')
    inputs = {'request.json':run/'sample_ids'/f'{request_id}.json'}
    response = run/'sample_results'/f'{request_id}.json'
    if response.exists() or response.is_symlink():inputs['result.json']=response
    receipt_path=selected/'receipt.json'
    if receipt_path.exists() or receipt_path.is_symlink():
        verify(run,request_id)
        receipt=json.loads(regular(receipt_path,LIMITS['receipt.json']))
        payloads={name:regular(selected/name,LIMITS[name]) for name in receipt['files']}
        inputs={name:run/('sample_ids' if name=='request.json' else 'sample_results')/f'{request_id}.json'
                for name in payloads}
        if 'result.json' not in payloads and (response.exists() or response.is_symlink()):
            raise ValueError('Unexpected active result after history publication')
    else:
        payloads = {name:regular(path,LIMITS[name]) for name,path in inputs.items()}
        selected.mkdir(parents=True,exist_ok=True)
        # Exclusive copies and readback precede removal. Partial copies remain
        # held and reuse requires exact matching active payloads.
        for name,data in payloads.items():retain(selected/name,data)
        receipt={'schema':'physics_sim_sample_history_v1','request_id':request_id,
                 'artifact_class':'operational_job','files':{
                     name:{'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
                     for name,data in payloads.items()}}
        retain(receipt_path,(json.dumps(receipt,sort_keys=True)+'\n').encode())
        verify(run,request_id)
    if pending.exists() or pending.is_symlink():raise ValueError('Pending sample changed during retirement')
    survivors={name:path for name,path in inputs.items() if path.exists() or path.is_symlink()}
    for name,path in survivors.items():
        if regular(path,LIMITS[name]) != payloads[name]:raise ValueError('Active sample changed during retirement')
    # Preserve file contents and each new history namespace entry before unlink.
    for directory in (selected,selected.parent,run):flush_directory(directory)
    for path in survivors.values():path.unlink()
    for directory in {path.parent for path in survivors.values()}:flush_directory(directory)
    return selected


def admit_new(run):
    total=0
    for name in ('sample_ids','sample_history'):
        directory=run/name
        if directory.is_symlink():raise ValueError('Sample namespace symlink held')
        if not directory.exists():continue
        with os.scandir(directory) as entries:
            for entry in entries:
                total+=1
                if total>=MAX_REQUESTS:
                    raise ValueError('sample_request_limit_reached; retained history requires owner retirement or a new run')
    return total


def history_entries(run):
    root=run/'sample_history'
    if root.is_symlink():raise ValueError('Sample history directory symlink held')
    if not root.exists():return []
    selected=[]
    with os.scandir(root) as entries:
        for entry in entries:
            if len(selected)>=MAX_REQUESTS or not entry.is_dir(follow_symlinks=False):
                raise ValueError('Sample history enumeration bound/type held')
            selected.append(entry.name)
    return sorted(selected)
