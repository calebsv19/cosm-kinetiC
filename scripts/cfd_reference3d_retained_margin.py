"""Explicit internal stopping margin; original reconstructed full target is unchanged."""
import math
def validate_margin(full_target,retained_target):
 if not (math.isfinite(full_target) and math.isfinite(retained_target) and 0<retained_target<full_target<1):raise ValueError('invalid retained/full stopping margin')
 return retained_target
