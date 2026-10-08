"""Preserve Desktop app predecessors before an already-authorized local refresh."""
import argparse
import ctypes
import errno
import fcntl
import hashlib
import json
import os
from pathlib import Path
import plistlib
import shutil
import stat
import subprocess
import sys
import time
import math
import uuid

from clean_outputs import no_symlinks
from check_clean_root import read_json

APP_NAMES = {'kinetiC.app', 'kinetiC Main Edit.app'}


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''):
            result.update(block)
    return result.hexdigest()


INVENTORY_LIMITS = {'max_entries':100000, 'max_bytes':34359738368,
                    'max_file_bytes':8589934592, 'wall_cap':120, 'max_depth':256}


class InventoryBudget:
    """One cooperative count/byte/deadline budget for an entire identity pass."""

    def __init__(self, *, max_entries=100000, max_bytes=34359738368,
                 max_file_bytes=8589934592, wall_cap=120, max_depth=256):
        for name,value,maximum in (('entries',max_entries,1000000),('bytes',max_bytes,137438953472),
                ('file bytes',max_file_bytes,34359738368),('depth',max_depth,1024)):
            if type(value) is not int or not 1<=value<=maximum:raise ValueError('Invalid inventory '+name+' bound')
        if type(wall_cap) not in (int,float) or not math.isfinite(wall_cap) or not 0<wall_cap<=600:
            raise ValueError('Invalid inventory wall bound')
        self.max_entries=max_entries;self.max_bytes=max_bytes
        self.max_file_bytes=max_file_bytes;self.wall_cap=wall_cap;self.max_depth=max_depth
        self.started=time.monotonic();self.entries=0;self.bytes=0

    def check(self):
        if time.monotonic()-self.started>=self.wall_cap:raise ValueError('Inventory wall bound reached')

    def entry(self):
        self.check();self.entries+=1
        if self.entries>self.max_entries:raise ValueError('Inventory entry bound reached')


