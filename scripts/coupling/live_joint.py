"""Opt-in 12-second live joint history: compact receipts plus bounded full states.

The existing Framework stages/validates native Fire and Fluid. This owner only
changes clock admission and transactional storage. Pruned records are custody,
not a claim of independently re-auditable full physical fields.
"""
from contextlib import closing
import base64
import hashlib
import json
from pathlib import Path
from .framework import Framework,ref
from . import feedback,source_delivery
from numeric_digest_stream import digest,sealed
from surface_sources.growth_fire_v1 import canonical,require

PROFILE='physics_sim_live12/v1'


def blob_sha(raw):
    return hashlib.sha256(raw if isinstance(raw,bytes) else raw.encode()).hexdigest()


class LiveFramework(Framework):
    MAX_WINDOWS=120

    def __init__(self,config,store):
        require(config['periods']==[[1,50],[1,200],[1,10]],'live12 fixed physical cadence')
        require(config['fluid']['grid'] in ([16]*3,[32]*3),'live12 qualified grid candidates')
        require(config.get('journal_codec')=='lossless_f64_zlib/v1','live12 lossless compressed journal')
        if 'rig_geometry' in config['fluid']:
            from . import rig_adapter
            self.FLUID_ADAPTER=rig_adapter
            require('native_packet_steps' not in config,'rig owns its native packet boundary')
        super().__init__(config,store)
        self.selected={0,4,8,12,16,32,*range(10,self.config['windows']+1,10),self.config['windows']}
        self.selected={r for r in self.selected if r<=self.config['windows']}
        require(len(self.selected)<=20,'live12 selection bound')

    def input_hashes(self):
        result=super().input_hashes()
        paths=[Path(__file__)]
        if 'rig_geometry' in self.config['fluid']:
            paths += [Path(__file__).with_name(n) for n in ('rig_adapter.py','rig_geometry.py')]
        for path in paths:result['live12/'+path.name]=hashlib.sha256(path.read_bytes()).hexdigest()
        return result

    def clock(self,request):
        return super().clock(dict(request,profile=PROFILE))

    def record(self,cp,encoded,previous=None):
        d=cp['fluid']['state']['data']
        native=self.fire.state_decode(base64.b64decode(cp['source']['checkpoint']['native_state_b64']))
        r=dict(schema='fire_fluid_live12_record/v1',revision=cp['revision'],binding_digest=self.binding_digest,
               checkpoint_digest=cp['digest'],parent_digest=cp['parent_digest'],id=cp['id'],
               previous_record_digest=None if previous is None else previous['digest'],
               payload_sha256=blob_sha(encoded),transition=cp['transition'],
               source_digest=cp['source']['digest'],fluid_digest=cp['fluid']['digest'],
               fire_tick=native['tick'],fluid_steps=d['steps'],time_s=d['time_s'],
               input_energy_j=d['input_energy_j'],input_smoke_kg=d['input_smoke_kg'])
        if previous is None:
            r['initial']=dict(initial_id=cp['id'],initial_validation=ref(cp['evidence']),
                              initial_checkpoints=[ref(cp['source']),ref(cp['fluid']['state'])])
        return sealed(r)

    def initial_record(self,db,cp):
        db.execute('CREATE TABLE live_records(revision INTEGER PRIMARY KEY,record TEXT NOT NULL)')
        encoded=db.execute('SELECT payload FROM checkpoints WHERE revision=0').fetchone()[0]
        r=self.record(cp,encoded)
        db.execute('INSERT INTO live_records VALUES(0,?)',(canonical(r).decode(),))

    def records(self,db):
        records=[]
        for revision,raw in db.execute('SELECT revision,record FROM live_records ORDER BY revision'):
            require(len(raw.encode())<=16384 and len(records)<=120,'live12 compact history bounds')
            r=json.loads(raw);previous=records[-1] if records else None
            require(r['digest']==digest(r) and r['revision']==revision==len(records) and
                    r['binding_digest']==self.binding_digest,'live12 record identity')
            require(r['previous_record_digest']==(previous['digest'] if previous else None) and
                    r['parent_digest']==(previous['checkpoint_digest'] if previous else None),'live12 chain')
            require(r['fire_tick']==5*revision and r['fluid_steps']==20*revision and
                    r['time_s']==r['fluid_steps']*.005,'live12 absolute native clocks')
            records.append(r)
        require(bool(records),'live12 genesis required')
        return records

    def payload(self,db,revision,record):
        row=db.execute('SELECT payload FROM checkpoints WHERE revision=?',(revision,)).fetchone()
        require(row is not None,'Requested full state was pruned; compact custody is not a full state')
        require(blob_sha(row[0])==record['payload_sha256'],'live12 retained payload hash')
        cp=self.journal.decode(row[0])
        require(cp['digest']==digest(cp)==record['checkpoint_digest'] and
                cp['binding_digest']==self.binding_digest and cp['revision']==revision and
                cp['parent_digest']==record['parent_digest'] and cp['transition']==record['transition'] and
                cp['id']==record['id'],'live12 retained checkpoint identity')
        self.native_checkpoint(cp['source'],cp['fluid'])
        return cp

    def bound(self,db):
        self.unchanged();metadata=db.execute('SELECT binding,head FROM metadata').fetchone()
        require(metadata and json.loads(metadata[0])==self.binding,'live12 configuration binding')
        records=self.records(db);head=len(records)-1
        require(metadata[1]==head,'live12 atomic accepted head')
        request=dict(self.base_plan,**records[0]['initial'],history=[r['transition'] for r in records[1:]])
        plan=self.clock(request);self.clock_history=request
        require(plan['completed_windows']==head and plan['accepted_id']==records[-1]['id'],'live12 clock acceptance')
        expected={r for r in self.selected if r<=head}|{head,max(0,head-1)}
        retained=set()
        for revision,raw in db.execute('SELECT revision,payload FROM checkpoints ORDER BY revision'):
            require(revision in expected and blob_sha(raw)==records[revision]['payload_sha256'],'live12 retained selection/hash')
            retained.add(revision)
        require(retained==expected,'live12 retained states missing')
        cp=self.payload(db,head,records[head])
        if head:
            parent=self.payload(db,head-1,records[head-1]);window=plan['schedule'][head-1]
            require(cp['source']['operation']['parent_digest']==parent['source']['digest'],'live12 source predecessor')
            self.fire.continuation_valid(parent['source'],cp['source'])
            source_delivery.persisted(self.config,head-1,cp['source']['frame'],cp['evidence'])
            require(cp['transition']==self.transition(parent,cp['source'],cp['fluid'],cp['evidence']),'live12 content references')
            if 'return_channel' in self.config:
                held=feedback.sample(parent,self.config,window,self.fire)
                require(cp['evidence']['held_return']==held,'live12 accepted-prior feedback')
                feedback.consumed(cp['source'],held,self.fire)
            for key,amount in (('input_energy_j','energy_transferred_j'),('input_smoke_kg','smoke_transferred_kg')):
                added=cp['fluid']['state']['data'][key]-parent['fluid']['state']['data'][key]
                import math
                require(math.isclose(added,source_delivery.expected(self.config,head-1,cp['source']['frame'])[amount],rel_tol=1e-10,abs_tol=1e-20),'live12 actual source consumption')
        owner=self
        class SparseRows:
            def __len__(self):return head+1
            def __getitem__(self,index):
                if type(index) is not int:raise ValueError('Explicit selected revisions required')
                if index<0:index+=head+1
                require(0<=index<=head,'live12 revision range')
                if index==head:return cp
                return owner.payload(db,index,records[index])
            def __iter__(self):raise ValueError('Live history is sparse; request selected full revisions explicitly')
        return SparseRows(),plan

    def plan(self,rows):
        # Used only while genesis is prepared, before its atomic first commit.
        return super().plan(rows)

    def publish(self,db,cp,operation_id,identity,receipt,encoded):
        previous=json.loads(db.execute('SELECT record FROM live_records WHERE revision=?',(cp['revision']-1,)).fetchone()[0])
        record=self.record(cp,encoded,previous)
        require(len(canonical(record))<=16384,'live12 compact record size')
        db.execute('INSERT INTO live_records VALUES(?,?)',(cp['revision'],canonical(record).decode()))
        db.execute('INSERT INTO checkpoints VALUES(?,?)',(cp['revision'],encoded))
        db.execute('INSERT INTO operations VALUES(?,?,?)',(operation_id,identity,canonical(receipt).decode()))
        db.execute('UPDATE metadata SET head=?',(cp['revision'],))
        keep=self.selected|{cp['revision'],max(0,cp['revision']-1)}
        db.execute('DELETE FROM checkpoints WHERE revision NOT IN ('+','.join('?' for _ in keep)+')',tuple(sorted(keep)))
        pages=db.execute('PRAGMA page_count').fetchone()[0];page_size=db.execute('PRAGMA page_size').fetchone()[0]
        require(pages*page_size+len(encoded)<self.journal.limit,'live12 bounded journal high-water size')

    def selected_frames(self):
        owner=self
        class Frames:
            def __len__(self):
                with closing(owner.db()) as db:return len(owner.bound(db)[0])
            def __getitem__(self,index):
                with closing(owner.db()) as db:return owner.bound(db)[0][index]
        return Frames()
