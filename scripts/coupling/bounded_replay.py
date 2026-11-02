"""Explicit bounded-retention adapter for the existing one-way ReplayRun.

Native requests, full result acceptance and conservation checks are inherited.
Reopening validates the complete head; audit validates all retained full states.
Pruned windows retain compact custody/accounting only, not full-field proof.
This does not extend any native profile's admitted grid, clock or horizon.
"""
import hashlib
import json
import os
from pathlib import Path
from numeric_digest_stream import sealed
from surface_sources.growth_fire_v1 import require
from .checkpoint_store import CheckpointStore
from .transport_replay import ReplayRun
from . import fluid_adapter
from .live_gust import retire_attempts


class BoundedReplayRun(ReplayRun):
    _bounded_retention=True
    def __init__(self,root,config,pulse,*,end_step,selected_steps,**options):
        # New long128 campaigns must have even their minimum cumulative work
        # admitted before source mapping, root creation or any native call.
        # Existing stores remain available for read-only forensic inspection.
        if (config['fluid'].get('capacity_profile') in ('scalar-wake128/v1','scalar-wake128/v2')
                and not (Path(root)/'owner.json').exists()):
            from .scalar_work_budget import require_horizon_work
            require_horizon_work(config['fluid'],end_step)
        super().__init__(root,config,pulse,**options)
        require(self.codec.compressed,'bounded replay compressed codec')
        scripts=Path(__file__).resolve().parents[1]
        self.files += [scripts/'coupling/checkpoint_store.py',Path(__file__).resolve(),
                       scripts/'field_packet.py',scripts/'coupling/scalar_work_budget.py']
        self.pins={str(p):hashlib.file_digest(p.open('rb'),'sha256').hexdigest() for p in self.files}
        policy=dict(end_step=end_step,selected_steps=list(selected_steps))
        self.binding=sealed({**self.binding,'inputs':self.pins,'bounded_retention':policy})
        self.store=CheckpointStore(self.root,self.binding['digest'],**policy)
        self.path=self.store.path

    def initialize(self):
        self.unchanged();self.store.create()
        with (self.root/'binding.json').open('x') as stream:
            json.dump(self.binding,stream);stream.write('\n');stream.flush();os.fsync(stream.fileno())
        os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(self.root/'native-attempts')
        result=fluid_adapter.run(self.config['fluid'],None,[],self.worker,
                                **({'io_format':self.io_format} if self.io_format else {}))
        return self.publish(result,None,{'energy_j':0.,'smoke_kg':0.})

    def _decode(self,record,raw):
        cp=self.codec.decode(raw);self.check(cp)
        require(cp['revision']==record['revision'] and cp['digest']==record['checkpoint_digest']
                and cp['parent_digest']==record['parent_checkpoint_digest']
                and cp['fluid']['state']['data']['steps']==record['step'],
                'bounded replay checkpoint/index identity')
        return cp

    def inspect(self):
        self.unchanged()
        return self._decode(*self.store.read())

    def publish(self,result,parent,delivered):
        self.unchanged()
        cp=sealed(dict(schema='physics_sim_transport_replay_checkpoint/v1',binding=self.binding['digest'],
                       revision=0 if parent is None else parent['revision']+1,
                       parent_digest=None if parent is None else parent['digest'],
                       delivered=delivered,fluid=result))
        self.check(cp)
        fields=result['fields']
        observations=dict(time_s=result['state']['data']['time_s'],delivered=delivered,
                          scalar_work_cells=result['state']['data']['scalar_work_cells'],
                          native_result_digest=result['digest'],native_state_digest=result['state']['digest'],
                          budgets=result['budgets'])
        for key in ('kinetic_stored_j','kinetic_ledger_j','max_divergence_s_inv',
                    'projection_relative_residual','numerical_peak_bytes',
                    'packet_momentum_courant_max','momentum_courant_limit',
                    'packet_scalar_cycles_max','scalar_cycles_limit','scalar_work_limit'):
            if key in fields:observations[key]=fields[key]
        self.store.append(self.codec.encode(cp),step=result['state']['data']['steps'],
                          checkpoint_digest=cp['digest'],parent_checkpoint_digest=cp['parent_digest'],
                          expected_revision=None if parent is None else parent['revision'],
                          observations=observations)
        retire_attempts(self.root/'native-attempts',self.root/'attempt-receipts')
        return cp

    def advance(self,revision,count):
        # Refuse an inadmissible publication before invoking the native worker.
        records=self.store.records();require(records,'bounded replay initialized')
        first=records[-1]['step'];last=first+count
        require(type(count) is int and count>0 and last<=self.store.policy['end_step']
                and not any(first<s<last for s in self.store.policy['selected_steps']),
                'bounded replay selection/horizon')
        return super().advance(revision,count)

    def audit(self):
        self.unchanged();summary=self.store.audit();records=self.store.records()
        for r in records:
            if r['revision']==0 or r['step'] in self.store.policy['selected_steps'] or r is records[-1]:
                self._decode(*self.store.read(r['revision']))
        require(not summary['uncommitted_payloads'],'bounded replay needs reconciliation')
        return self.inspect()
