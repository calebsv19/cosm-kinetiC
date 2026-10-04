"""Bounded 3D verification scene policy and explicit independent reference assessment."""
import math
MODES = {
    'cfd_box_3d': 'steady_box_duct',
    'cfd_obstacle_3d': 'steady_obstacle_duct',
    'cfd_duct_3d': 'steady_duct',
    'cfd_manufactured_3d': 'manufactured_transient',
    'cfd_open_duct_3d': 'steady_open_duct',
    'cfd_wall_stokes_3d': 'wall_stokes_transient',
    'cfd_wall_transport_3d': 'wall_transport_transient',
    'cfd_pressure_startup_3d': 'pressure_startup',
    'cfd_open_wall_transient_3d': 'open_wall_stokes_transient',
}
TEMPLATES = tuple(MODES)
TRANSIENT_MODES = ('wall_stokes_transient','wall_transport_transient','pressure_startup','open_wall_stokes_transient')
MODEL = 'incompressible_cartesian3d_v1'


def scene_parameters(template, lengths, channel, number, error):
    mode = MODES[template]
    driven = mode in ('steady_duct', 'steady_open_duct', 'pressure_startup','steady_obstacle_duct','steady_box_duct')
    params = {'solve_mode': mode}
    if driven:
        params['volume_flow_m3_s'] = .002 * lengths[1] * lengths[2]
    if mode=='steady_obstacle_duct':params['center_x_m']=2.
    if mode=='steady_box_duct':
        params.update(body_min_m=None,body_max_m=None)
    if channel is not None:
        if not isinstance(channel, dict) or set(channel) - set(params):
            raise error('unsupported bounded 3D parameters')
        params.update(channel)
    if params['solve_mode'] != mode:
        raise error('3D template fixes its verification solve mode')
    if driven:
        params['volume_flow_m3_s'] = number(params['volume_flow_m3_s'], 1e-9, 1e4)
    if mode == 'open_wall_stokes_transient' and (lengths[0] < 4 or abs(lengths[0]/2-round(lengths[0]/2)) > 1e-12):
        raise error('open wall reference requires Lx=4,6,8,... m with fixed 4 m wavelength')
    if mode=='steady_obstacle_duct':
        if lengths[1:]!=[2,2]:raise error('bounded cube duct requires Y=Z=2 m')
        params['center_x_m']=number(params['center_x_m'],.5,lengths[0]-.5)
    if mode=='steady_box_duct':
        if lengths[1:]!=[2,2]:raise error('bounded box duct requires Y=Z=2 m')
        for key in ('body_min_m','body_max_m'):
            values=params[key]
            if not isinstance(values,list) or len(values)!=3:
                raise error(key+' requires three explicit physical bounds in metres')
            params[key]=[number(v,0,lengths[a]) for a,v in enumerate(values)]
        if any(hi<=lo for lo,hi in zip(params['body_min_m'],params['body_max_m'])):
            raise error('stationary box requires positive extents')
    return dict(params, solver_model=MODEL, dimensions_m=lengths)


def finite(value):
    return not isinstance(value,bool) and isinstance(value,(int,float)) and math.isfinite(value) and value>=0

def _checks(ref, limits):
    checks = {}
    for key, limit in limits:
        value = ref.get(key)
        checks[key] = {'value': value, 'limit': limit, 'status':
                       'not_established' if not finite(value) else 'passed' if value <= limit else 'failed'}
    return checks


