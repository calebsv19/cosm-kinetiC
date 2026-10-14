"""Manufactured domain fixtures; no experimental coefficient fitting."""
import math


def face(kind='slip_wall', pressure=101325., temperature=300., velocity=(0., 0., 0.), wall_temperature=0.):
    return dict(kind=kind, pressure_origin_pa=pressure, temperature_k=temperature,
                velocity_m_s=list(velocity), wall_temperature_k=wall_temperature)


def config(grid=(4, 4, 16), length=(.1, .1, .1), gravity=(0., 0., -9.81), conductivity=0.):
    return dict(gas=dict(grid=list(grid), length_m=list(length), gamma=1.4, gas_constant_j_kg_k=287.),
        domain=dict(faces=[face() for _ in range(6)], gravity_m_s2=list(gravity), conductivity_w_m_k=conductivity))


def conserved(rho, temperature, velocity=(0., 0., 0.)):
    return [rho, *[rho*v for v in velocity], rho*287*temperature/.4+.5*rho*math.fsum(v*v for v in velocity)]


def hydrostatic(c, temperature=300., pressure=101325.):
    gas=c['gas'];grid=gas['grid'];length=gas['length_m'];gravity=c['domain']['gravity_m_s2'];out=[]
    average=1.
    for a in range(3):
        half=gravity[a]*length[a]/grid[a]/(2*287*temperature)
        average*=math.sinh(half)/half if half else 1.
    for q in range(math.prod(grid)):
        stride=1;argument=0.
        for a in range(3):
            argument+=gravity[a]*(q//stride % grid[a]+.5)*length[a]/grid[a]/(287*temperature)
            stride*=grid[a]
        rho=pressure*math.exp(argument)*average/(287*temperature)
        out+=conserved(rho,temperature)
    return out


def temperature_profile(c, surface=300., slope=1000., curvature=0.):
    """Exact cell-average T polynomial with constant density, zero flow/gravity."""
    grid=c['gas']['grid'];h=c['gas']['length_m'][2]/grid[2];plane=grid[0]*grid[1];out=[]
    for q in range(math.prod(grid)):
        z=(q//plane+.5)*h
        t=surface+slope*z+curvature*(z*z+h*h/12)
        out+=conserved(1.,t)
    return out
