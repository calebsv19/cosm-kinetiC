"""Numerical screens, never a declaration of validated CFD or surface forces."""
import math
import statistics

DEFAULT_CRITERIA = {
    'window_s': 1.0, 'min_samples_per_window': 5, 'min_flow_through_times': 3.0,
    'mean_drift_over_inlet_speed': .01, 'range_over_inlet_speed': .02,
    'relative_flux_imbalance': 1e-4, 'refinement_change_over_inlet_speed': .02,
}


def steady_screen(series, history, speed, length, criteria=None):
    c = dict(DEFAULT_CRITERIA, **(criteria or {}))
    if not all(isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v) and v > 0 for v in c.values()):
        raise ValueError('criteria must be positive finite numbers')
    if not math.isfinite(speed) or not math.isfinite(length) or speed <= 0 or length <= 0: raise ValueError('positive speed and length required')
    result = {'schema':'physics_sim_steady_screen_v1', 'criteria':c,
              'status':'insufficient_evidence','reasons':[], 'metrics':{},
              'scope':'two trailing windows; steady numerical screen, not statistical stationarity or physical validation'}
    if not series: result['reasons'].append('no_wake_samples'); return result
    times=[s['time_s'] for s in series]
    if any(not math.isfinite(t) for t in times) or any(b <= a for a,b in zip(times,times[1:])):
        result['reasons'].append('invalid_sample_times'); return result
    end=times[-1]; window=c['window_s']; start=end-2*window
    result['window_end_s']=end
    if start < c['min_flow_through_times']*length/speed-1e-8:
        result['reasons'].append('insufficient_warmup_before_windows')
    groups=[[s for s in series if end-(2-k)*window-1e-8 <= s['time_s'] < end-(1-k)*window-1e-8]
            for k in range(2)]
    # Include the final sample only in the second window.
    groups[1].append(series[-1])
    for g in groups:
        if len(g)<c['min_samples_per_window'] or g[-1]['time_s']-g[0]['time_s']<.75*window or any(b['time_s']-a['time_s']>window/2 for a,b in zip(g,g[1:])):
            result['reasons'].append('insufficient_window_coverage'); break
    if result['reasons']: return result
    for station in ('near_wake','far_wake'):
        for field in ('central_mean_vx_m_s','section_mean_vx_m_s','reverse_flow_fraction'):
            try: values=[[s[station][field] for s in g] for g in groups]
            except (KeyError,TypeError): result['reasons'].append('missing_wake_metric'); continue
            if any(v is None or not math.isfinite(v) for group in values for v in group):
                result['reasons'].append('nonfinite_or_empty_wake_aperture'); continue
            scale=1.0 if field=='reverse_flow_fraction' else speed
            means=[statistics.fmean(v) for v in values]
            drift=abs(means[1]-means[0])/scale
            spread=max(max(v)-min(v) for v in values)/scale
            result['metrics'][station+'.'+field]={'window_means':means,'normalized_mean_drift':drift,
                'normalized_max_range':spread,'window_stddev':[statistics.pstdev(v) for v in values]}
            if drift > c['mean_drift_over_inlet_speed']: result['reasons'].append('wake_mean_drift')
            if spread > c['range_over_inlet_speed']: result['reasons'].append('wake_fluctuation')
    observed=[h for h in history if start-1e-8 <= h['time_s'] <= end+1e-8]
    if not observed or observed[0]['time_s']>start+window/4 or observed[-1]['time_s']<end-1e-6 or any(b['time_s']-a['time_s']>window/4 for a,b in zip(observed,observed[1:])): result['reasons'].append('missing_solver_history')
    for h in observed:
        health=h['health']; b=health.get('conservation',{})
        if health.get('projection_status')!='converged': result['reasons'].append('pressure_not_converged')
        if health.get('velocity_clamped_cells',0) or health.get('skipped_clusters',0): result['reasons'].append('clamped_or_skipped_solver')
        if not b.get('available'): result['reasons'].append('conservation_unavailable'); continue
        flux=b.get('outward_face_flux_m3_s_minmax_xyz',[])
        net=b.get('net_outward_volume_flux_m3_s')
        if len(flux)!=6 or net is None or not all(math.isfinite(x) for x in [net,*flux]):
            result['reasons'].append('invalid_flux'); continue
        incoming=sum(-f for f in flux if f<0)
        if incoming<=1e-12: result['reasons'].append('no_through_flow')
        elif abs(net)/incoming>c['relative_flux_imbalance']: result['reasons'].append('flux_imbalance')
    result['reasons']=sorted(set(result['reasons']))
    result['status']='steady_window_screen_pass' if not result['reasons'] else 'not_steady_or_numerically_unresolved'
    return result


