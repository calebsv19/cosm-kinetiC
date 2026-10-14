"""Read-only accepted-run observables and authoring feedback provenance."""
import base64,csv,json,math,sqlite3
from pathlib import Path
from .framework import Framework
from surface_sources.growth_fire_v1 import sealed,require


def inspect_case(root,output):
    root=Path(root);output=Path(output);output.mkdir(parents=True,exist_ok=False)
    config=json.loads((root/'config.json').read_text());host=Framework(config,root/'run');head=host.inspect()
    require(head==json.loads((root/'accepted-checkpoint.json').read_text()),'review accepted head identity')
    db=sqlite3.connect(host.path.resolve().as_uri()+'?mode=ro',uri=True)
    try:checkpoints=[json.loads(v[0]) for v in db.execute('SELECT payload FROM checkpoints ORDER BY revision')]
    finally:db.close()
    native=lambda cp:host.fire.state_decode(base64.b64decode(cp['source']['checkpoint']['native_state_b64']))
    open_profile=config['source']['evolution']['model']=='physical_rates_heat_outflow_v1'
    initial=native(checkpoints[0]);areas=host.fire.areas(config['source']);cal=config['source']['calibration'];rows=[]
    previous_energy=0.;previous_mass=0.;previous_time=0.
    for cp in checkpoints:
        state=native(cp);fluid=cp['fluid'];fields=fluid['fields'];time=fields['time_s']
        mass=math.fsum((a-b)*area*cal['fuel_kg_per_unit_m2'] for a,b,area in zip(initial['fields']['fuel'],state['fields']['fuel'],areas))
        energy=mass*cal['energy_j_per_fuel_kg'];dt=time-previous_time
        transport=cp['source'].get('frame');budget={} if transport is None else transport['diagnostics'].get('thermal_transport',{})
        held=cp['evidence'].get('held_return');speed=0 if held is None else max(math.hypot(*v) for v in held['wind_input']['tangent_velocity_m_s'])
        proxy=state['fields']['heat'];proxy_total=math.fsum(v*a for v,a in zip(proxy,areas))
        edge=math.fsum(proxy[i]*areas[i] for i in range(len(proxy)) if i%config['source']['width']==config['source']['width']-1)
        rows.append({'right_edge_heat_proxy_fraction':0 if proxy_total==0 else edge/proxy_total,'time_s':time,'fuel_consumed_kg':mass,'energy_generated_j':energy,
            'source_interval_average_hrr_w':0 if dt==0 else (energy-previous_energy)/dt,
            'source_interval_average_burning_rate_kg_s':0 if dt==0 else (mass-previous_mass)/dt,
            'fluid_input_energy_j':fluid['budgets']['energy_j']['input'],
            'fluid_stored_energy_j':math.fsum(fields['energy_j']),
            'fluid_outflow_energy_j':fluid['budgets']['energy_j'].get('outflow',0),
            'fluid_temperature_peak_k':max(fields['temperature_k']),
            'max_applied_tangent_speed_m_s':speed,'window_proxy_outflow_normalized_m2':budget.get('outflow',0)})
        previous_energy=energy;previous_mass=mass;previous_time=time
    with (output/'observables.csv').open('w',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
    held=head['evidence']['held_return'];scene=json.loads((root/'scene/combined_scene.json').read_text())
    value=sealed({'schema':'fire_fluid_authoring_review/v1','joint_checkpoint_digest':head['digest'],
        'source_case':str(root.resolve()),'model_time_s':rows[-1]['time_s'],
        'feedback':{'profile':config['source']['evolution']['model'],'units':'m/s','frame':'right_handed_z_up_meters',
            'sample_height_m':config['return_channel']['sample_height_m'],
            'hold_window_ticks':held['held_window_ticks'],'ticks_per_second':head['evidence']['rate'],
            'prior_accepted_joint_digest':held['observation']['joint_checkpoint_digest'],
            'observation_digest':held['observation']['digest'],'wind_input_digest':held['wind_input_digest'],
            'max_applied_tangent_speed_m_s':rows[-1]['max_applied_tangent_speed_m_s'],'applied_to_fire':True},
        'right_edge_heat_proxy_fraction':rows[-1]['right_edge_heat_proxy_fraction'],
        'scene_digest':scene['digest'],'scene_capabilities':scene['capabilities'],
        'fire_boundary':('ambient_zero_proxy_advective_outflow_no_diffusive_flux_v1' if open_profile else 'closed_proxy_transport_v1'),
        'field_semantics':{'fire_heat':'normalized ignition proxy; not K or J','fluid_temperature':'native K',
            'energy_generated':'authored fuel-consumption calibration, J','velocity':'native double sampled; explicit binary32 Fire conversion'},
        'authoring_gaps':['Exported plane is static; scene declares surface_field_shading false.',
            'Review metadata exposes applied wind height/hold; no live editor controls are implemented.',
            ('Open advective Fire edges permit proxy outflow with zero incoming proxy; diffusion remains zero flux. This is not a material thermal law.' if open_profile else 'Closed Fire patch blocks heat outflow; strong long runs accumulate heat at the downwind edge. Assess boundary policy before physical calibration.'),
            'Human visual acceptance and combustion calibration remain outstanding.'],
        'physical_calibration_qualified':False})
    (output/'authoring-review.json').write_text(json.dumps(value,indent=2)+'\n')
    return value,rows,head
