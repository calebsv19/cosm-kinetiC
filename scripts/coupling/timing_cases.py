"""Bounded timing recipes: host partition, native emission, then live feedback.

All recipes share native clocks, source calibration, physical support and1.6s
delivery cutoff. Replay recipes do not generate or consume new Fire feedback.
"""
import array
from fractions import Fraction
import hashlib
import json
import math
import os
from pathlib import Path
import shutil

from surface_sources.growth_fire_v1 import require
from .framework import Framework
from .live_gust import retire_attempts, write
from .overnight_cases import configuration as baseline
from .transport_replay import ReplayRun
from .wind_sensitivity import cadence

CASES=('host100','host200','host400','emission020','emission100','emission200','live100')


def configuration(workers,name):
    require(name in CASES,'timing recipe')
    period={'host100':Fraction(1,10),'host400':Fraction(2,5),
            'live100':Fraction(1,10),'emission100':Fraction(1,10)}.get(name,Fraction(1,5))
    c=cadence(baseline(workers,'exchange32'),period,Fraction(16,5))
    c['source_delivery']['active_windows']=int(Fraction(8,5)/period)
    c['native_packet_steps']=20
    return c


def observation(cp,c,*,live):
    d=cp['fluid']['state']['data'];b=cp['fluid']['budgets'];o=d['inert_object']
    f=c['fluid'];gas=math.fsum(d['energy_j'])
    defect=math.fsum([gas,o['gas_to_object_j'],b['energy_j']['outflow'],
                      -b['energy_j']['inflow'],-d['input_energy_j'],-d['initial_energy_j']])
    require(abs(defect)<1e-9,'timing thermal conservation')
    row={'time_s':d['time_s'],'checkpoint_digest':cp['digest'],
         'native_state_digest':cp['fluid']['state']['digest'],
         'object_rise_k':o['temperature_k']-f['inert_object']['initial_temperature_k']+o['temperature_correction_k'],
         'gas_to_object_j':o['gas_to_object_j'],'gas_stored_j':gas,
         'gas_input_j':d['input_energy_j'],'gas_smoke_kg':math.fsum(d['smoke_kg']),
         'gas_input_smoke_kg':d['input_smoke_kg'],'thermal_defect_j':defect,
         'max_divergence_s_inv':cp['fluid']['fields']['max_divergence_s_inv'],
         'numerical_peak_bytes':cp['fluid']['fields']['numerical_peak_bytes']}
    if live and cp['revision']:
        row['source_delivery']=cp['evidence']['source_delivery']
        row['source_frame_digest']=cp['source']['frame']['digest']
        row['held_return']=cp['evidence']['held_return']
    return row


