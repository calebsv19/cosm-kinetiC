"""Read-only job-pair planning and exact-plan retained rollback or forward recovery."""
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
from cache_publication_recovery import path, regular, read, sync
from cfd_evidence import file_identity
from cfd_run_support import admitted_json

HOLD='.physics-sim-job-publication.pending'
LOCK='.physics-sim-job-operation.lock'
CAP=1024*1024

def digest(value):return hashlib.sha256(value).hexdigest()
def identity(p):
 row=p.lstat();return tuple(file_identity(row))+(row.st_nlink,)
def directory(p):
 p=path(p);s=p.lstat()
 if not stat.S_ISDIR(s.st_mode):raise ValueError('Job recovery directory kind')
 return (s.st_dev,s.st_ino,s.st_mode)

def observe(p,budget,cap=CAP):
 p=path(p)
 try:s=regular(p,cap)
 except FileNotFoundError:return None
 budget['bytes']+=s.st_size
 if budget['bytes']>512*CAP or time.monotonic()-budget['started']>120:raise ValueError('Job recovery operation read/time bound')
 data=read(p,cap)
 if tuple(file_identity(s))+(s.st_nlink,)!=identity(p):raise ValueError('Job recovery input changed')
 return {'identity':identity(p),'bytes':len(data),'sha256':digest(data)}

def decode(p,cap):return admitted_json(read(p,cap))[0]

def admit_root(root):
 if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,62}',root.name) or root.name in ('.','..'):raise ValueError('Job recovery root identifier')
 if root==Path('/') or any(root==Path(x) or root.is_relative_to(x) for x in ('/System','/usr','/bin','/sbin','/private/etc','/Library','/Applications','/dev','/proc','/sys')):raise ValueError('Job recovery protected root')
 if root in (Path('/private/tmp'),Path('/private/var'),Path('/Users'),Path('/home')) or Path.home().is_relative_to(root):raise ValueError('Job recovery broad home/system root')
 for ancestor in root.parents:
  if os.path.lexists(ancestor/'.git'):
   generated=[ancestor/name for name in ('build','tmp','data/experiments','data/runtime','visual_artifacts','export','dist')]
   if not any(root!=p and root.is_relative_to(p) for p in generated):raise ValueError('Job recovery overlaps protected source checkout storage')

@contextmanager
def owned(root,exclusive=False):
 root=path(root);admit_root(root)
 roots=(directory(root),directory(root.parent));lock=root/LOCK;before=regular(lock,0);witness=tuple(file_identity(before))+(before.st_nlink,)
 fd=os.open(lock,os.O_RDWR|os.O_NOFOLLOW|os.O_NONBLOCK|os.O_CLOEXEC)
 def check():
  admit_root(root)
  row=os.fstat(fd)
  if (directory(root),directory(root.parent))!=roots or identity(lock)!=witness or tuple(file_identity(row))+(row.st_nlink,)!=witness:raise ValueError('Job recovery owner changed')
 try:
  check()
  try:fcntl.flock(fd,(fcntl.LOCK_EX if exclusive else fcntl.LOCK_SH)|fcntl.LOCK_NB)
  except BlockingIOError:raise ValueError('Job operation/recovery is active')
  check();yield root,check;check()
 finally:os.close(fd)