def inventory(root, *, max_entries=100000, max_bytes=34359738368,
              max_file_bytes=8589934592, wall_cap=120, max_depth=256, budget=None):
    """Bind bytes/modes/internal links with one optionally shared scan budget.

    Limits are cooperative between filesystem calls, not a syscall timeout or an
    atomic filesystem snapshot. Every observed identity is rechecked at the end.
    """
    if budget is None:
        budget=InventoryBudget(max_entries=max_entries,max_bytes=max_bytes,
            max_file_bytes=max_file_bytes,wall_cap=wall_cap,max_depth=max_depth)
    if not isinstance(budget,InventoryBudget):raise ValueError('Invalid inventory budget')
    max_file_bytes=budget.max_file_bytes;max_depth=budget.max_depth;max_bytes=budget.max_bytes
    root=Path(os.path.abspath(root))
    if root.parent.resolve()!=root.parent:raise ValueError('Inventory parent symlink is unclassified')
    budget.entry();rows={};observed={};pending=[(root,0)]
    check_time=budget.check
    def identity(info):
        return (info.st_dev,info.st_ino,info.st_mode,info.st_size,info.st_mtime_ns,info.st_ctime_ns)
    def open_parent(path):
        descriptor=os.open('/',os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
        prefix=Path('/')
        try:
            for component in path.parent.parts[1:]:
                check_time();next_descriptor=os.open(component,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW,dir_fd=descriptor)
                os.close(descriptor);descriptor=next_descriptor;prefix=prefix/component
                expected=observed.get(prefix)
                selected=os.fstat(descriptor)
                if expected and (selected.st_dev,selected.st_ino)!=expected[:2]:
                    raise ValueError('Inventory ancestor changed during admission')
            return descriptor
        except BaseException:
            os.close(descriptor);raise
    def open_leaf(path, flags):
        parent=open_parent(path)
        try:return os.open(path.name,flags,dir_fd=parent)
        finally:os.close(parent)
    while pending:
        check_time();path,depth=pending.pop()
        if depth>max_depth:raise ValueError('Inventory depth bound reached')
        if path.parent.resolve()!=path.parent:raise ValueError('Inventory parent changed to a symlink')
        info=path.lstat();observed[path]=identity(info);row={'mode':stat.S_IMODE(info.st_mode)}
        if stat.S_ISLNK(info.st_mode):
            target=os.readlink(path)
            if Path(target).is_absolute() or not path.resolve().is_relative_to(root):
                raise ValueError('App link escapes bundle: '+str(path))
            row.update(kind='symlink',target=target)
        elif stat.S_ISDIR(info.st_mode):
            row.update(kind='directory')
            descriptor=open_leaf(path,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
            try:
                if identity(os.fstat(descriptor))!=observed[path]:raise ValueError('Inventory directory changed during admission')
                with os.scandir(descriptor) as children:
                    for child in children:
                        budget.entry()
                        pending.append((path/child.name,depth+1))
            finally:os.close(descriptor)
        elif stat.S_ISREG(info.st_mode):
            if info.st_size>max_file_bytes or budget.bytes+info.st_size>max_bytes:
                raise ValueError('Inventory byte bound reached')
            descriptor=open_leaf(path,os.O_RDONLY|os.O_NOFOLLOW|os.O_NONBLOCK)
            try:
                selected=os.fstat(descriptor)
                if not stat.S_ISREG(selected.st_mode) or identity(selected)!=observed[path]:
                    raise ValueError('Inventory file changed during admission')
                result=hashlib.sha256();count=0
                while True:
                    check_time();block=os.read(descriptor,1048576)
                    if not block:break
                    count+=len(block);budget.bytes+=len(block)
                    if count>max_file_bytes or budget.bytes>max_bytes:raise ValueError('Inventory byte bound reached')
                    result.update(block)
                if count!=info.st_size or identity(os.fstat(descriptor))!=observed[path]:
                    raise ValueError('Inventory file changed during hashing')
                row.update(kind='file',bytes=count,sha256=result.hexdigest())
            finally:os.close(descriptor)
        else:raise ValueError('App contains a special file: '+str(path))
        rows[str(path.relative_to(root))]=row
    for path,expected in observed.items():
        check_time()
        if path.parent.resolve()!=path.parent or identity(path.lstat())!=expected:
            raise ValueError('Inventory changed during readback: '+str(path))
    return rows


def check_bundle(root, bundle_id):
    if not root.is_dir() or root.is_symlink():
        raise ValueError('App must be a real directory: '+str(root))
    rows = inventory(root)
    plist = root/'Contents/Info.plist'
    if not plist.is_file() or plist.is_symlink():
        raise ValueError('App requires a regular Info.plist')
    if plist.stat().st_size > 1024*1024:
        raise ValueError('App Info.plist exceeds size limit')
    with plist.open('rb') as stream:
        info = plistlib.load(stream)
    if not isinstance(info, dict) or info.get('CFBundleIdentifier') != bundle_id:
        raise ValueError('App bundle identity mismatch: '+str(root))
    return rows


def copy_bundle(source, destination):
    if sys.platform == 'darwin':
        # ditto preserves macOS bundle attributes and resource forks. Inventory
        # proves ordinary bytes/modes/links only; authentication is a separate gate.
        subprocess.run(['/usr/bin/ditto', str(source), str(destination)], check=True)
    else:
        shutil.copytree(source, destination, symlinks=True)


def write_record(path, value):
    temporary = path.with_name(path.name+'.pending')
    with temporary.open('w') as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write('\n');stream.flush();os.fsync(stream.fileno())
    os.replace(temporary, path)
    sync_directory(path.parent)


def sync_directory(path):
    fd = os.open(path, os.O_RDONLY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def rename_exclusive(source, destination):
    """Require native no-replace rename; never degrade to overwriting rename."""
    library=ctypes.CDLL(None,use_errno=True)
    try:
        if sys.platform=='darwin':
            function=library.renamex_np
            function.argtypes=[ctypes.c_char_p,ctypes.c_char_p,ctypes.c_uint]
            function.restype=ctypes.c_int
            code=function(os.fsencode(source),os.fsencode(destination),0x00000004)  # RENAME_EXCL
        elif sys.platform.startswith('linux'):
            function=library.renameat2
            function.argtypes=[ctypes.c_int,ctypes.c_char_p,ctypes.c_int,ctypes.c_char_p,ctypes.c_uint]
            function.restype=ctypes.c_int
            code=function(-100,os.fsencode(source),-100,os.fsencode(destination),1)  # AT_FDCWD, RENAME_NOREPLACE
        else:
            raise OSError(errno.ENOTSUP,'Exclusive directory rename is unsupported')
    except AttributeError as error:
        raise OSError(errno.ENOTSUP,'Exclusive directory rename is unavailable') from error
    if code:
        number=ctypes.get_errno()
        raise OSError(number,os.strerror(number),str(destination))


def recovery_completed(attempt, receipt, row, destination, check_destination=False):
    """A recovery is separate evidence bound to the unchanged attempt journal."""
    for proof in sorted((attempt/'recovery').glob('*/receipt.json')):
        no_symlinks(proof,Path.home())
        data=read_json(proof,16*1024*1024)
        if not isinstance(data,dict):raise ValueError('Invalid Desktop recovery proof')
        if (data.get('state') == 'completed' and data.get('attempt_receipt_sha256') == digest(receipt)
                and data.get('destination') == str(destination)):
            expected=data.get('destination_inventory')
            valid = expected in (row.get('predecessor_inventory'),row.get('source_inventory'))
            if expected is None:valid = row.get('predecessor_inventory') is None
            if not valid:raise ValueError('Recovery proof inventory does not bind the attempt')
            if not check_destination:return True
            if expected is None:return not destination.exists() and not destination.is_symlink()
            return destination.exists() and inventory(destination)==expected
    return False


def recovery_plan(attempt):
    attempt=Path(os.path.abspath(attempt))
    home=Path.home();history=home/'Desktop/.physics-sim-app-history'
    no_symlinks(attempt,home)
    if attempt.parent != history or not attempt.is_dir():
        raise ValueError('Recovery requires an exact direct-child Desktop history attempt')
    receipt=attempt/'receipt.json';no_symlinks(receipt,home)
    row=read_json(receipt,16*1024*1024)
    if not isinstance(row,dict) or row.get('schema') != 'physics_sim_desktop_replacement_v1':
        raise ValueError('Recovery requires a typed Desktop replacement journal')
    destination=Path(row.get('destination',''))
    if destination.parent != home/'Desktop' or destination.name not in APP_NAMES:
        raise ValueError('Recovery journal has an invalid Desktop destination')
    no_symlinks(destination,home)
    if not isinstance(row.get('bundle_id'),str) or not row['bundle_id']:
        raise ValueError('Recovery journal lacks bundle identity')
    old=row.get('predecessor_inventory');new=row.get('source_inventory')
    if (old is not None and not isinstance(old,dict)) or not isinstance(new,dict) or not new:
        raise ValueError('Recovery journal has invalid inventories')
    backups=[]
    for name in ('predecessor','displaced'):
        path=attempt/name/destination.name;no_symlinks(path,home)
        if path.exists():
            if old is None or check_bundle(path,row['bundle_id']) != old:
                raise ValueError('Recovery predecessor inventory mismatch: '+str(path))
            backups.append(path)
    if destination.exists():
        current=check_bundle(destination,row['bundle_id'])
        if current == old:action='reconcile_predecessor'
        elif current == new:action='reconcile_candidate'
        else:raise ValueError('Recovery refuses an occupied destination with different contents')
        chosen=None
    elif old is None:
        action='reconcile_absent';current=None;chosen=None
    else:
        if not backups:raise ValueError('Recovery has no verified predecessor copy')
        action='restore_predecessor';current=old;chosen=backups[0]
    return {'schema':'physics_sim_desktop_recovery_plan_v1','attempt':str(attempt),
            'attempt_receipt_sha256':digest(receipt),'destination':str(destination),
            'bundle_id':row['bundle_id'],'action':action,
            'predecessor':str(chosen) if chosen else None,
            'destination_inventory':current,'authentication_verified':False,
            'release_authority_granted':False}


def recover(attempt, apply=False):
    planned=recovery_plan(attempt)
    if not apply:return dict(planned,state='plan_only')
    attempt=Path(planned['attempt']);destination=Path(planned['destination'])
    history=attempt.parent
    lock=history/(hashlib.sha256(str(destination).encode()).hexdigest()+'.lock')
    no_symlinks(lock,Path.home())
    fd=os.open(lock,os.O_RDWR|os.O_NOFOLLOW)
    journal=None;record=None
    try:
        try:fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:raise ValueError('Recovery held by active Desktop refresh/recovery')
        if recovery_plan(attempt) != planned:
            raise ValueError('Recovery plan changed before application')
        row=read_json(attempt/'receipt.json',16*1024*1024)
        if recovery_completed(attempt,attempt/'receipt.json',row,destination,check_destination=True):
            return dict(planned,state='already_recovered')
        parent=attempt/'recovery';no_symlinks(parent,Path.home());parent.mkdir(exist_ok=True)
        journal=parent/str(uuid.uuid4());journal.mkdir()
        record=dict(planned,state='staging')
        write_record(journal/'receipt.json',record)
        if planned['action']=='restore_predecessor':
            stage=journal/destination.name
            copy_bundle(Path(planned['predecessor']),stage)
            if check_bundle(stage,planned['bundle_id']) != planned['destination_inventory']:
                raise ValueError('Recovery staging copy mismatch')
            if recovery_plan(attempt) != planned:
                raise ValueError('Recovery inputs changed during staging')
            # No occupied app is ever overwritten; retain source backup and stage.
            rename_exclusive(stage,destination);sync_directory(destination.parent)
        if planned['destination_inventory'] is None:
            if destination.exists() or destination.is_symlink():raise ValueError('Recovery destination appeared')
        elif check_bundle(destination,planned['bundle_id']) != planned['destination_inventory']:
            raise ValueError('Recovery destination readback mismatch')
        record['state']='completed';write_record(journal/'receipt.json',record)
        return dict(planned,state='completed',recovery_receipt=str(journal/'receipt.json'))
    except BaseException as error:
        if journal is not None and record is not None:
            record['state']='failed_retained';record['failure']=str(error)
            write_record(journal/'receipt.json',record)
        raise
    finally:os.close(fd)


def replace(repo, source, destination, bundle_id):
    repo = repo.resolve()
    source = Path(os.path.abspath(source))
    destination = Path(os.path.abspath(destination))
    desktop = Path.home()/'Desktop'
    if source.name not in APP_NAMES or destination.name != source.name:
        raise ValueError('Refresh requires the matching PhysicsSim app name')
    if destination.parent != desktop:
        raise ValueError('Refresh destination must be directly under the current user Desktop')
    no_symlinks(source, repo)
    if not (source.is_relative_to(repo/'build') or source.is_relative_to(repo/'dist')):
        raise ValueError('Refresh source must be checkout build/dist staging')
    no_symlinks(destination, Path.home())
    source_rows = check_bundle(source, bundle_id)
    desktop.mkdir(parents=True, exist_ok=True)
    history = desktop/'.physics-sim-app-history'
    no_symlinks(history, Path.home());history.mkdir(exist_ok=True)
    lock = history/(hashlib.sha256(str(destination).encode()).hexdigest()+'.lock')
    fd = os.open(lock, os.O_RDWR|os.O_CREAT|os.O_NOFOLLOW, 0o600)
    attempt = None; record = None
    try:
        try:
            fcntl.flock(fd, fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:
            raise ValueError('Desktop replacement held by another active refresh')
        for folder in history.iterdir():
            if folder.is_symlink():
                raise ValueError('Desktop refresh held by unclassified history: '+str(folder))
            if not folder.is_dir():
                continue
            prior=folder/'receipt.json'
            no_symlinks(prior,Path.home())
            try:
                previous=read_json(prior,16*1024*1024)
            except (OSError,ValueError,RecursionError) as error:
                raise ValueError('Desktop refresh held by unreadable history: '+str(prior)) from error
            if not isinstance(previous,dict) or not isinstance(previous.get('destination'),str):
                raise ValueError('Desktop refresh held by invalid history: '+str(prior))
            if previous['destination'] == str(destination) and previous.get('state') != 'completed' and previous.get('destination_restored_or_unchanged') is not True:
                if not recovery_completed(folder,prior,previous,destination):
                    raise ValueError('Desktop refresh requires recovery of retained attempt: '+str(prior))
        old_rows = check_bundle(destination, bundle_id) if destination.exists() else None
        attempt = history/str(uuid.uuid4());attempt.mkdir()
        candidate = attempt/source.name
        predecessor = attempt/'predecessor'/destination.name
        displaced = attempt/'displaced'/destination.name
        record = {'schema':'physics_sim_desktop_replacement_v1',
                  'artifact_class':'installed_package_predecessor',
                  'destination':str(destination), 'source':str(source),
                  'bundle_id':bundle_id, 'source_inventory':source_rows,
                  'predecessor_inventory':old_rows, 'state':'staging',
                  'authentication_verified':False, 'release_authority_granted':False}
        write_record(attempt/'receipt.json',record)
        copy_bundle(source,candidate)
        if inventory(candidate) != source_rows or inventory(source) != source_rows:
            raise ValueError('Candidate copy or source changed before publication')
        if old_rows is not None:
            predecessor.parent.mkdir()
            copy_bundle(destination,predecessor)
            if inventory(predecessor) != old_rows or inventory(destination) != old_rows:
                raise ValueError('Predecessor copy or installed app changed before publication')
        record['state']='copies_verified';write_record(attempt/'receipt.json',record)
        # Existing names are never deleted. Journal and retained copies make the
        # two-rename interval recoverable; it is not an atomic multi-path swap.
        if old_rows is not None:
            displaced.parent.mkdir()
            rename_exclusive(destination,displaced)
            sync_directory(desktop)
            record['state']='predecessor_displaced';write_record(attempt/'receipt.json',record)
        elif destination.exists() or destination.is_symlink():
            raise ValueError('Destination appeared during refresh')
        rename_exclusive(candidate,destination)
        sync_directory(desktop)
        if inventory(destination) != source_rows:
            raise ValueError('Published app readback mismatch')
        record['state']='completed';write_record(attempt/'receipt.json',record)
        return {'state':'completed','destination':str(destination),'receipt':str(attempt/'receipt.json'),
                'predecessor':str(predecessor) if old_rows is not None else None}
    except BaseException as error:
        if attempt is not None and record is not None:
            try:
                if old_rows is not None and not destination.exists() and displaced.exists() and inventory(displaced) == old_rows:
                    rename_exclusive(displaced,destination);sync_directory(desktop)
                record['destination_restored_or_unchanged'] = (
                    inventory(destination) == old_rows if old_rows is not None and destination.exists()
                    else old_rows is None and not destination.exists())
            except (OSError,ValueError) as recovery_error:
                record['recovery_error']=str(recovery_error)
                record['destination_restored_or_unchanged']=False
            record['failure']=str(error);record['last_state']=record['state'];record['state']='failed_retained'
            write_record(attempt/'receipt.json',record)
        raise
    finally:
        os.close(fd)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo',type=Path,default=Path.cwd())
    parser.add_argument('--source',type=Path)
    parser.add_argument('--destination',type=Path)
    parser.add_argument('--bundle-id')
    parser.add_argument('--recover-attempt',type=Path)
    parser.add_argument('--apply',action='store_true',help='Apply an exact predecessor recovery plan')
    args=parser.parse_args()
    try:
        if args.recover_attempt:
            if args.source or args.destination or args.bundle_id:
                parser.error('Recovery uses the retained attempt identity, not replacement arguments')
            print(json.dumps(recover(args.recover_attempt,args.apply)))
        else:
            if args.apply or not all((args.source,args.destination,args.bundle_id)):
                parser.error('Replacement requires source, destination and bundle-id; apply is recovery-only')
            print(json.dumps(replace(args.repo,args.source,args.destination,args.bundle_id)))
    except (OSError, ValueError, subprocess.CalledProcessError, plistlib.InvalidFileException, RecursionError) as error:
        parser.exit(2,str(error)+'\n')


if __name__=='__main__':main()
