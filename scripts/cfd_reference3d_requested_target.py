"""Requested original full residual controls publication, separately from legacy gates."""
import math

def requested_full_linear_acceptance(info,full_residual,target):
    if not math.isfinite(target) or not 0<target<1:raise ValueError('invalid requested target')
    return info==0 and math.isfinite(full_residual) and 0<=full_residual<target
