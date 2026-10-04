"""Existing continuous duct Fourier response; separate mathematical tail and float arithmetic."""
import math
import numpy as np

def conductance(height=2.,width=2.,terms=512):
 if not (math.isfinite(height) and math.isfinite(width) and height>0 and width>0 and isinstance(terms,int) and 16<=terms<=4096):raise ValueError('invalid duct reference')
 h,w=sorted((height,width));s=math.fsum(math.tanh((2*t+1)*math.pi*w/(2*h))/(2*t+1)**5 for t in range(terms))
 value=h**3*w/12-16*h**4/math.pi**5*s;last=2*terms-1
 tail=4*h**4/(math.pi**5*last**4)
 assert value>tail>0
 return dict(conductance_m4=value,positive_series_tail_bound_m4=tail,terms=terms,float64_roundoff_certified=False,source='existing cfd_duct3d_reference_mean times cross-section area; positive odd m^-5 tail bounded by all-integer integral')

def baseline_diagnostics(length,mu,flow,inlet,dissipation,p,pb,mass):
 c=conductance();expected=mu*flow*length/c['conductance_m4'];exact=expected*(1-pb.doflocs[0]/length);shape=inlet*(1-pb.doflocs[0]/length)
 errors=dict(pressure_drop_relative_error=abs(inlet/expected-1),dissipation_relative_error=abs(dissipation/(expected*flow)-1),pressure_field_mass_relative_error=mass.norm(p-exact)/mass.norm(exact),recovered_inlet_pressure_shape_mass_relative_error=mass.norm(p-shape)/mass.norm(shape))
 accepted=all(math.isfinite(v) and v<.001 for k,v in errors.items() if k!='recovered_inlet_pressure_shape_mass_relative_error')
 return dict(reference=c,expected_inlet_pressure_pa=expected,expected_dissipation_w=expected*flow,errors=errors,relative_accuracy_gate=.001,baseline_reference_accuracy_accepted=accepted,physical_obstacle_force_certified=False)