def inspect(root,budget):
 roots={str(p):directory(p) for p in (root,root.parent,root/'output')}
 pending=decode(root/HOLD,256)
 version=2 if pending.get('schema')=='physics_sim_job_pair_pending_v2' else 1
 if set(pending)!=({'schema','attempt','plan_sha256'} if version==2 else {'schema','attempt'}) or pending['schema']!=f'physics_sim_job_pair_pending_v{version}' or not isinstance(pending['attempt'],str) or not re.fullmatch(r'\.job-pair-[1-9][0-9]*-[1-9][0-9]*',pending['attempt']) or (version==2 and (not isinstance(pending['plan_sha256'],str) or not re.fullmatch('[0-9a-f]{64}',pending['plan_sha256']))):raise ValueError('Job recovery pending schema/scope')
 attempt=root/pending['attempt'];roots[str(attempt)]=directory(attempt)
 record=decode(attempt/'plan.json',1024)
 fixed={'schema':f'physics_sim_job_pair_v{version}','status_target':'job_status.json','report_target':'output/report.json','old_status':'old-status.json','old_report':'old-report.json','new_status':'new-status.json','new_report':'new-report.json'}
 if set(record)!=set(fixed)|{'root_device','root_inode','old_status_present','old_report_present'}|({'retained_sha256'} if version==2 else set()) or any(record[k]!=v for k,v in fixed.items()) or type(record['root_device']) is not int or type(record['root_inode']) is not int or (record['root_device'],record['root_inode'])!=roots[str(root)][:2] or any(type(record[k]) is not bool for k in ('old_status_present','old_report_present')):raise ValueError('Job recovery journal schema/root binding')
 allowed={'plan.json','old-status.json','old-report.json','new-status.json','new-report.json','completed.json','rollback-intent.json','rolled-back.json','rollback-new-status.json','rollback-new-report.json','forward-intent.json','forwarded.json','forward-old-status.json','forward-old-report.json'}
 children=[]
 with os.scandir(attempt) as entries:
  for entry in entries:
   if len(children)>=128:raise ValueError('Job recovery attempt entry bound')
   if entry.name not in allowed and not re.fullmatch(r'\.(?:rollback|forward)-(?:record|file)-(?:status|report|intent|complete)-[1-9][0-9]*-[1-9][0-9]*\.pending',entry.name):raise ValueError('Job recovery unknown attempt entry')
   children.append(entry.name)
 files={str(root/HOLD):observe(root/HOLD,budget,256),str(attempt/'plan.json'):observe(attempt/'plan.json',budget,1024)}
 for name in sorted(set(children)|allowed):
  if name=='plan.json':continue
  files[str(attempt/name)]=observe(attempt/name,budget,1024 if name in ('completed.json','rollback-intent.json','rolled-back.json','forward-intent.json','forwarded.json') else CAP)
 rollback_selected=any(files[str(attempt/name)] is not None for name in ('rollback-intent.json','rolled-back.json','rollback-new-status.json','rollback-new-report.json'))
 forward_selected=any(files[str(attempt/name)] is not None for name in ('forward-intent.json','forwarded.json','forward-old-status.json','forward-old-report.json'))
 if rollback_selected and forward_selected:raise ValueError('Job recovery conflicting directions')
 if rollback_selected and files[str(attempt/'rollback-intent.json')] is None:raise ValueError('Job recovery rollback intent absent')
 if forward_selected and (version!=2 or files[str(attempt/'forward-intent.json')] is None):raise ValueError('Job recovery verified forward intent absent')
 direction='forward' if forward_selected else 'rollback' if rollback_selected else None
 slots=[]
 for label in ('status','report'):
  old=files[str(attempt/f'old-{label}.json')];new=files[str(attempt/f'new-{label}.json')];present=record[f'old_{label}_present']
  if (old is not None)!=present or new is None:raise ValueError('Job recovery retained generation absent')
  for p in (attempt/f'new-{label}.json',attempt/f'old-{label}.json'):
   if files[str(p)] is not None:decode(p,CAP)
  target=root/record[f'{label}_target'];current=observe(target,budget);files[str(target)]=current
  displaced=files[str(attempt/f"{'forward-old' if forward_selected else 'rollback-new'}-{label}.json")]
  hashes={new['sha256']}|({old['sha256']} if old else set())
  if current is not None and current['sha256'] not in hashes:raise ValueError('Job recovery current generation unknown')
  if displaced is not None and displaced['sha256'] not in hashes:raise ValueError('Job recovery displaced generation unknown')
  if present and current is None and displaced is None:raise ValueError('Job recovery unexplained absent current')
  desired=new if forward_selected else old
  if displaced is not None and current is not None and (desired is None or current['sha256']!=desired['sha256']):raise ValueError('Job recovery ambiguous resumed slot')
  slots.append({'label':label,'target':str(target),'existed':present,'old':old,'new':new,'current':current,'displaced':displaced})
 journal=files[str(attempt/'plan.json')]['sha256']
 if version==2:
  if journal!=pending['plan_sha256']:raise ValueError('Job recovery producer plan digest mismatch')
  declared=record['retained_sha256'];names={'old-status.json','old-report.json','new-status.json','new-report.json'}
  if not isinstance(declared,dict) or set(declared)!=names:raise ValueError('Job recovery producer inventory schema')
  for name in names:
   expected=declared[name];observed=files[str(attempt/name)]
   if expected is None:
    if observed is not None:raise ValueError('Job recovery producer absence mismatch')
   elif not isinstance(expected,str) or not re.fullmatch('[0-9a-f]{64}',expected) or observed is None or observed['sha256']!=expected:raise ValueError('Job recovery producer snapshot digest mismatch')
 binding={'schema':'physics_sim_job_pair_rollback_intent_v1','journal_sha256':journal,'old_sha256':[s['old']['sha256'] if s['old'] else None for s in slots],'new_sha256':[s['new']['sha256'] for s in slots]}
 for name,schema in (('rollback-intent.json','physics_sim_job_pair_rollback_intent_v1'),('rolled-back.json','physics_sim_job_pair_rolled_back_v1'),('forward-intent.json','physics_sim_job_pair_forward_intent_v1'),('forwarded.json','physics_sim_job_pair_forwarded_v1')):
  if files[str(attempt/name)] is not None:
   expected=dict(binding,schema=schema)
   if decode(attempt/name,1024)!=expected:raise ValueError('Job recovery direction binding changed')
 if files[str(attempt/'completed.json')] is not None and decode(attempt/'completed.json',1024)!={'schema':'physics_sim_job_pair_complete_v1'}:raise ValueError('Job recovery completion record unknown')
 if files[str(attempt/'rolled-back.json')] is not None and any((s['current']['sha256'] if s['current'] else None)!=(s['old']['sha256'] if s['old'] else None) for s in slots):raise ValueError('Job recovery rollback receipt contradicts current')
 if files[str(attempt/'forwarded.json')] is not None and any((s['current']['sha256'] if s['current'] else None)!=s['new']['sha256'] for s in slots):raise ValueError('Job recovery forward receipt contradicts current')
 with os.scandir(attempt) as entries:
  again=[]
  for entry in entries:
   if len(again)>=128:raise ValueError('Job recovery attempt entry bound')
   again.append(entry.name)
 if set(again)!=set(children):raise ValueError('Job recovery attempt inventory changed')
 if any(directory(Path(p))!=w for p,w in roots.items()):raise ValueError('Job recovery directory changed')
 for p,row in files.items():
  try:now=identity(Path(p))
  except FileNotFoundError:now=None
  if now!=(tuple(row['identity']) if row else None):raise ValueError('Job recovery whole-plan changed')
 result={'schema':'physics_sim_job_pair_recovery_plan_v1','root':str(root),'attempt':str(attempt),'directories':roots,'files':files,'slots':slots,'binding':binding,'forward_binding':dict(binding,schema='physics_sim_job_pair_forward_intent_v1'),'producer_inventory_verified':version==2,'selected_direction':direction,'rollback_allowed':not forward_selected,'action':'review_explicit_recovery_direction','mutation_performed':False,'forward_promotion_allowed':version==2 and not rollback_selected}
 result['plan_sha256']=digest(json.dumps(result,sort_keys=True,separators=(',',':')).encode())
 return result

