"""Pinned NIST/MaCFP 30 cm methanol benchmark, with explicit SI and model limits.

CSV HRR endpoints describe a steady interval, not a measured ignition transient.
Radial heat-flux gauges are outside the pool; they are not pool heat feedback.
"""
import csv,hashlib,json,math
from pathlib import Path
from surface_sources.growth_fire_v1 import require,sealed

SPECS={
    'Methanol_30_cm_HRR_Sung_2024.csv':(('Time','HRR','MLR','MLRPUA','X_RAD'),('s','kW','kg/s','kg/s/m2','none'),(1,1000,1,1,1)),
    'Methanol_30_cm_TC_r=0_cm_Sung_2024.csv':(('z_cm','r_cm','TC_K','TC_RMS_K','TG_K','TG_RMS_K','Uc_TG_K','D_bead_um'),None,(.01,.01,1,1,1,1,1,1e-6)),
    'Methanol_30_cm_U_r=0_Sung_2024.csv':(('z','r','U','Uc_U'),('cm','cm','m/s','m/s'),(.01,.01,1,1)),
    'Methanol_30_cm_HF_radial_z=1_cm_Sung_2024.csv':(('r','z','q','Uc_q'),('cm','cm','kW/m2','kW/m2'),(.01,.01,1000,1000)),
}

def table(path,headers,units,scales):
    with Path(path).open(newline='') as f:rows=list(csv.reader(f))
    require(tuple(rows[0])==headers,'reference column identity')
    offset=1
    if units is not None:
        require(tuple(rows[1])==units,'reference units');offset=2
    values=[]
    for row in rows[offset:]:
        require(len(row)==len(headers),'reference row width')
        v=[float(x)*scale for x,scale in zip(row,scales)]
        require(all(math.isfinite(x) and x>=0 for x in v),'reference finite nonnegative measurements')
        values.append(v)
    require(values,'empty reference');return values

def inspect(root,config):
    root=Path(root);provenance=json.loads((root/'provenance.json').read_text())
    hashes={row['file']:row for row in provenance['files']}
    for name in SPECS:
        require(name in hashes,'reference provenance missing')
        require(hashlib.sha256((root/name).read_bytes()).hexdigest()==hashes[name]['sha256'],'reference bytes changed')
    data={name:table(root/name,*spec) for name,spec in SPECS.items()}
    hrr=data['Methanol_30_cm_HRR_Sung_2024.csv'];require(len(hrr)==2 and hrr[0][0]==0 and hrr[1][0]==600 and hrr[0][1:]==hrr[1][1:],'published steady source interval')
    temperature=data['Methanol_30_cm_TC_r=0_cm_Sung_2024.csv'];velocity=data['Methanol_30_cm_U_r=0_Sung_2024.csv']
    ambient=config['fluid']['properties']['reference_temperature_k'];peak=max(row[4] for row in temperature)
    contrast=(peak-ambient)/ambient
    # Reference selection is independent of fitting. These missing physical
    # capabilities must be resolved before any full-pool calibration claim.
    gaps=['Current Fire state is a normalized ignition proxy with stationary authored fuel, not a liquid surface energy/vaporization balance.',
          'Current source support is a rectangular patch; benchmark is a round 0.301 m water-cooled pool with a 0.01 m lip.',
          'Current return channel carries tangent velocity only; material heat response needs declared incident heat flux and losses.',
          'Current cases are short shear-forced transients; benchmark values are steady measurements after warm-up.',
          'Radiative surroundings flux is a validation observable, not imposed pool heating or convective return flux.']
    if contrast>config['fluid']['buoyancy']['max_temperature_contrast_fraction']:
        gaps.append('Measured gas temperature exceeds the admitted constant-property Boussinesq temperature-contrast regime.')
    if max(row[0] for row in velocity)>config['fluid']['length_m'][2]:gaps.append('Current domain does not contain all measured centerline velocity locations.')
    return sealed({'schema':'fire_fluid_methanol_reference/v1','material':'methanol','repository_commit':provenance['repository_commit'],
        'source_hashes':{name:hashes[name]['sha256'] for name in SPECS},
        'publication':'https://doi.org/10.6028/NIST.TN.2162r1','pool_diameter_m':.301,'lip_height_m':.01,
        'steady_interval_s':[hrr[0][0],hrr[-1][0]],'steady_hrr_w':hrr[0][1],'steady_mass_loss_kg_s':hrr[0][2],
        'mass_flux_kg_m2_s':hrr[0][3],'radiative_fraction':hrr[0][4],
        'derived_energy_j_per_kg':hrr[0][1]/hrr[0][2],
        'si_profiles':{'gas_temperature':[{'z_m':v[0],'r_m':v[1],'temperature_k':v[4],'rms_k':v[5],'reported_uncertainty_k':v[6]} for v in temperature],
            'upward_velocity':[{'z_m':v[0],'r_m':v[1],'velocity_m_s':v[2],'reported_uncertainty_m_s':v[3]} for v in velocity],
            'surroundings_heat_flux':[{'r_m':v[0],'z_m':v[1],'flux_w_m2':v[2],'reported_uncertainty_w_m2':v[3]} for v in data['Methanol_30_cm_HF_radial_z=1_cm_Sung_2024.csv']]},
        'peak_reference_gas_temperature_k':peak,'peak_relative_temperature_contrast':contrast,
        'current_model_compatible':False,'compatibility_gaps':gaps,'parameters_fitted':False,'physical_calibration_qualified':False,
        'validation_split':'Use mass loss/material energetics for source development; reserve centerline velocity/temperature and surroundings flux for validation. No empirical wind-response data supplied by this benchmark.'})