def run_case(workers,root,name,coarse_pulse,tick_pulse,*,budget_root,progress=None,replay_prefix=None):
    """Fresh root only; failed capsules/journals are deliberately preserved."""
    root=Path(root);budget_root=Path(budget_root);c=configuration(workers,name)
    live=name=='live100'
    pulse=tick_pulse if name.startswith('emission') else coarse_pulse
    period={'emission200':Fraction(1,5),'emission100':Fraction(1,10)}.get(name)
    if live:
        root.mkdir(parents=True,exist_ok=False)
        attempts=root/'native-attempts'
        os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(attempts)
        host=Framework(c,root/'run');cp=host.initialize()
        retire_attempts(attempts,root/'attempt-receipts')
    else:
        host=ReplayRun(root,c,pulse,packet_steps=20,deposition_interval=period)
        if replay_prefix is not None:
            # Explicit recovery forks the accepted immutable prefix into a new
            # root. It neither reuses failed staging nor rewrites old bindings.
            require(name=='host100','only declared failed cadence prefix may continue')
            prefix=Path(replay_prefix)
            error=json.loads((prefix.parent/'error.json').read_text())
            require(error['case']==name and error['status']=='failed-preserved' and
                    'Capture exceeded wall-time limit' in error['error'],
                    'declared capture-timeout recovery only')
            require(os.environ.get('TZ')=='UTC','startup recovery needs process-local UTC')
            require(json.loads((prefix/'config.json').read_text())==c,'prefix configuration unchanged')
            prior=ReplayRun(prefix,c,pulse,packet_steps=20).audit()
            require(0<prior['revision']<c['windows'],'incomplete accepted prefix')
            root.mkdir(parents=True,exist_ok=False)
            files=['replay.sqlite3','binding.json','observations.json']
            files += [p.name for p in prefix.glob('fields-*.bin')]
            identities={}
            for filename in files:
                source=prefix/filename
                require(not source.is_symlink() and source.is_file(),'regular prefix input')
                before=hashlib.sha256(source.read_bytes()).hexdigest()
                shutil.copyfile(source,root/filename)
                require(hashlib.sha256(source.read_bytes()).hexdigest()==before and
                        hashlib.sha256((root/filename).read_bytes()).hexdigest()==before,
                        'immutable accepted prefix copy')
                identities[filename]=before
            cp=host.audit()
            require(cp['digest']==prior['digest'],'copied prefix head identity')
            write(root/'continuation.json',{'schema':'explicit_native_replay_prefix_continuation/v1',
                'source_root':str(prefix.resolve()),'accepted_head_digest':cp['digest'],
                'accepted_revision':cp['revision'],'copied_sha256':identities,
                'environment_change':'process-local TZ=UTC plus normal local execution access; timezone alone did not fix sampled startup notification wait',
                'old_failure_and_staging':'preserved; no historical receipt or binding rewritten'})
        else:
            cp=host.initialize()
    write(root/'config.json',c)
    rows=(json.loads((root/'observations.json').read_text()) if replay_prefix is not None else
          [observation(cp,c,live=live)])
    require(len(rows)==cp['revision']+1 and rows[-1]['checkpoint_digest']==cp['digest'],
            'complete observed prefix')
    for revision in range(cp['revision'],c['windows']):
        allocated=sum(p.stat().st_blocks*512 for p in budget_root.rglob('*') if p.is_file())
        require(allocated<4*1024**3 and shutil.disk_usage(root).free>5*1024**3,
                'timing study storage reserve')
        if progress:progress(name,revision,c['windows'],'native-step')
        if live:
            host.step(revision,name+'-'+str(revision));cp=host.inspect()
            retire_attempts(attempts,root/'attempt-receipts')
        else:
            cp=host.advance(revision,int(Fraction(*c['periods'][2])/Fraction(*c['periods'][1])))
        row=observation(cp,c,live=live);rows.append(row)
        require(row['numerical_peak_bytes']<=512*1024**2,'timing native allocation')
        if any(abs(row['time_s']-t)<1e-12 for t in (.8,1.6,2.4,3.2)):
            with (root/('fields-%04d.bin'%round(row['time_s']*1000))).open('xb') as stream:
                for key in ('face_velocity_m_s','energy_j','smoke_kg'):
                    array.array('d',cp['fluid']['state']['data'][key]).tofile(stream)
        write(root/'observations.json',rows)
        if progress:progress(name,revision+1,c['windows'],'published')
    if progress:progress(name,c['windows'],c['windows'],'independent-reopen')
    reopened=(Framework(c,root/'run').inspect() if live else
              ReplayRun(root,c,pulse,packet_steps=20,deposition_interval=period).audit())
    require(reopened['digest']==cp['digest'],'timing independent restart')
    if not live:
        require(math.isclose(rows[-1]['gas_input_j'],pulse['totals']['energy_transferred_j'],rel_tol=1e-12),
                'timing fixed heat budget')
    summary={'status':'completed','recipe':name,'head_digest':cp['digest'],
             'independent_reopen':True,'grid':32,'horizon_s':3.2,
             'live_feedback':live,'source_pulse_digest':None if live else pulse['digest'],
             'deposition_interval_s':float(period) if period else (None if live else pulse['frames'][0]['config']['dt_s'] if name=='emission020' else .2),
             'final':rows[-1]}
    write(root/'summary.json',summary)
    return summary