def budget():return {'bytes':0,'started':time.monotonic()}
def plan(root):
 with owned(root) as (selected,check):
  result=inspect(selected,budget());check();return result

def sync_pair(a,b):sync(a);sync(b)
def temp_name(attempt,label,kind,direction='rollback'):
 for number in range(1,65):
  p=attempt/f'.{direction}-{kind}-{label}-{os.getpid()}-{number}.pending'
  try:p.lstat()
  except FileNotFoundError:return p
 raise ValueError('Job recovery retained staging bound')

def write_stage(p,data):
 fd=os.open(p,os.O_RDWR|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW|os.O_CLOEXEC,0o600)
 try:
  sync(p.parent)
  offset=0
  while offset<len(data):
   count=os.write(fd,data[offset:])
   if count<=0:raise OSError('Job recovery short write')
   offset+=count
  os.fsync(fd)
 finally:os.close(fd)
 if read(p,CAP)!=data:raise ValueError('Job recovery stage readback changed')
 sync(p.parent)

def recover(root,expected,direction):
 if direction not in ('rollback','forward'):raise ValueError('Job recovery direction invalid')
 if not re.fullmatch('[0-9a-f]{64}',expected or ''):raise ValueError('Job recovery requires exact read-only plan digest')
 with owned(root,True) as (selected,check):
  reads=budget();state=inspect(selected,reads)
  if state['plan_sha256']!=expected:raise ValueError('Job recovery plan changed')
  if direction=='forward' and not state['forward_promotion_allowed']:raise ValueError('Job forward requires verified v2 inventory and no rollback intent')
  if direction=='rollback' and not state['rollback_allowed']:raise ValueError('Job recovery conflicts with committed forward direction')
  attempt=Path(state['attempt']);source_key='new' if direction=='forward' else 'old'
  binding_key='forward_binding' if direction=='forward' else 'binding'
  marker='forwarded.json' if direction=='forward' else 'rolled-back.json'
  marker_schema='physics_sim_job_pair_forwarded_v1' if direction=='forward' else 'physics_sim_job_pair_rolled_back_v1'
  def change(action,allowed_paths):
   nonlocal state
   check();before=inspect(selected,reads)
   if before['plan_sha256']!=state['plan_sha256']:raise ValueError('Job recovery whole-plan drift')
   action();check();after=inspect(selected,reads)
   if before['directories']!=after['directories']:raise ValueError('Job recovery directory identity drift')
   for p in set(before['files'])|set(after['files']):
    if p not in {str(x) for x in allowed_paths} and before['files'].get(p)!=after['files'].get(p):raise ValueError('Job recovery unrelated evidence drift')
   state=after
  def record(name,label,payload):
   target=attempt/name
   if state['files'].get(str(target)) is not None:return
   stage=temp_name(attempt,label,'record',direction);data=(json.dumps(payload,sort_keys=True,separators=(',',':'))+'\n').encode()
   change(lambda:write_stage(stage,data),[stage])
   def publish():
    if target.exists():raise ValueError('Job recovery receipt appeared')
    os.rename(stage,target);sync(attempt)
   change(publish,[stage,target])
  record(f'{direction}-intent.json','intent',state[binding_key])
  for i in range(2):
   slot=state['slots'][i];label=slot['label'];target=Path(slot['target']);old=slot[source_key];current=slot['current'];desired=old['sha256'] if old else None
   if (current['sha256'] if current else None)==desired:continue
   stage=None
   if old:
    stage=temp_name(attempt,label,'file',direction);data=read(attempt/f'{source_key}-{label}.json',CAP)
    if digest(data)!=desired:raise ValueError('Job recovery original changed')
    change(lambda:write_stage(stage,data),[stage])
   displaced=attempt/f"{'forward-old' if direction=='forward' else 'rollback-new'}-{label}.json"
   if current is not None:
    def retain():
     if displaced.exists():raise ValueError('Job recovery displacement appeared')
     os.rename(target,displaced);sync_pair(target.parent,attempt)
    change(retain,[target,displaced])
   if old:
    def restore():
     if target.exists():raise ValueError('Job recovery destination appeared')
     os.rename(stage,target);sync_pair(target.parent,attempt)
    change(restore,[stage,target])
  if any((s['current']['sha256'] if s['current'] else None)!=(s[source_key]['sha256'] if s[source_key] else None) for s in state['slots']):raise ValueError('Job recovery restored pair unverified')
  record(marker,'complete',dict(state[binding_key],schema=marker_schema))
  check();verified=inspect(selected,reads)
  if verified['plan_sha256']!=state['plan_sha256']:raise ValueError('Job recovery final drift')
  (selected/HOLD).unlink();sync(selected);check()
  return {'status':f'{direction}_content_verified','attempt':str(attempt),'plan_sha256':expected,'mutation_performed':True,'failed_current_retained':True,'old_snapshot_bytes_retained':True,'forward_promotion_allowed':False}

def rollback(root,expected):return recover(root,expected,'rollback')
def forward(root,expected):return recover(root,expected,'forward')

def main():
 parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--job-root',type=Path,required=True);choice=parser.add_mutually_exclusive_group();choice.add_argument('--rollback',action='store_true');choice.add_argument('--forward',action='store_true');parser.add_argument('--expected-plan-sha256');args=parser.parse_args()
 try:result=forward(args.job_root,args.expected_plan_sha256) if args.forward else rollback(args.job_root,args.expected_plan_sha256) if args.rollback else plan(args.job_root)
 except (OSError,ValueError,UnicodeError) as error:print(json.dumps({'status':'held','reason':str(error),'mutation_performed':'unconfirmed' if args.rollback or args.forward else False}));return 2
 print(json.dumps(result,indent=2));return 0
if __name__=='__main__':raise SystemExit(main())
