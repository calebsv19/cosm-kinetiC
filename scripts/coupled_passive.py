#!/usr/bin/env python3
"""Transactional source consumption for the prescribed-flow passive receiving model."""
import argparse
from contextlib import closing
import hashlib
import json
import math
from pathlib import Path
import sqlite3
from passive_atmosphere import run,validate,SCHEMA,ROOT
from surface_sources.receiver import policy_valid,source_frame,mapping,allocate
from surface_sources.growth_fire_v1 import strict_load,canonical,digest,sealed,require,identifier,integer,number,keys

def adapter_sha():
    names=('coupled_passive.py','passive_atmosphere.py','surface_sources/receiver.py','surface_sources/growth_fire_v1.py')
    return hashlib.sha256(canonical({name:hashlib.sha256((ROOT/'scripts'/name).read_bytes()).hexdigest() for name in names})).hexdigest()

class Coupled:
    def __init__(self,store,policy,properties,velocity,worker):
        self.path=Path(store)/'coupled.sqlite3';self.worker=Path(worker).resolve()
        policy=policy_valid(policy);g=policy['receiver'];n=math.prod(g['dimensions'])
        require(g['origin_m']==[0,0,0] and all(g['fluid_mask']) and policy['stream_start_tick']==0,'body-free zero-origin/start policy required')
        require(type(velocity) is list and len(velocity)==3,'constant velocity vector')
        for v in velocity:number(v,-1e6,1e6)
        zero=[0.]*n
        self.base={'schema':SCHEMA,'grid':g['dimensions'],'length_m':[a*b for a,b in zip(g['dimensions'],g['spacing_m'])],
            'properties':properties,'initial_energy_j':zero,'initial_smoke_kg':zero,'steps':[]}
        validate(dict(self.base,steps=[{'dt_s':.001,'face_velocity_m_s':[v for v in velocity for _ in range(n)],'energy_j':zero,'smoke_kg':zero}]))
        self.base=json.loads(canonical(self.base))
        self.binding=json.loads(canonical({'policy':policy,'properties':properties,'velocity_m_s':velocity,
            'worker_sha256':hashlib.sha256(self.worker.read_bytes()).hexdigest(),'adapter_sha256':adapter_sha()}));self.binding_digest=digest(self.binding)

    def db(self):
        self.path.parent.mkdir(parents=True,exist_ok=True)
        db=sqlite3.connect(self.path,timeout=130,isolation_level=None);db.execute('PRAGMA synchronous=FULL');return db

    def initialize(self):
        with closing(self.db()) as db:
            try:
                db.execute('BEGIN IMMEDIATE')
                db.execute('CREATE TABLE IF NOT EXISTS metadata (binding TEXT, head INTEGER)')
                db.execute('CREATE TABLE IF NOT EXISTS checkpoints (revision INTEGER PRIMARY KEY, payload TEXT)')
                db.execute('CREATE TABLE IF NOT EXISTS events (sequence INTEGER PRIMARY KEY, frame TEXT, receipt TEXT)')
                db.execute('CREATE TABLE IF NOT EXISTS operations (id TEXT PRIMARY KEY, digest TEXT, receipt TEXT)')
                existing=db.execute('SELECT binding FROM metadata').fetchone()
                if existing:
                    require(digest(json.loads(existing[0]))==self.binding_digest,'coupled binding mismatch');db.rollback();return self.inspect()
                cp=sealed({'schema':'physics_sim_prescribed_flow_coupled_checkpoint/v1','revision':0,'binding_digest':self.binding_digest,
                    'time_s':0.,'request':self.base,'result':None,'consumed':[],'parent_digest':None})
                db.execute('INSERT INTO checkpoints VALUES(?,?)',(0,canonical(cp).decode()))
                db.execute('INSERT INTO metadata VALUES(?,?)',(canonical(self.binding).decode(),0));db.commit();return cp
            except BaseException:db.rollback();raise

    def bound(self,db):
        require(digest(self.binding)==self.binding_digest,'receiving binding mutated')
        require(adapter_sha()==self.binding['adapter_sha256'],'receiving adapter changed')
        row=db.execute('SELECT binding,head FROM metadata').fetchone();require(row is not None,'initialize receiving history first')
        require(digest(json.loads(row[0]))==self.binding_digest,'coupled binding mismatch')
        require(hashlib.sha256(self.worker.read_bytes()).hexdigest()==self.binding['worker_sha256'],'worker changed')
        cp=json.loads(db.execute('SELECT payload FROM checkpoints WHERE revision=?',(row[1],)).fetchone()[0])
        require(cp['digest']==digest(cp) and cp['binding_digest']==self.binding_digest,'checkpoint integrity')
        return cp

    def inspect(self):
        require(self.path.is_file(),'initialize first')
        with closing(sqlite3.connect(self.path.resolve().as_uri()+'?mode=ro',uri=True)) as db:return self.bound(db)

    def admit(self,value):
        require(self.path.is_file(),'initialize receiving history first')
        frame=source_frame(value);policy=self.binding['policy']
        require(digest(frame['config'])==digest(policy['source_config']) and frame['producer']==policy['producer'],'source policy/provenance')
        mapping(frame,policy)
        with closing(self.db()) as db:
            try:
                db.execute('BEGIN IMMEDIATE');cp=self.bound(db);seq=frame['event']['sequence']
                old=db.execute('SELECT frame,receipt FROM events WHERE sequence=?',(seq,)).fetchone()
                if old:
                    require(json.loads(old[0])['digest']==frame['digest'],'source event conflict');db.rollback();return json.loads(old[1])
                last=db.execute('SELECT sequence,frame FROM events ORDER BY sequence DESC LIMIT 1').fetchone()
                require(seq==(last[0]+1 if last else 0),'source sequence gap')
                require(frame['interval']['start_tick']==(json.loads(last[1])['interval']['end_tick'] if last else 0),'source time gap')
                require(frame['interval']['start_s']>=cp['time_s'],'source behind physical history')
                require(seq<256 and self.path.stat().st_size+2*len(canonical(frame))<128*1024*1024,'journal budget')
                receipt=sealed({'schema':'physics_sim_coupled_source_admission/v1','event':frame['event'],'source_digest':frame['digest'],
                    'binding_digest':self.binding_digest,'status':'admitted_not_consumed'})
                db.execute('INSERT INTO events VALUES(?,?,?)',(seq,canonical(frame).decode(),canonical(receipt).decode()));db.commit();return receipt
            except BaseException:db.rollback();raise

    def step(self,revision,operation_id,dt,source_mode='required'):
        require(self.path.is_file(),'initialize receiving history first')
        integer(revision,0,256);identifier(operation_id);number(dt,1e-12,1)
        require(source_mode in ('required','none'),'source mode')
        identity=digest({'revision':revision,'operation_id':operation_id,'dt_s':dt,'source_mode':source_mode})
        with closing(self.db()) as db:
            try:
                db.execute('BEGIN IMMEDIATE');cp=self.bound(db)
                old=db.execute('SELECT digest,receipt FROM operations WHERE id=?',(operation_id,)).fetchone()
                if old:
                    require(old[0]==identity,'operation identity conflict');db.rollback();return json.loads(old[1])
                require(revision==cp['revision'],'stale physical revision');require(revision<64,'checkpoint count budget')
                request=json.loads(canonical(cp['request']));n=math.prod(request['grid']);zero=[0.]*n
                velocity=[v for v in self.binding['velocity_m_s'] for _ in range(n)]
                current=cp['time_s'];end=current+dt;consumed=list(cp['consumed'])
                frames=[json.loads(row[0]) for row in db.execute('SELECT frame FROM events ORDER BY sequence')]
                while current<end:
                    frame=next((f for f in frames if f['interval']['start_s']<=current<f['interval']['end_s']),None)
                    energy=zero.copy();smoke=zero.copy();until=end
                    if source_mode=='required':
                        require(frame is not None,'source interval unavailable');until=min(end,frame['interval']['end_s'])
                        plan=allocate(frame,self.binding['policy'],[current,until],operation_id)
                        for cell in plan['steps'][0]['cells']:
                            energy[cell['cell_index']]=cell['energy_transferred_j'];smoke[cell['cell_index']]=cell['smoke_transferred_kg']
                        consumed.append({'event':frame['event'],'source_digest':frame['digest'],'start_s':current,'end_s':until,
                            'energy_j':math.fsum(energy),'smoke_kg':math.fsum(smoke)})
                    else:require(not any(f['interval']['end_s']>current for f in frames),'cannot bypass admitted source')
                    require(until>current,'unresolved step precision')
                    request['steps'].append({'dt_s':until-current,'face_velocity_m_s':velocity,'energy_j':energy,'smoke_kg':smoke});current=until
                # Recompute the exact retained step history with pinned worker bytes.
                # This bounded model has prescribed flow, not a serialized CFD integrator.
                result=run(request,self.worker)
                next_cp=sealed({'schema':cp['schema'],'revision':revision+1,'binding_digest':self.binding_digest,'time_s':end,
                    'request':request,'result':result,'consumed':consumed,'parent_digest':cp['digest']})
                receipt=sealed({'schema':'physics_sim_coupled_step/v1','operation_id':operation_id,'revision':revision+1,
                    'checkpoint_digest':next_cp['digest'],'time_s':end,'budgets':result['budgets'],'status':'applied_and_checkpointed'})
                require(self.path.stat().st_size+2*(len(canonical(next_cp))+len(canonical(receipt))+4096)<128*1024*1024,'checkpoint byte budget')
                require(adapter_sha()==self.binding['adapter_sha256'],'adapter changed during step')
                require(hashlib.sha256(self.worker.read_bytes()).hexdigest()==self.binding['worker_sha256'],'worker changed during step')
                db.execute('INSERT INTO checkpoints VALUES(?,?)',(revision+1,canonical(next_cp).decode()))
                db.execute('INSERT INTO operations VALUES(?,?,?)',(operation_id,identity,canonical(receipt).decode()))
                db.execute('UPDATE metadata SET head=?',(revision+1,));db.commit();return receipt
            except BaseException:db.rollback();raise

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--config',type=Path,required=True);p.add_argument('--store',type=Path,required=True)
    sub=p.add_subparsers(dest='action',required=True);sub.add_parser('init');sub.add_parser('inspect')
    a=sub.add_parser('admit');a.add_argument('source',type=Path)
    a=sub.add_parser('step');a.add_argument('--revision',type=int,required=True);a.add_argument('--operation-id',required=True);a.add_argument('--dt',type=float,required=True);a.add_argument('--source-mode',choices=['required','none'],default='required')
    args=p.parse_args();c=strict_load(args.config);keys(c,('policy','properties','velocity_m_s','worker'))
    receiver=Coupled(args.store,c['policy'],c['properties'],c['velocity_m_s'],c['worker'])
    result=receiver.initialize() if args.action=='init' else receiver.inspect() if args.action=='inspect' else receiver.admit(strict_load(args.source)) if args.action=='admit' else receiver.step(args.revision,args.operation_id,args.dt,args.source_mode)
    print(json.dumps(result,allow_nan=False))
if __name__=='__main__':
    try:main()
    except (ValueError,OSError,sqlite3.Error) as error:raise SystemExit(str(error))
