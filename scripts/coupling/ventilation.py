"""Opt-in channel aperture and signed balanced-X flow; v1 inlet stays unchanged."""
import math
from surface_sources.growth_fire_v1 import keys,number,require
from .open_boundaries import face,boundaries
CHANNEL='physics_sim_channel_x_inlet/v1'
BALANCED='physics_sim_balanced_x_wind/v1'
LEGACY='physics_sim_driven_x_inlet/v1'
SCHEMAS=(LEGACY,CHANNEL,BALANCED)


def validate_extension(config):
    w=config['wind_forcing']
    if w['schema']!=LEGACY:
        if 'diagnostic_transport' in config:
            from .frozen_transport import validate
            validate(config)
        else:
            require(all(v==0 for v in config['initial_face_velocity_m_s']),'extended wind begins from still gas')
        require(config['boundary_policy']['pressure_datum_pa']==[0,0],'extended wind uses zero pressure reservoirs')
    if w['schema']!=CHANNEL:return
    patch=w['inlet_patch_m'];keys(patch,('lower_yz_m','upper_yz_m'))
    lo=patch['lower_yz_m'];hi=patch['upper_yz_m']
    require(type(lo) is list and type(hi) is list and len(lo)==len(hi)==2,'YZ patch bounds')
    for axis,(a,b) in enumerate(zip(lo,hi),1):
        number(a,0,config['length_m'][axis]);number(b,0,config['length_m'][axis]);require(a<b,'positive aperture extent')
        dx=config['length_m'][axis]/config['grid'][axis]
        require(abs(a/dx-round(a/dx))<1e-12 and abs(b/dx-round(b/dx))<1e-12,'grid-aligned physical aperture')


def aperture(config):
    w=config['wind_forcing'];ny,nz=config['grid'][1:]
    if w['schema']!=CHANNEL:return (0,ny,0,nz)
    p=w['inlet_patch_m'];dy=config['length_m'][1]/ny;dz=config['length_m'][2]/nz
    return (round(p['lower_yz_m'][0]/dy),round(p['upper_yz_m'][0]/dy),
            round(p['lower_yz_m'][1]/dz),round(p['upper_yz_m'][1]/dz))


def face_state(config,data,command):
    w=config['wind_forcing'];nx,ny,nz=config['grid'];dz=config['length_m'][2]/nz
    speed=(command(config,0) if 'diagnostic_transport' in config else 0) if data['steps']==0 else command(config,data['steps']-1);lo,hi,bottom,top=aperture(config)
    sides=(0,1) if w['schema']==BALANCED else ((1,) if w['inlet']=='x_max' else (0,))
    for side in sides:
        for k in range(nz):
            for j in range(ny):
                expected=speed*min(1,(k+.5)*dz/w['ramp_height_m']) if lo<=j<hi and bottom<=k<top else 0
                q=face(config['grid'],0,nx if side else 0,j,k,True)
                require(math.isclose(data['face_velocity_m_s'][q],expected,rel_tol=1e-13,abs_tol=1e-14),'native prescribed X face')
    if w['schema']==CHANNEL:
        for k in range(nz):
            for i in range(nx):
                for j in (0,ny):require(data['face_velocity_m_s'][face(config['grid'],1,i,j,k,True)]==0,'impermeable Y wall')
        for j in range(ny):
            for i in range(nx):require(data['face_velocity_m_s'][face(config['grid'],2,i,j,nz,True)]==0,'impermeable top')
        faces=sum(row[4] for row in boundaries(config))
        for name,axis,normal,start,count,shape in boundaries(config):
            if name not in ('y_min','y_max','z_max'):continue
            for block in range(4):require(all(v==0 for v in data['boundary_fluxes'][block*faces+start:block*faces+start+count]),'closed boundary scalar flux')


def volume(config,steps,command):
    w=config['wind_forcing'];lo,hi,bottom,top=aperture(config);ny,nz=config['grid'][1:]
    if w['schema']==BALANCED:
        total=math.fsum(abs(command(config,k)) for k in range(steps))
    else:
        # Extended channels use the same exact left-endpoint command clock.
        total=math.fsum(abs(command(config,k)) for k in range(steps))
    dy=config['length_m'][1]/ny;dz=config['length_m'][2]/nz
    area=(hi-lo)*dy*dz*math.fsum(min(1,(k+.5)*dz/w['ramp_height_m']) for k in range(bottom,top))
    return total*area*config['momentum_dt_s']


def description(config):
    w=config['wind_forcing']
    if w['schema']==CHANNEL:return 'prescribed_patch_x_inlet_slip_y_top_opposing_x_pressure_outlet'
    if w['schema']==BALANCED:return 'prescribed_balanced_x_throughflow_ambient_y_open_top'
    return 'prescribed_x_inlet_ambient_y_open_top'
