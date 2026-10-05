#!/usr/bin/env python3
"""Transactional source consumption with native evolving-flow checkpoint continuation."""
import argparse
from contextlib import closing
import hashlib
import json
import math
from pathlib import Path
import sqlite3
import subprocess
from coupled_passive import Coupled as Prescribed
from evolving_atmosphere import run,validate,SCHEMA,ROOT
from surface_sources.receiver import policy_valid,source_frame,mapping,allocate
from surface_sources.growth_fire_v1 import strict_load,canonical,digest,sealed,require,identifier,integer,number,keys

def adapter_sha():
    names=('coupled_atmosphere.py','evolving_atmosphere.py','coupled_passive.py','passive_atmosphere.py','surface_sources/receiver.py','surface_sources/growth_fire_v1.py')
    return hashlib.sha256(canonical({name:hashlib.sha256((ROOT/'scripts'/name).read_bytes()).hexdigest() for name in names})).hexdigest()

class CoupledAtmosphere(Prescribed):
    def __init__(self,store,policy,properties,velocity,dt,worker):
        self.path=Path(store)/'coupled.sqlite3';self.worker=Path(worker).resolve()
        policy=policy_valid(policy);grid=policy['receiver']
        require(grid['origin_m']==[0,0,0] and all(grid['fluid_mask']) and policy['stream_start_tick']==0,'body-free zero-origin/start policy')
        self.base=json.loads(canonical({'grid':grid['dimensions'],'length_m':[a*b for a,b in zip(grid['dimensions'],grid['spacing_m'])],
            'properties':properties,'momentum_dt_s':dt,'initial_face_velocity_m_s':velocity}))
        validate(dict(self.base,schema=SCHEMA,state=None,steps=[]))
        self.binding=json.loads(canonical({'policy':policy,'configuration':self.base,'worker_sha256':hashlib.sha256(self.worker.read_bytes()).hexdigest(),'adapter_sha256':adapter_sha()}))
        self.binding_digest=digest(self.binding)
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
                cp=sealed({'schema':'physics_sim_evolving_coupled_checkpoint/v1','revision':0,'binding_digest':self.binding_digest,
                    'time_s':0.,'configuration':self.base,'result':run(dict(self.base,schema=SCHEMA,state=None,steps=[]),self.worker),'consumed':[],'parent_digest':None})
                require(adapter_sha()==self.binding['adapter_sha256'] and hashlib.sha256(self.worker.read_bytes()).hexdigest()==self.binding['worker_sha256'],'receiving bytes changed during init')
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
                ticks=0 if cp['result'] is None else cp['result']['state']['data']['steps']
                clock=ticks*self.base['momentum_dt_s'];start=frame['interval']['start_s']
                require(start>=clock or math.isclose(start,clock,rel_tol=1e-13,abs_tol=1e-15),'source behind physical history')
                require(seq<256 and self.path.stat().st_size+2*len(canonical(frame))<128*1024*1024,'journal budget')
                receipt=sealed({'schema':'physics_sim_coupled_source_admission/v1','event':frame['event'],'source_digest':frame['digest'],
                    'binding_digest':self.binding_digest,'status':'admitted_not_consumed'})
                db.execute('INSERT INTO events VALUES(?,?,?)',(seq,canonical(frame).decode(),canonical(receipt).decode()));db.commit();return receipt
            except BaseException:db.rollback();raise

    def step(self,revision,operation_id,dt,source_mode='required'):
        require(self.path.is_file(),'initialize receiving history first');integer(revision,0,64);identifier(operation_id);number(dt,1e-6,1)
        require(source_mode in ('required','none'),'source mode')
        h=self.base['momentum_dt_s'];count=round(dt/h)
        require(1<=count<=256 and math.isclose(count*h,dt,rel_tol=1e-12,abs_tol=1e-12),'duration must be integral fixed momentum steps')
        identity=digest({'revision':revision,'operation_id':operation_id,'dt_s':dt,'source_mode':source_mode})
        with closing(self.db()) as db:
            try:
                db.execute('BEGIN IMMEDIATE');cp=self.bound(db)
                old=db.execute('SELECT digest,receipt FROM operations WHERE id=?',(operation_id,)).fetchone()
                if old:
                    require(old[0]==identity,'operation identity conflict');db.rollback();return json.loads(old[1])
                require(revision==cp['revision'] and revision<64,'stale revision/checkpoint bound')
                state=None if cp['result'] is None else cp['result']['state'];start=0 if state is None else state['data']['steps']
                n=math.prod(self.base['grid']);frames=[json.loads(row[0]) for row in db.execute('SELECT frame FROM events ORDER BY sequence')]
                steps=[];consumed=list(cp['consumed'])
                for tick in range(start,start+count):
                    current=tick*h;end=(tick+1)*h;energy=[0.]*n;smoke=[0.]*n
                    while current<end:
                        if source_mode=='none':
                            require(not any(f['interval']['end_s']>current for f in frames),'cannot bypass admitted source');current=end;break
                        frame=next((f for f in frames if f['interval']['start_s']<=current<f['interval']['end_s']),None)
                        require(frame is not None,'source interval unavailable');until=min(end,frame['interval']['end_s'])
                        plan=allocate(frame,self.binding['policy'],[current,until],operation_id)
                        for cell in plan['steps'][0]['cells']:
                            q=cell['cell_index'];energy[q]+=cell['energy_transferred_j'];smoke[q]+=cell['smoke_transferred_kg']
                        consumed.append({'event':frame['event'],'source_digest':frame['digest'],'start_s':current,'end_s':until,
                            'energy_j':plan['steps'][0]['totals']['energy_transferred_j'],'smoke_kg':plan['steps'][0]['totals']['smoke_transferred_kg']})
                        require(until>current,'step precision');current=until
                    steps.append({'energy_j':energy,'smoke_kg':smoke})
                result=run(dict(self.base,schema=SCHEMA,state=state,steps=steps),self.worker)
                next_cp=sealed({'schema':cp['schema'],'revision':revision+1,'binding_digest':self.binding_digest,'configuration':self.base,
                    'time_s':result['fields']['time_s'],'result':result,'consumed':consumed,'parent_digest':cp['digest']})
                receipt=sealed({'schema':'physics_sim_evolving_coupled_step/v1','operation_id':operation_id,'revision':revision+1,
                    'checkpoint_digest':next_cp['digest'],'time_s':next_cp['time_s'],'native_steps_advanced':count,'budgets':result['budgets'],'status':'applied_and_checkpointed'})
                require(self.path.stat().st_size+2*(len(canonical(next_cp))+len(canonical(receipt))+4096)<128*1024*1024,'checkpoint byte budget')
                require(adapter_sha()==self.binding['adapter_sha256'] and hashlib.sha256(self.worker.read_bytes()).hexdigest()==self.binding['worker_sha256'],'receiving bytes changed during step')
                db.execute('INSERT INTO checkpoints VALUES(?,?)',(revision+1,canonical(next_cp).decode()))
                db.execute('INSERT INTO operations VALUES(?,?,?)',(operation_id,identity,canonical(receipt).decode()))
                db.execute('UPDATE metadata SET head=?',(revision+1,));db.commit();return receipt
            except BaseException:db.rollback();raise

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--config',type=Path,required=True);p.add_argument('--store',type=Path,required=True)
    sub=p.add_subparsers(dest='action',required=True);sub.add_parser('init');sub.add_parser('inspect');a=sub.add_parser('admit');a.add_argument('source',type=Path)
    a=sub.add_parser('step');a.add_argument('--revision',type=int,required=True);a.add_argument('--operation-id',required=True);a.add_argument('--dt',type=float,required=True);a.add_argument('--source-mode',choices=['required','none'],default='required')
    args=p.parse_args();c=strict_load(args.config);keys(c,('policy','properties','initial_face_velocity_m_s','momentum_dt_s','worker'))
    receiver=CoupledAtmosphere(args.store,c['policy'],c['properties'],c['initial_face_velocity_m_s'],c['momentum_dt_s'],c['worker'])
    result=receiver.initialize() if args.action=='init' else receiver.inspect() if args.action=='inspect' else receiver.admit(strict_load(args.source)) if args.action=='admit' else receiver.step(args.revision,args.operation_id,args.dt,args.source_mode)
    print(json.dumps(result,allow_nan=False))
if __name__=='__main__':
    try:main()
    except (ValueError,OSError,sqlite3.Error,subprocess.SubprocessError) as error:raise SystemExit(str(error))
