"""Opt-in local scientific checkpoint retention, independent of native physics.

Immutable exact payloads precede a FULL-synchronous SQLite metadata commit.
All records retain hashes/accounting; only genesis, science selections and head
retain payloads. Readers lease a shared lock; publication/retirement is exclusive
and nonblocking. External consumers must copy under a lease. No pruned payload
is represented as independently re-auditable native data.
"""
from contextlib import closing, contextmanager
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import sqlite3
from surface_sources.growth_fire_v1 import require

MIB=1024**2
SCHEMA='physics_sim_bounded_checkpoints/v1'
HEX=re.compile('[0-9a-f]{64}')

def canonical(value):
    return json.dumps(value,sort_keys=True,separators=(',',':'),allow_nan=False).encode()

def sha(raw):return hashlib.sha256(raw).hexdigest()

def sync_directory(path):
    fd=os.open(path,os.O_RDONLY)
    try:os.fsync(fd)
    finally:os.close(fd)


class CheckpointStore:
    def __init__(self,root,binding,*,end_step,selected_steps,limit=2048*MIB,
                 payload_limit=256*MIB,genesis_limit=MIB,index_limit=16*MIB,
                 consumer_check=None):
        path=Path(root).absolute()
        require(path==path.resolve(),'checkpoint physical path required')
        require(type(binding) is str and HEX.fullmatch(binding),'checkpoint binding SHA256')
        require(type(end_step) is int and 1<=end_step<=640,'checkpoint horizon bound')
        pins=list(selected_steps)
        require(pins==sorted(set(pins)) and 1<=len(pins)<=6 and pins[-1]==end_step
                and all(type(v) is int and 0<v<=end_step for v in pins),'checkpoint selections')
        require(all(type(v) is int and v>0 for v in (limit,payload_limit,genesis_limit,index_limit))
                and limit<=2048*MIB and payload_limit<=256*MIB
                and genesis_limit<=MIB and index_limit<=16*MIB,'checkpoint capacity policy')
        # Worst case: five science states, prior head and pending next head.
        require(7*payload_limit+genesis_limit+2*index_limit<=limit,'checkpoint transient capacity')
        self.root=path;self.path=path/'index.sqlite3';self.blobs=path/'payloads'
        self.binding=binding;self.consumer_check=consumer_check
        self.policy=dict(schema=SCHEMA,binding=binding,end_step=end_step,
                         selected_steps=pins,limit=limit,payload_limit=payload_limit,
                         genesis_limit=genesis_limit,index_limit=index_limit)

    def create(self):
        self.root.mkdir(parents=True,exist_ok=False)
        self.blobs.mkdir()
        with (self.root/'owner.json').open('xb') as stream:
            stream.write(canonical(self.policy));stream.flush();os.fsync(stream.fileno())
        with (self.root/'lease.lock').open('xb') as stream:stream.flush();os.fsync(stream.fileno())
        with closing(sqlite3.connect(self.path)) as db:
            db.execute('PRAGMA synchronous=FULL')
            db.execute('create table records(revision integer primary key, record text not null)')
            db.commit()
        sync_directory(self.root)

    def verify_root(self):
        require(self.root==self.root.resolve() and self.root.is_dir(),'checkpoint root identity')
        for name in ('owner.json','lease.lock','index.sqlite3','payloads'):
            require(not (self.root/name).is_symlink(),'checkpoint symlink refusal')
        require((self.root/'owner.json').stat().st_size<=4096 and
                json.loads((self.root/'owner.json').read_bytes())==self.policy,'checkpoint ownership/policy')
        require(self.path.stat().st_size<=self.policy['index_limit'],'checkpoint index capacity')
        require(self.blobs.is_dir(),'checkpoint payload directory')

    @contextmanager
    def lease(self,exclusive=False):
        self.verify_root()
        fd=os.open(self.root/'lease.lock',os.O_RDONLY|os.O_NOFOLLOW)
        try:
            try:fcntl.flock(fd,(fcntl.LOCK_EX if exclusive else fcntl.LOCK_SH)|fcntl.LOCK_NB)
            except BlockingIOError:raise ValueError('checkpoint active reader/writer') from None
            self.verify_root()
            yield
        finally:os.close(fd)

    def _records(self):
        with closing(sqlite3.connect(self.path.as_uri()+'?mode=ro',uri=True)) as db:
            rows=db.execute('select revision,record from records order by revision').fetchall()
        require(len(rows)<=641,'checkpoint record count')
        records=[]
        for revision,raw in rows:
            require(len(raw.encode())<=8192,'checkpoint compact record bound')
            r=json.loads(raw);unsigned={k:v for k,v in r.items() if k!='record_sha256'}
            require(r['record_sha256']==sha(canonical(unsigned)),'checkpoint record digest')
            previous=records[-1] if records else None
            require(r['revision']==revision==len(records) and r['binding']==self.binding
                    and r['previous_record_sha256']==(previous['record_sha256'] if previous else None)
                    and r['parent_checkpoint_digest']==(previous['checkpoint_digest'] if previous else None),
                    'checkpoint record chain')
            require(type(r['step']) is int and (r['step']==0 if previous is None else
                    previous['step']<r['step']<=self.policy['end_step']),'checkpoint progress')
            require(all(type(r[k]) is str and HEX.fullmatch(r[k]) for k in
                        ('checkpoint_digest','payload_sha256')),'checkpoint record hashes')
            require(r['payload_bytes']>0 and r['payload_bytes']<=
                    self.policy['payload_limit' if revision else 'genesis_limit'],'checkpoint record payload size')
            if previous:
                require(not any(previous['step']<s<r['step'] for s in self.policy['selected_steps']),
                        'checkpoint skipped science selection')
            records.append(r)
        return records

    def _name(self,r):return '%04d-%s.blob'%(r['revision'],r['payload_sha256'])

    def _retained(self,records):
        return {self._name(r) for r in records if r['revision']==0 or
                r['step'] in self.policy['selected_steps'] or r is records[-1]}

    def _inventory(self):
        entries=list(self.blobs.iterdir())
        require(all(p.is_file() and not p.is_symlink() and
                    re.fullmatch(r'\d{4}-[0-9a-f]{64}\.blob(?:\.pending)?',p.name)
                    for p in entries),'checkpoint unowned payload entry')
        return entries

    def _read(self,r):
        p=self.blobs/self._name(r)
        require(p.exists() and not p.is_symlink(),'checkpoint payload pruned or missing')
        require(p.stat().st_size==r['payload_bytes'],'checkpoint payload size')
        raw=p.read_bytes();require(sha(raw)==r['payload_sha256'],'checkpoint payload SHA256')
        return raw

    def _reconcile(self,records):
        retained=self._retained(records)
        for r in records:
            if self._name(r) in retained:self._read(r)
        extras=[p for p in self._inventory() if p.name not in retained]
        # Fail closed when the ordinary active-consumer route is unavailable.
        if extras:
            if self.consumer_check is None:
                from verification_retention import consumers
                held=consumers(self.blobs)
            else:held=self.consumer_check(self.blobs)
            require(not held,'checkpoint payload active consumer')
        for p in extras:p.unlink()
        if extras:sync_directory(self.blobs)
        return [p.name for p in extras]

    def reconcile(self):
        with self.lease(exclusive=True):return self._reconcile(self._records())

    def records(self):
        with self.lease():return self._records()

    def read(self,revision=None):
        with self.lease():
            records=self._records();require(records,'checkpoint empty store')
            require(revision is None or type(revision) is int and 0<=revision<len(records),'checkpoint revision')
            r=records[-1] if revision is None else records[revision]
            require(self._name(r) in self._retained(records),'checkpoint payload intentionally pruned')
            return r,self._read(r)

    def audit(self):
        with self.lease():
            records=self._records();retained=self._retained(records)
            for r in records:
                if self._name(r) in retained:self._read(r)
            entries=self._inventory()
            total=sum(p.stat().st_size for p in self.root.rglob('*') if p.is_file())
            require(total<=self.policy['limit'],'checkpoint total capacity')
            return dict(records=len(records),retained_payloads=len(retained),bytes=total,
                        uncommitted_payloads=[p.name for p in entries if p.name not in retained],
                        head=None if not records else records[-1])

    def append(self,raw,*,step,checkpoint_digest,parent_checkpoint_digest,observations,expected_revision,
               fault=None):
        """Caller validates scientific values first; crash hooks are test-only."""
        require(type(raw) is bytes and raw,'checkpoint exact bytes')
        with self.lease(exclusive=True):
            records=self._records();head=records[-1] if records else None
            require(expected_revision==(head['revision'] if head else None),'checkpoint concurrent publication')
            require(parent_checkpoint_digest==(head['checkpoint_digest'] if head else None),'checkpoint parent')
            require(type(step) is int and (step==0 if not head else head['step']<step<=self.policy['end_step']),
                    'checkpoint next step')
            require(not head or not any(head['step']<s<step for s in self.policy['selected_steps']),
                    'checkpoint skipped science selection')
            require(type(checkpoint_digest) is str and HEX.fullmatch(checkpoint_digest),'checkpoint digest')
            require(len(raw)<=self.policy['payload_limit' if head else 'genesis_limit'],'checkpoint payload capacity')
            r=dict(revision=len(records),binding=self.binding,step=step,checkpoint_digest=checkpoint_digest,
                   parent_checkpoint_digest=parent_checkpoint_digest,payload_sha256=sha(raw),payload_bytes=len(raw),
                   previous_record_sha256=head['record_sha256'] if head else None,observations=observations)
            r['record_sha256']=sha(canonical(r));encoded=canonical(r)
            require(len(encoded)<=8192,'checkpoint compact observations capacity')
            self._reconcile(records)
            total=sum(p.stat().st_size for p in self.root.rglob('*') if p.is_file())
            require(total+len(raw)+2*len(encoded)+65536<=self.policy['limit'],'checkpoint pending capacity')
            require(self.path.stat().st_size+2*len(encoded)+16384<=self.policy['index_limit'],'checkpoint index headroom')
            final=self.blobs/self._name(r);pending=final.with_suffix('.blob.pending')
            with pending.open('xb') as stream:
                stream.write(raw);stream.flush();os.fsync(stream.fileno())
            os.replace(pending,final);sync_directory(self.blobs)
            if fault:fault('before_commit')
            with closing(sqlite3.connect(self.path)) as db:
                db.execute('PRAGMA synchronous=FULL');db.execute('BEGIN IMMEDIATE')
                actual=db.execute('select max(revision) from records').fetchone()[0]
                require(actual==expected_revision,'checkpoint index concurrent publication')
                db.execute('insert into records values(?,?)',(r['revision'],encoded.decode()));db.commit()
            if fault:fault('after_commit')
            self._reconcile(records+[r])
            return r
