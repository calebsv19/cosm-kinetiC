"""One-way native Fire/Fluid host; core_sim owns exact scheduling and admission.

Numerical states and full SHA identities are host-verified. Native computations
are staged in memory; SQLite publishes both participants and joint head together.
This source-development host does not activate worker jobs or mutate scenes.
"""
import base64
from contextlib import closing
import hashlib
import json
import math
from pathlib import Path
import sqlite3
import subprocess
import sys
import tempfile

from evolving_atmosphere import ROOT
from . import fluid_adapter,feedback
from atmosphere_attempt import CONTROLS
from surface_sources.growth_fire_v1 import canonical,digest,sealed,require,keys,integer
from cfd_evidence import admitted_path
from .allocation import allocate_window


def ref(value):
    # Opaque bounded IDs for core_sim; full SHA identities remain in the host ledger.
    return int(digest(value)[:13],16) or 1


def load_fire(growth):
    root=admitted_path(growth)
    path=str(root/'scripts')
    if path not in sys.path:sys.path.insert(0,path)
    import fire_source.producer as producer
    require(Path(producer.__file__).resolve()==root/'scripts/fire_source/producer.py',
            'Fire adapter already imported from another checkout')
    return producer


class Framework:
    def __init__(self,config,store):
        keys(config,('growth_root','fire_worker','fluid_worker','clock_worker','source',
                     'fluid','receiver','periods','windows',*(('return_channel',) if 'return_channel' in config else ())))
        self.config=json.loads(canonical(config));self.fire=load_fire(config['growth_root'])
        self.path=admitted_path(store)/'joint.sqlite3'
        self.workers={k:admitted_path(config[k]) for k in ('fire_worker','fluid_worker','clock_worker')}
        integer(config['windows'],1,32)
        require(type(config['periods']) is list and len(config['periods'])==3,'three rational periods')
        for period in config['periods']:
            require(type(period) is list and len(period)==2,'rational period')
            for value in period:integer(value,1,10**9)
        require(config['source']['dt_s']==config['periods'][0][0]/config['periods'][0][1],'Fire cadence mismatch')
        require(config['fluid']['momentum_dt_s']==config['periods'][1][0]/config['periods'][1][1],'Fluid cadence mismatch')
        require(config['source']['origin_m'][2]==0,'horizontal floor source')
        self.fire.config_valid(config['source'])
        feedback.validate(config)
        fluid_adapter.validate(config['fluid'])
        self.binding={'configuration':self.config,'inputs':self.input_hashes()}
        self.binding_digest=digest(self.binding)
        self.base_plan={'periods':config['periods'],'windows':config['windows'],
            'domain':ref({'run':config['source']['run_id'],'branch':config['source']['branch_id']}),
            'plan_id':ref(self.binding),'plan_ref':ref({'binding':self.binding_digest}),
            'held_return':int('return_channel' in config)}
        # Admission before any solver work or database creation.
        self.clock(dict(self.base_plan,initial_id=1,initial_validation=1,initial_checkpoints=[1,2],history=[]))

    def input_hashes(self):
        files={k:v for k,v in self.workers.items()}
        for name in (*CONTROLS,'coupling/framework.py','coupling/allocation.py','coupling/fluid_adapter.py','coupling/feedback.py','coupling/velocity.py','coupling/solid_box.py','coupling/open_boundaries.py','surface_sources/receiver.py'):
            files['physics/'+name]=ROOT/'scripts'/name
        for name in ('producer.py','contract.py','wind.py'):
            files['fire/'+name]=Path(self.config['growth_root'])/'scripts/fire_source'/name
        return {name:hashlib.sha256(path.read_bytes()).hexdigest() for name,path in files.items()}

    def unchanged(self):
        require(self.input_hashes()==self.binding['inputs'],'framework input bytes changed')
        require(digest(self.binding)==self.binding_digest,'framework binding changed')

    def clock(self,request):
        with tempfile.TemporaryDirectory(prefix='fire-fluid-clock-') as directory:
            path=Path(directory)/'request.json';path.write_text(json.dumps(request,allow_nan=False))
            value=subprocess.run([str(self.workers['clock_worker']),str(path)],capture_output=True,text=True,timeout=10)
            require(value.returncode==0,value.stderr or 'core_sim clock failed')
            return json.loads(value.stdout)

    def db(self):
        admitted_path(self.path)
        self.path.parent.mkdir(parents=True,exist_ok=True)
        db=sqlite3.connect(self.path,timeout=10,isolation_level=None)
        db.execute('PRAGMA synchronous=FULL');return db

    def native_checkpoint(self,source,result):
        self.fire.bundle_valid(source)
        require(source['checkpoint']['config']==self.config['source'],'source configuration binding')
        fluid_adapter.validate(self.config['fluid'],result['state'])
        require(result['digest']==digest(result),'fluid result digest')
        require(result['state']['digest']==digest(result['state']),'fluid state digest')
        require(result['state']['worker_sha256']==self.binding['inputs']['fluid_worker'],'fluid worker identity')
        data=result['state']['data'];fields=result['fields']
        require(fields['time_s']==data['time_s'],'fluid checkpoint/field clock')
        fluid_adapter.conservation(self.config['fluid'],result)
        require(source['checkpoint']['producer']==self.fire.producer_identity(self.workers['fire_worker']),'Fire worker identity')

    def plan(self,rows):
        initial=rows[0]
        request=dict(self.base_plan,initial_id=initial['id'],initial_validation=ref(initial['evidence']),
            initial_checkpoints=[ref(initial['source']),ref(initial['fluid']['state'])],
            history=[row['transition'] for row in rows[1:]])
        return self.clock(request)

    def bound(self,db):
        self.unchanged()
        row=db.execute('SELECT binding,head FROM metadata').fetchone()
        require(row is not None and json.loads(row[0])==self.binding,'joint binding mismatch')
        rows=[json.loads(v[0]) for v in db.execute('SELECT payload FROM checkpoints ORDER BY revision')]
        require(len(rows)==row[1]+1,'joint checkpoint count')
        parent=None
        for index,cp in enumerate(rows):
            require(cp['digest']==digest(cp) and cp['revision']==index and cp['parent_digest']==parent,
                    'joint checkpoint chain integrity')
            require(cp['binding_digest']==self.binding_digest,'joint checkpoint binding')
            self.native_checkpoint(cp['source'],cp['fluid'])
            if index:
                require(cp['source']['operation']['parent_digest']==rows[index-1]['source']['digest'],
                        'source parent checkpoint identity')
                self.fire.continuation_valid(rows[index-1]['source'],cp['source'])
                if 'return_channel' in self.config:
                    held=feedback.sample(rows[index-1],self.config,{'begin':cp['evidence']['window_ticks'][0],
                        'end':cp['evidence']['window_ticks'][1]},self.fire)
                    require(cp['evidence']['held_return']==held,'persisted accepted prior return identity')
                    feedback.consumed(cp['source'],held,self.fire)
                transition=cp['transition']
                require(transition==self.transition(rows[index-1],cp['source'],cp['fluid'],cp['evidence']),
                        'joint content reference mismatch')
            parent=cp['digest']
        planned=self.plan(rows)
        require(planned['completed_windows']==row[1] and planned['accepted_id']==rows[-1]['id'],'joint accepted progress')
        fire_ticks=fluid_ticks=0
        for index,cp in enumerate(rows):
            if index:
                window=planned['schedule'][index-1]
                fire_ticks+=window['fire_steps'];fluid_ticks+=window['fluid_steps']
                require(cp['evidence']['window_ticks']==[window['begin'],window['end']] and
                        cp['evidence']['rate']==planned['rate'],'persisted exact window')
                for key,amount in (('energy_j','energy_transferred_j'),('smoke_kg','smoke_transferred_kg')):
                    added=cp['fluid']['state']['data']['input_energy_j' if key=='energy_j' else 'input_smoke_kg']-rows[index-1]['fluid']['state']['data']['input_energy_j' if key=='energy_j' else 'input_smoke_kg']
                    require(math.isclose(added,cp['source']['frame']['totals'][amount],rel_tol=1e-10,abs_tol=1e-20),
                            'persisted source consumption')
            native=self.fire.state_decode(base64.b64decode(cp['source']['checkpoint']['native_state_b64']))
            require(native['tick']==fire_ticks and cp['fluid']['state']['data']['steps']==fluid_ticks,
                    'persisted native/joint step counters')
        return rows,planned

    def initialize(self):
        with closing(self.db()) as db:
            try:
                db.execute('BEGIN IMMEDIATE')
                db.execute('CREATE TABLE IF NOT EXISTS metadata(binding TEXT,head INTEGER)')
                db.execute('CREATE TABLE IF NOT EXISTS checkpoints(revision INTEGER PRIMARY KEY,payload TEXT)')
                db.execute('CREATE TABLE IF NOT EXISTS operations(id TEXT PRIMARY KEY,identity TEXT,receipt TEXT)')
                if db.execute('SELECT head FROM metadata').fetchone():
                    rows,_=self.bound(db);db.rollback();return rows[-1]
                source=self.fire.initial(self.config['source'],self.workers['fire_worker'],'framework-initial')
                result=fluid_adapter.run(self.config['fluid'],None,[],self.workers['fluid_worker'])
                self.native_checkpoint(source,result)
                evidence={'initial':True,'source_digest':source['digest'],'state_digest':result['state']['digest'],
                    'rate':self.clock(dict(self.base_plan,initial_id=1,initial_validation=1,initial_checkpoints=[1,2],history=[]))['rate'],
                    'window_ticks':[0,0]}
                cp=sealed({'schema':'fire_fluid_joint_checkpoint/v1','revision':0,'binding_digest':self.binding_digest,
                    'parent_digest':None,'source':source,'fluid':result,'evidence':evidence,
                    'id':ref(evidence),'transition':None})
                self.plan([cp]);self.unchanged()
                db.execute('INSERT INTO checkpoints VALUES(0,?)',(canonical(cp).decode(),))
                db.execute('INSERT INTO metadata VALUES(?,0)',(canonical(self.binding).decode(),));db.commit();return cp
            except BaseException:db.rollback();raise

    def transition(self,parent,source,result,evidence):
        transition={'id':ref({'parent':parent['digest'],'source':source['digest'],'fluid':result['digest'],'evidence':evidence}),
            'predecessor':parent['id'],'window':parent['revision'],
            'checkpoints':[ref(source),ref(result['state'])],'payload':ref(source['frame']),
            'validation':ref(evidence)}
        if 'return_channel' in self.config:
            held=evidence['held_return']
            transition.update(return_checkpoint=ref(parent['fluid']['state']),return_payload=ref(held),
                return_tick=held['observation']['checkpoint_tick'],return_request=evidence['window_ticks'][1])
        return transition

    def inspect(self):
        with closing(self.db()) as db:
            rows,_=self.bound(db);return rows[-1]

    def step(self,revision,operation_id,*,before_publish=None):
        integer(revision,0,self.config['windows']-1)
        require(type(operation_id) is str and 1<=len(operation_id)<=96,'operation ID')
        identity=digest({'revision':revision,'operation':operation_id,'binding':self.binding_digest})
        with closing(self.db()) as db:
            try:
                db.execute('BEGIN IMMEDIATE');rows,plan=self.bound(db);parent=rows[-1]
                old=db.execute('SELECT identity,receipt FROM operations WHERE id=?',(operation_id,)).fetchone()
                if old:
                    require(old[0]==identity,'operation identity conflict');db.rollback();return json.loads(old[1])
                require(revision==parent['revision'],'stale joint predecessor')
                window=plan['schedule'][revision]
                held=feedback.sample(parent,self.config,window,self.fire) if 'return_channel' in self.config else None
                source=self.fire.advance_exact(parent['source'],self.workers['fire_worker'],window['fire_steps'],
                    'window-'+str(revision),self.config['periods'][0],wind=None if held is None else held['wind_input'])
                if held is not None:feedback.consumed(source,held,self.fire)
                frame=source['frame'];rate=plan['rate']
                require(math.isclose(frame['interval']['start_s'],window['begin']/rate,abs_tol=1e-14) and
                        math.isclose(frame['interval']['end_s'],window['end']/rate,abs_tol=1e-14),'source window clock')
                policy={'schema':'physics_sim_surface_source_policy/v1','consumer_id':'framework-fluid',
                    'model_id':'exact-window-uniform-source','source_config':self.config['source'],
                    'producer':source['checkpoint']['producer'],'stream_start_tick':0,'receiver':self.config['receiver']}
                steps=allocate_window(frame,policy,window)
                result=fluid_adapter.run(self.config['fluid'],parent['fluid']['state'],steps,self.workers['fluid_worker'])
                self.native_checkpoint(source,result)
                require(result['state']['data']['steps']==parent['fluid']['state']['data']['steps']+window['fluid_steps'],'fluid step count')
                evidence={'source_digest':source['digest'],'state_digest':result['state']['digest'],
                    'window_ticks':[window['begin'],window['end']],'rate':rate,'budgets':result['budgets'],
                    'transferred':{k:frame['totals'][k] for k in ('energy_transferred_j','smoke_transferred_kg')}}
                if held is not None:evidence['held_return']=held
                for key,amount in (('energy_j','energy_transferred_j'),('smoke_kg','smoke_transferred_kg')):
                    added=result['budgets'][key]['input']-parent['fluid']['budgets'][key]['input']
                    require(math.isclose(added,evidence['transferred'][amount],rel_tol=1e-10,abs_tol=1e-20),'joint consumption evidence')
                transition=self.transition(parent,source,result,evidence)
                cp=sealed({'schema':parent['schema'],'revision':revision+1,'binding_digest':self.binding_digest,
                    'parent_digest':parent['digest'],'source':source,'fluid':result,'evidence':evidence,
                    'id':transition['id'],'transition':transition})
                accepted=self.plan([*rows,cp]);require(accepted['completed_windows']==revision+1,'joint acceptance')
                receipt=sealed({'schema':'fire_fluid_joint_step/v1','revision':revision+1,'checkpoint_digest':cp['digest'],
                    'time_ticks':window['end'],'ticks_per_second':rate,'budgets':result['budgets'],
                    'fire_steps':window['fire_steps'],'fluid_steps':window['fluid_steps']})
                self.unchanged()
                require(self.path.stat().st_size+2*len(canonical(cp))<64*1024*1024,'joint journal budget')
                if before_publish:before_publish(cp)
                db.execute('INSERT INTO checkpoints VALUES(?,?)',(revision+1,canonical(cp).decode()))
                db.execute('INSERT INTO operations VALUES(?,?,?)',(operation_id,identity,canonical(receipt).decode()))
                db.execute('UPDATE metadata SET head=?',(revision+1,));db.commit();return receipt
            except BaseException:db.rollback();raise