def refinement_screen(cases, kind, tolerance=.02):
    if kind not in ('spatial','temporal','outlet'): raise ValueError('unknown study kind')
    if not math.isfinite(tolerance) or tolerance<=0: raise ValueError('positive finite tolerance required')
    result={'schema':'physics_sim_refinement_screen_v1','kind':kind,'status':'insufficient_evidence',
            'tolerance_over_inlet_speed':tolerance,'reasons':[], 'changes':[],
            'physical_outlet_status':'not_qualified','force_measurement_ready':False}
    required=2 if kind=='outlet' else 3
    if len(cases)<required: result['reasons'].append('need_'+str(required)+'_independent_levels')
    if not cases: return result
    def identity(c):
        request=c['request']; setup=c['study_setup']
        key=[c['shape'],c['fluid'],request['worker_sha256'],request['geometry_assets'],
             setup['object_center_m'],setup['speed_m_s'],c['duration_s'],c['steady_screen']['criteria'],
             setup['wake_positions_m'],request['qualification_mode'],request['solver_iterations'],
             c['reference']['characteristic_length_m'],setup['dimensions_m'][1:],
             setup['sampling_interval_s'],c['snapshot']['health']['projection_operator']]
        if kind!='temporal': key.append(c['dt_s'])
        if kind!='outlet': key.append(setup['dimensions_m'])
        if kind!='spatial': key.append(c['validation']['voxel_size_m'])
        return key
    try:
        if any(identity(c)!=identity(cases[0]) for c in cases[1:]): result['reasons'].append('unmatched_controls_or_worker')
        if any(c['steady_screen']['status']!='steady_window_screen_pass' for c in cases): result['reasons'].append('unsettled_or_unresolved_case')
        level=lambda c: c['validation']['voxel_size_m'] if kind=='spatial' else (c['dt_s'] if kind=='temporal' else c['study_setup']['dimensions_m'][0])
        if any(not math.isfinite(level(c)) or level(c)<=0 for c in cases): raise ValueError('invalid level')
        ordered=sorted(cases,key=level,reverse=kind!='outlet')
        if len(set(level(c) for c in ordered))!=len(ordered): result['reasons'].append('duplicate_levels')
        for a,b in zip(ordered,ordered[1:]):
            differences={}
            for station in ('near_wake','far_wake'):
                key=station+'.central_mean_vx_m_s'
                av=a['steady_screen']['metrics'][key]['window_means'][-1]
                bv=b['steady_screen']['metrics'][key]['window_means'][-1]
                if not all(math.isfinite(x) for x in (av,bv)): raise ValueError('nonfinite wake mean')
                differences[station]=abs(av-bv)/a['study_setup']['speed_m_s']
            result['changes'].append({'from_level':level(a),'to_level':level(b),
                                      'normalized_changes':differences})
        for station in ('near_wake','far_wake'):
            changes=[c['normalized_changes'][station] for c in result['changes']]
            if changes and changes[-1]>tolerance: result['reasons'].append('refinement_difference_exceeds_tolerance')
            if len(changes)>1 and any(b>a+1e-12 for a,b in zip(changes,changes[1:])):
                result['reasons'].append('differences_not_decreasing')
    except (KeyError,TypeError,ValueError): result['reasons'].append('missing_study_evidence')
    result['reasons']=sorted(set(result['reasons']))
    if not result['reasons']: result['status']='numerical_sensitivity_screen_pass'
    elif result['changes']: result['status']='not_demonstrated'
    result['scope']='finite-level numerical screen using nearest-cell wake planes; no observed-order/GCI estimate or validated outlet pressure'
    return result
