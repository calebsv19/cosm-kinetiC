"""Versioned historical aqueous-glucose data reduction; never invent missing errors."""
import hashlib,itertools,json,math
from pathlib import Path
DATA=Path(__file__).resolve().parents[1]/'data/chemistry/aqueous_glucose_v1.json'
MOLAR_MASS=.180156 # kg/mol, C/H/O weights 0.012011/0.001008/0.015999
R=8.31446261815324

def fit_rate(times,rotations,endpoint):
 origin=times[0];amplitude=rotations[0]-endpoint
 return -sum((t-origin)*math.log((y-endpoint)/amplitude) for t,y in zip(times,rotations))/sum((t-origin)**2 for t in times)/60

def load_data():
 raw=DATA.read_bytes();d=json.loads(raw)
 if d['schema']!='aqueous-glucose-data/v1':raise ValueError('aqueous dataset schema')
 o=d['observations'];e=d['equilibrium'];recipe=d['recipe'];train=o['training_indices'];times=[o['time_min'][i] for i in train];rot=[o['rotation_deg_saccharimeter'][i] for i in train];endpoint=o['equilibrium_rotation_deg_saccharimeter']
 if set(train)&set(o['held_out_indices']) or train!=[0,1,2,3] or o['held_out_indices']!=[4,5]:raise ValueError('frozen training/held-out split')
 rate=fit_rate(times,rot,endpoint);A=e['specific_rotation_alpha'];B=e['specific_rotation_beta'];E=e['specific_rotation_equilibrium'];K=(A-E)/(E-B)
 rates=[fit_rate(times,[v+s*o['rotation_rounding_half_unit'] for v,s in zip(rot,signs[:-1])],endpoint+signs[-1]*o['rotation_rounding_half_unit']) for signs in itertools.product((-1,1),repeat=5)]
 ks=[(a-eq)/(eq-b) for a,b,eq in itertools.product(*[(x-e['specific_rotation_rounding_half_unit'],x+e['specific_rotation_rounding_half_unit']) for x in (A,B,E)])]
 alpha_bounds=[(first*eq/end-b)/(a-b) for first,end,a,b,eq in itertools.product((rot[0]-.005,rot[0]+.005),(endpoint-.005,endpoint+.005),(A-.05,A+.05),(B-.05,B+.05),(E-.05,E+.05))]
 volume=(recipe['glucose_g']+recipe['water_g'])/recipe['solution_density_g_ml']*1e-6
 concentration=recipe['glucose_g']*.001/MOLAR_MASS/volume
 return dict(raw=d,sha256=hashlib.sha256(raw).hexdigest(),rate_s=rate,K=K,k_forward=rate*K/(1+K),k_reverse=rate/(1+K),rate_rounding_range_s=[min(rates),max(rates)],K_rounding_range=[min(ks),max(ks)],concentration_mol_m3=concentration,recipe_volume_m3=volume,first_alpha_fraction=(rot[0]*E/endpoint-B)/(A-B),first_alpha_rotation_rounding_range=[min(alpha_bounds),max(alpha_bounds)],rotation_scale=endpoint/E)

def qualification():
 p=load_data();d=p['raw'];o=d['observations'];e=d['equilibrium'];q=d['qualification'];endpoint=o['equilibrium_rotation_deg_saccharimeter'];first=o['rotation_deg_saccharimeter'][0];start=o['time_min'][0];rows=[]
 for i in o['held_out_indices']:
  t=o['time_min'][i];actual=o['rotation_deg_saccharimeter'][i];pred=endpoint+(first-endpoint)*math.exp(-p['rate_s']*60*(t-start));bounds=[end+(begin-end)*math.exp(-rate*60*(t-start)) for end in (endpoint-.005,endpoint+.005) for begin in (first-.005,first+.005) for rate in p['rate_rounding_range_s']]
  rows.append(dict(time_min=t,observed=actual,predicted=pred,error=pred-actual,rounding_only_prediction_range=[min(bounds),max(bounds)],within_rounding_only_range=min(bounds)<=actual<=max(bounds)))
 published=d['published_rate']['common_log_total_per_min']*math.log(10)/60;rate_error=abs(p['rate_s']/published-1);beta=p['K']/(1+p['K']);eq_error=abs(beta-e['independent_approximate_beta_fraction'])
 return dict(schema='rc6-empirical-qualification/v1',dataset_sha256=p['sha256'],parameters={k:p[k] for k in ['rate_s','K','k_forward','k_reverse','rate_rounding_range_s','K_rounding_range','first_alpha_fraction','first_alpha_rotation_rounding_range','concentration_mol_m3']},held_out=rows,published_natural_rate_s=published,relative_published_rate_error=rate_error,equilibrium_beta_fraction=beta,approximate_equilibrium_fraction_error=eq_error,acceptance=q,passed=all(abs(r['error'])<=q['held_out_max_error_deg_saccharimeter'] for r in rows) and rate_error<=q['rate_relative_tolerance'] and eq_error<=q['approximate_beta_fraction_tolerance'],uncertainty_status='Rotation-digit rounding-only bands (timestamps fixed) exclude held-out observations; measurement/systematic/temperature/pH/model-reduction uncertainty remains unquantified. Engineering tolerance is not a confidence interval.')
