"""Independent discrete MAC kinetic accounting, separate from scalar heat."""
import math
from surface_sources.growth_fire_v1 import require
from .open_boundaries import face
NAMES=('initial','advection_boundary','advection_interior','viscosity','buoyancy',
       'explicit_euler','inlet_enforcement','projection','roundoff_closure','arithmetic_roundoff')
def kinetic(config,velocity):
    grid=config['grid'];volume=math.prod(l/n for l,n in zip(config['length_m'],grid))
    terms=[]
    for a in range(3):
        for k in range(grid[2]+(a==2)):
            for j in range(grid[1]+(a==1)):
                for i in range(grid[0]+(a==0)):
                    weight=.5 if (i,j,k)[a] in (0,grid[a]) else 1.
                    u=velocity[face(grid,a,i,j,k,True)]
                    terms.append(weight*u*u)
    return .5*config['properties']['density_kg_m3']*volume*math.fsum(terms)
def validate(config,data):
    if not config.get('wind_forcing',{}).get('momentum_backflow'):return
    ledger=data['kinetic_ledger_j'];stored=data['kinetic_stored_j']
    require(type(ledger) is list and len(ledger)==10 and all(math.isfinite(x) for x in ledger),'finite kinetic ledger')
    expected=kinetic(config,data['face_velocity_m_s'])
    scale=max(1.,abs(stored),math.fsum(abs(x) for x in ledger))
    require(math.isfinite(stored) and stored>=0 and abs(stored-expected)<1e-11*scale,'independent MAC kinetic energy')
    require(abs(stored-math.fsum(ledger))<1e-10*scale,'discrete kinetic stage closure')
    require(ledger[5]>=0,'Euler quadratic nonnegative')
    initial=kinetic(config,config['initial_face_velocity_m_s'])
    require(abs(ledger[0]-initial)<1e-12*max(1,initial),'kinetic initial authority')