def assess_3d(status, comparisons=None):
    health = status.get('health', {})
    r = health.get('linear_relative_residual'); d = health.get('max_abs_divergence_s_inv')
    numerical = (status.get('state') != 'failed' and health.get('projection_status') == 'converged'
                 and finite(r) and r <= 1e-11 and finite(d) and d < 1e-8)
    mode = status.get('solve_mode')
    if mode in ('steady_obstacle_duct','steady_box_duct'):
        energy=status.get('energy_budget',{});force=status.get('boundary_force_budget',{})
        checks=_checks({'energy':energy.get('physical_relative_imbalance'),'momentum':force.get('momentum_relative_residual'),
                        'flux':health.get('flux_conservation_relative_error')}, [('energy',.03),('momentum',.02),('flux',1e-9)])
        vals=[v['status'] for v in checks.values()]
        return {'schema':'physics_sim_cfd_run_assessment_v1','numerical_status':'passed' if numerical else 'failed' if status.get('state')=='failed' else 'not_established',
                'reference_accuracy':{'status':'not_established','checks':{},'scope':'requires independent converged same-domain 3D force reference'},
                'physical_budget':{'status':'failed' if 'failed' in vals else 'passed' if numerical and all(v=='passed' for v in vals) else 'not_established','checks':checks},
                'physical_accuracy_certified':False,'status':'not_established','limitations':['stationary aligned box creeping Stokes only' if mode=='steady_box_duct' else 'stationary aligned cube creeping Stokes only','refinement and boundary-distance are separate multi-run gates']}

    transient = mode in TRANSIENT_MODES
    startup = mode == 'pressure_startup'
    is_open = mode == 'steady_open_duct'
    qualification = status.get('qualification', {})
    ref = qualification.get('transient_reference_gate' if transient else 'duct_reference_gate', {})
    if transient:
        limits = [('velocity_relative_l2', .01 if startup else .03),
                  ('pressure_relative_error', .01 if startup else .03),
                  ('max_wall_relative_error', .02 if startup else .05),
                  ('energy_relative_error', .02 if startup else .05),
                  ('energy_imbalance', .02 if startup else .05)]
        if startup: limits.append(('volume_flow_relative_error', .01))
    else:
        limits = [('velocity_relative_l2', .01), ('pressure_relative_error', .01),
                  ('volume_flow_relative_error', .01), ('max_wall_relative_error', .02), ('energy_relative_error', .02)]
        if is_open: limits += [('energy_imbalance', .02), ('flux_relative_error', 1e-10)]
    checks = _checks(ref, limits)
    if transient:
        ready = ref.get('reference_time_resolved')
        checks['reference_time_resolved'] = {'value': ready, 'status': 'passed' if ready is True else 'not_established'}
    values = [v['status'] for v in checks.values()]
    reference = ('not_applicable' if ref.get('applicable') is not True else
                 'not_established' if transient and ref.get('reference_time_resolved') is not True else
                 'failed' if 'failed' in values else 'passed' if numerical and all(v == 'passed' for v in values) else 'not_established')
    scopes = {'pressure_startup': 'pressure-driven startup from rest with independent rectangular Fourier reference; impulsive unresolved interval excluded',
              'open_wall_stokes_transient': 'three-component unsteady Stokes with independently prescribed natural tractions; fixed X wavelength',
              'wall_stokes_transient': 'three-component independently forced wall Stokes transient',
              'wall_transport_transient': 'three-component independently forced wall Navier-Stokes transient',
              'steady_open_duct': 'stationary straight duct with analytic developed inlet and vector-Laplacian outlet'}
    harmonic = qualification.get('harmonic_reference', {})
    harmonic_checks = {}
    if transient and not startup and harmonic.get('available') is True:
        for component in ('velocity', 'pressure'):
            for key, value in _checks(harmonic.get(component, {}), [('amplitude_relative_error', .03), ('phase_error_degrees', 1)]).items():
                harmonic_checks[component + '_' + key] = value
    hv = [v['status'] for v in harmonic_checks.values()]
    harmonic_status = ('not_applicable' if not transient or startup else 'not_established' if not hv else
                       'failed' if 'failed' in hv else 'passed' if numerical and all(v == 'passed' for v in hv) else 'not_established')
    return {'schema': 'physics_sim_cartesian3d_assessment_v1', 'run_id': status.get('run_id'),
            'numerical_status': 'passed' if numerical else 'failed' if status.get('state') == 'failed' else 'not_established',
            'reference_accuracy': {'status': reference, 'checks': checks if ref.get('applicable') is True else {},
                                   'scope': scopes.get(mode, 'fully developed periodic-X rectangular duct only')},
            'harmonic_accuracy': {'status': harmonic_status, 'checks': harmonic_checks,
                                  'scope': 'first-period accepted-step least squares; independent amplitude and phase errors'},
            'refinement_comparisons': comparisons or {}, 'physical_accuracy_certified': False,
            'scope': 'per-run reference checks do not certify multi-run convergence/outlet extensions, body forces or arbitrary CFD'}


def comparison_metrics(status):
    result={}
    for group,keys in [('physics',('pressure_drop_pa',)),('health',('volume_flux_m3_s','velocity_amplitude_projection','pressure_amplitude_projection_pa','velocity_orthogonal_relative_error')),('energy_budget',('kinetic_energy_j','physical_strain_dissipation_w','physical_boundary_power_w','body_force_power_w','kinetic_energy_rate_w','transport_power_w'))]:
        for key in keys:
            v=status.get(group,{}).get(key)
            if v is not None:result[key]=v
    for a,v in enumerate(status.get('boundary_force_budget',{}).get('wall_drag_n',[])):result['wall_drag_n_'+str(a)]=v
    for a,v in enumerate(status.get('boundary_force_budget',{}).get('wall_tangential_shear_rms_pa',[])):result['wall_shear_rms_pa_'+str(a)]=v
    for name in ('body_pressure_force_n','body_viscous_force_n','body_total_force_n'):
        for a,v in enumerate(status.get('boundary_force_budget',{}).get(name,[])):result[name+'_'+str(a)]=v
    return result
