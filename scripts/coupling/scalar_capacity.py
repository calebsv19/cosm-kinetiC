"""Closed capacity policy for a matched scalar wake, independent of solver law."""
from surface_sources.growth_fire_v1 import require
PROFILE='scalar-wake80/v1'
def validate(r):
    require(r['capacity_profile']==PROFILE,'closed scalar wake capacity')
    require(r['grid']==[80]*3 and r['length_m']==[1]*3 and r['momentum_dt_s']==.005,'fixed scalar wake grid/domain/timestep')
    require(r.get('scalar_transport')=='mc_ssprk2/v1' and r.get('transport_scheme','upwind')=='upwind','scalar MC with unchanged upwind momentum')
    require(all(type(r.get(k)) is dict for k in ('open_domain_forcing','solid_box','boundary_policy')),'typed scalar wake bundle')
    require(r['open_domain_forcing'].get('momentum_backflow')=='quiescent_exterior/v1','scalar wake quiescent backflow')
    require(r['solid_box'].get('lower_m')==[.375,.375,.0625] and r['solid_box'].get('upper_m')==[.625,.625,.3125],'fixed low obstacle')
    require(r['boundary_policy'].get('horizontal')=='ambient_open_xy' and r['boundary_policy'].get('vertical')=='solid_bottom_open_top','scalar wake open ground')
