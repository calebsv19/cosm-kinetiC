"""Refined 2D scene policy and scoped assessment for the shared session service."""
import math

TEMPLATES = ('cfd_refined_channel_2d', 'cfd_refined_obstacle_2d')
MODEL = 'incompressible_refined2d_v1'


def scene_parameters(template, lengths, channel, number, error):
    params = {'inlet_mean_m_s': .002, 'solve_mode': 'transient_navier_stokes',
              'refinement_regions': [{'bounds_m': [.25*lengths[0], .25*lengths[1],
                                                   .75*lengths[0], .75*lengths[1]], 'level': 1}]}
    # Smooth empty channels do not benefit from an arbitrary central patch.
    # Keep explicit regions available; obstacle defaults retain body coverage.
    if template == TEMPLATES[0]:
        params['refinement_regions'] = []
    if template == TEMPLATES[1]:
        params['obstacle_bounds_m'] = [.375*lengths[0], .375*lengths[1],
                                       .625*lengths[0], .625*lengths[1]]
    if channel is not None:
        if not isinstance(channel, dict) or set(channel)-set(params):
            raise error('unsupported refined CFD parameters')
        params.update(channel)
    params['inlet_mean_m_s'] = number(params['inlet_mean_m_s'], .00001, 100)
    if params['solve_mode'] not in ('steady_stokes', 'transient_navier_stokes'):
        raise error('refined solve_mode must be steady_stokes or transient_navier_stokes')
    def bounds(values, interior=False):
        if not isinstance(values, list) or len(values) != 4:
            raise error('refined bounds_m requires [xmin,ymin,xmax,ymax]')
        values = [number(v, 0, lengths[k % 2]) for k, v in enumerate(values)]
        if not (values[0] < values[2] and values[1] < values[3]):
            raise error('refined bounds require positive area')
        if interior and not (values[0] > 0 and values[1] > 0 and
                             values[2] < lengths[0] and values[3] < lengths[1]):
            raise error('refined obstacle must be strictly inside the domain')
        return values
    if 'obstacle_bounds_m' in params:
        params['obstacle_bounds_m'] = bounds(params['obstacle_bounds_m'], True)
    regions = params['refinement_regions']
    if not isinstance(regions, list) or len(regions) > 64:
        raise error('at most 64 fixed refinement regions')
    checked = []
    for region in regions:
        if not isinstance(region, dict) or set(region) != {'bounds_m', 'level'}:
            raise error('refinement region requires bounds_m and level')
        checked.append({'bounds_m': bounds(region['bounds_m']),
                        'level': number(region['level'], 0, 10, True)})
    params['refinement_regions'] = checked
    params['solver_model'] = MODEL
    return dict(params, dimensions_m=lengths)


def finite_nonnegative(value):
    return (not isinstance(value, bool) and isinstance(value, (int, float))
            and math.isfinite(value) and value >= 0)


def assess_refined(status, comparisons=None):
    health = status.get('health', {})
    residual = health.get('linear_relative_residual')
    divergence = health.get('max_abs_divergence_s_inv')
    converged = (status.get('state') not in ('starting', 'failed') and
                 health.get('projection_status') == 'converged' and
                 finite_nonnegative(residual) and residual <= 1e-11 and
                 finite_nonnegative(divergence) and divergence < 1e-8)
    reference = status.get('qualification', {}).get('fixed_case_reference_gate', {})
    # Distinguish an unsupported reference problem from a supported case whose
    # spatial resolution is inadequate. Process completion is not acceptance.
    reference_checks = {}
    for name in ('pressure_relative_error', 'viscous_relative_error', 'total_relative_error'):
        value = reference.get(name)
        reference_checks[name] = {'value': value, 'limit': .02,
                                 'status': 'not_established' if not finite_nonnegative(value)
                                 else 'passed' if value <= .02 else 'failed'}
    energy = status.get('energy_budget', {}).get('steady_stokes_relative_residual')
    reference_checks['physical_energy_imbalance'] = {
        'value': energy, 'limit': .02,
        'status': 'not_established' if not finite_nonnegative(energy)
        else 'passed' if energy <= .02 else 'failed'}
    statuses = [check['status'] for check in reference_checks.values()]
    reference_status = ('not_applicable' if reference.get('applicable') is not True
                        else 'failed' if 'failed' in statuses
                        else 'passed' if converged and all(x == 'passed' for x in statuses)
                        else 'not_established')
    reference_accuracy = {
        'status': reference_status,
        'scope': 'fixed confined steady Stokes rectangle only',
        'checks': reference_checks if reference.get('applicable') is True else {},
        'numerical_convergence_required': True,
        'next_action': ('refine the mesh and compare components separately' if reference_status == 'failed'
                        else 'retain a matched finer run as the resolution sensitivity check' if reference_status == 'passed'
                        else 'use a reference applicable to this physical case' if reference_status == 'not_applicable'
                        else 'obtain a successful solve and complete finite observations')}

    return {'schema': 'physics_sim_refined2d_assessment_v1', 'run_id': status.get('run_id'),
            'numerical_status': 'passed' if converged else 'failed' if status.get('state') == 'failed' else 'not_established',
            'reference_accuracy': reference_accuracy,
            'refinement_comparisons': comparisons or {},
            'fixed_case_reference_gate': reference, 'physical_accuracy_certified': False,
            'steady_state': 'stationary_equations_solved' if converged and status.get('solve_mode') == 'steady_stokes' else 'not_established',
            'numerical_memory': health.get('numerical_memory', {}),
            'scope': 'Reference gate applies only to matching fixed steady Stokes rectangle; arbitrary transient physical accuracy is not certified'}


def comparison_metrics(status):
    """Physical integral observations; no field-error or uncertainty inference."""
    result = {}
    force = status.get('boundary_force_budget', {})
    if force.get('available'):
        for key in ('pressure_force_n', 'viscous_force_n', 'total_force_n'):
            values = force.get(key)
            if isinstance(values, list) and len(values) == 2:
                for axis, value in zip(('x', 'y'), values):
                    result[key + '_' + axis] = value
    energy = status.get('energy_budget', {})
    if energy.get('available'):
        for key in ('kinetic_energy_j', 'physical_strain_dissipation_w',
                    'physical_boundary_power_w', 'outward_kinetic_flux_w'):
            if key in energy:
                result[key] = energy[key]
    return result
