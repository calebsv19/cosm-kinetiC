"""Separate 2019 surface-feedback apparatus from the 2024 plume benchmark.

Curated primary-source facts, not machine-extracted synchronized sensor data.
The NIST-hosted author manuscript and published version differ in property tables;
no manuscript latent-heat value is silently substituted for WebBook properties.
"""
from surface_sources.growth_fire_v1 import sealed


def gauge_height_constraint():
    """Local center extrapolation; deliberately not an integrated power factor."""
    return sealed({'schema':'methanol_gauge_height_constraint/v1',
        'case_id':'nist-kim-lee-hamins-2019-30cm-water-cooled',
        'source':'https://tsapps.nist.gov/publication/get_pdf.cfm?pub_id=928848',
        'source_location':'author manuscript page14, gauge elevation discussion',
        'quantity':'approximate difference between raised gauge and surface total flux',
        'reported_fraction':.15,'location':'pool center','gauge_height_above_rim_m':.003,
        'evidence_kind':'authors extrapolation; not a whole-pool surface measurement',
        'global_attenuation_factor_admissible':False,
        'surface_power_identified':False,
        'required_model_comparison':'Evaluate the two probe planes at pool center using the same gas/radiation field.',
        'comparison_status':'not_evaluated; physical surface transfer field not yet available'})


def experimental_boundary():
    return sealed({'schema':'methanol_experimental_boundary/v1',
        'case_id':'nist-kim-lee-hamins-2019-30cm-water-cooled',
        'source':'https://tsapps.nist.gov/publication/get_pdf.cfm?pub_id=928848',
        'publication':'https://doi.org/10.1016/j.firesaf.2019.01.004',
        'source_form':'NIST-hosted author manuscript, checked using public web PDF reader',
        'source_pdf_local_capture':'HTTP403; no local PDF byte hash claimed',
        'geometry':{'diameter_m':.301,'pan_depth_m':.15,'wall_thickness_m':.0013,
            'lip_height_m':.010,'rim_height_above_floor_m':.27,'liquid_depth_m':.14},
        'cooling':{'bottom_temperature_k':293.15,'reported_uncertainty_k':2.,
            'approximate_water_flow_m3_s':.001/60,'conductance_w_k':None},
        'warmup_s':600,'steady_measurement_duration_s':{'greater_than':2000},
        'table_3':{'mass_flux_kg_m2_s':.0135,'relative_expanded_uncertainty':.09,
            'ideal_hrr_w':19200,'note':'Use same study mass/HRR with feedback; not TN2162 averages.'},
        'table_6':{'integrated_total_gauge_power_w':1600,'relative_expanded_uncertainty':.24,
            'calculated_surface_convection_power_w':520,'relative_expanded_convection_uncertainty':.17,
            'feedback_fraction':.082,'radiative_feedback_fraction':.055},
        'gauge':{'height_above_rim_m':.003,'height_above_liquid_m':.013,
            'quantity':'total incident flux to cooled gauge at raised plane',
            'direct_surface_forcing_admissible':False},
        'uncertainty_definition':'Combined expanded uncertainty with coverage factor two unless otherwise noted.',
        'unresolved_boundary_parameters':['Actual absorbed radiative and convective flux at liquid surface',
            'Fuel reflection/absorption and effective thermal emissivity',
            'Liquid-to-pan/coolant effective conductance and stratification',
            'Temperature-dependent average liquid heat capacity'],
        'interpretation':'Authors identify gauge elevation and heat losses as explanations for excess gauge heat. No inferred residual is a measured loss.',
        'physical_calibration_qualified':False})
