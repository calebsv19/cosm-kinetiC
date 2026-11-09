"""Two-fluid linear capillary–gravity waves with a vertically resolved potential solve."""
import copy
import hashlib
import json
import math
from pathlib import Path

from chemistry_reactor import digest

SCHEMA = 'rc7-capillary-interface/v1'
SPECIES = {'species': [{'id': 'H2O_liquid'}, {'id': 'dry_air'}]}
MOLAR_MASS = (.01801528, .028965)


def example(layers=32):
    return dict(schema=SCHEMA, chemistry=copy.deepcopy(SPECIES), n=[64, 32, 1],
                length_m=.04, width_m=.01, depth_m=[.018, .003],
                density_kg_m3=[997., 1.2], gravity_m_s2=9.81, tension_n_m=.072,
                layers=layers, dt_s=.0002, modes=[
                    dict(harmonic=1, elevation_m=[8e-5, 0.], speed_m_s=[0., 0.]),
                    dict(harmonic=2, elevation_m=[0., 1.5e-5], speed_m_s=[.001, 0.])])


def solve_potential(k, depth, layers):
    """P1 weak Laplace solve from rigid wall to interface with unit outward flux."""
    dz = depth/layers
    diagonal = 1/dz+k*k*dz/3
    off = -1/dz+k*k*dz/6
    upper, values = [], []
    for i in range(layers+1):
        pivot = diagonal if i in (0,layers) else 2*diagonal
        if i:
            pivot -= off*upper[-1]
        values.append(((1. if i==layers else 0.)-(off*values[-1] if i else 0.))/pivot)
        upper.append(off/pivot)
    for i in range(layers-1,-1,-1):
        values[i] -= upper[i]*values[i+1]
    return values


class CapillaryInterface:
    def __init__(self, config):
        if set(config) != set(example()) or config['schema'] != SCHEMA or config['chemistry'] != SPECIES:
            raise ValueError('capillary interface configuration')
        self.config = copy.deepcopy(config)
        self.length, self.width, self.dt = (config[k] for k in ('length_m','width_m','dt_s'))
        self.depth, self.density = self.config['depth_m'],self.config['density_kg_m3']
        self.gravity,self.tension = config['gravity_m_s2'],config['tension_n_m']
        if len(self.depth)!=2 or len(self.density)!=2:
            raise ValueError('two capillary layers required')
        for value,lo,hi in [(self.length,.005,10.),(self.width,.001,10.),(self.dt,1e-7,.1),
                (self.gravity,0.,30.),(self.tension,0.,1.),
                *((d,.0005,1.) for d in self.depth), *((d,.01,2000.) for d in self.density)]:
            if type(value) not in (int,float) or not math.isfinite(value) or not lo<=value<=hi:
                raise ValueError('capillary interface parameter bounds')
        if self.density[0]<=self.density[1] or not (self.gravity or self.tension):
            raise ValueError('capillary interface requires stable density and restoring force')
        self.nx,self.ny,nz = config['n']
        self.layers = config['layers']
        if (any(type(v) is not int for v in (self.nx,self.ny,nz,self.layers)) or
                not 16<=self.nx<=256 or not 8<=self.ny<=128 or nz!=1 or not 4<=self.layers<=256):
            raise ValueError('capillary grid bounds')
        if not 1<=len(config['modes'])<=8:
            raise ValueError('capillary mode count')
        self.area = self.length*self.width
        self.modes = []
        self.initial = []
        used = set()
        for mode in config['modes']:
            if set(mode)!={'harmonic','elevation_m','speed_m_s'}:
                raise ValueError('capillary mode fields')
            harmonic = mode['harmonic']
            if type(harmonic) is not int or not 1<=harmonic<self.nx//4 or harmonic in used:
                raise ValueError('capillary mode harmonic')
            used.add(harmonic)
            if any(len(mode[key])!=2 for key in ('elevation_m','speed_m_s')):
                raise ValueError('capillary mode shape')
            components = [*mode['elevation_m'],*mode['speed_m_s']]
            if any(type(v) not in (int,float) or not math.isfinite(v) or abs(v)>10 for v in components):
                raise ValueError('capillary mode amplitude')
            k = math.tau*harmonic/self.length
            profiles = [solve_potential(k,d,self.layers) for d in self.depth]
            inertia = [rho*p[-1] for rho,p in zip(self.density,profiles)]
            restoring = (self.density[0]-self.density[1])*self.gravity+self.tension*k*k
            omega = math.sqrt(restoring/math.fsum(inertia))
            if self.dt*omega>.25:
                raise ValueError('capillary timestep exceeds wave resolution bound')
            self.modes.append((k,profiles,inertia,restoring,omega))
            self.initial.append(components)
        envelopes = [math.hypot(a,b)+math.hypot(c,d)/m[-1] for (a,b,c,d),m in zip(self.initial,self.modes)]
        if (math.fsum(envelopes)>.08*min(self.depth) or
                math.fsum(a*m[0] for a,m in zip(envelopes,self.modes))>.08):
            raise ValueError('capillary small-amplitude envelope')
        self.state = copy.deepcopy(self.initial)
        self.tick = 0
        self.initial_energy = math.fsum(self.energy())
        self.adapter = digest([hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
                               for name in ('chemistry_capillary_interface.py','chemistry_reactor.py')])

    def energy(self):
        factor = self.area/4
        return [
            factor*math.fsum(mode[2][phase]*(s[2]**2+s[3]**2) for s,mode in zip(self.state,self.modes))
            for phase in (0,1)] + [
            factor*(self.density[0]-self.density[1])*self.gravity*math.fsum(s[0]**2+s[1]**2 for s in self.state),
            factor*self.tension*math.fsum(mode[0]**2*(s[0]**2+s[1]**2)
                                         for s,mode in zip(self.state,self.modes))]

    def advance(self, steps):
        if type(steps) is not int or not 0<=steps<=10000 or self.tick+steps>2**53:
            raise ValueError('capillary step count')
        # Implicit midpoint on each oscillator, applied as one equivalent rotation.
        state = []
        for (a,b,u,v),mode in zip(self.initial,self.modes):
            omega = mode[-1]
            phase = (self.tick+steps)*2*math.atan(.5*self.dt*omega)
            c,s = math.cos(phase),math.sin(phase)
            state.append([a*c+u*s/omega,b*c+v*s/omega,u*c-a*omega*s,v*c-b*omega*s])
        self.state = state
        self.tick += steps

    def checkpoint(self):
        payload = dict(schema=SCHEMA, config=digest(self.config), adapter=self.adapter, tick=self.tick)
        return dict(payload=payload,sha256=digest(payload))

    def restore(self, packet):
        p = packet['payload']
        expected = self.checkpoint()['payload']
        if (set(p)!=set(expected) or packet['sha256']!=digest(p) or
                any(p[k]!=expected[k] for k in ('schema','config','adapter')) or
                type(p['tick']) is not int or not 0<=p['tick']<=2**53):
            raise ValueError('capillary checkpoint identity')
        # State is a deterministic function of the accepted tick.
        self.tick = p['tick']
        self.advance(0)

    def sample(self, phase, x, distance_from_wall):
        """Return (u, w, dynamic pressure) on the linearized phase domain."""
        depth = self.depth[phase]
        if not 0<=distance_from_wall<=depth or not math.isfinite(x):
            raise ValueError('capillary sample position')
        dz = depth/self.layers
        i = min(int(distance_from_wall/dz),self.layers-1)
        fraction = (distance_from_wall-i*dz)/dz
        sign = 1 if phase==0 else -1
        u,w,p = 0.,0.,0.
        for (a,b,vx,vy),(k,profiles,_,_,omega) in zip(self.state,self.modes):
            c,s = math.cos(k*x),math.sin(k*x)
            f = profiles[phase]
            value = f[i]+fraction*(f[i+1]-f[i])
            u += sign*value*k*(-vx*s+vy*c)
            w += (f[i+1]-f[i])/dz*(vx*c+vy*s)
            p += sign*self.density[phase]*value*omega**2*(a*c+b*s)
        return u,w,p

    def report(self):
        surface, speed, pressure, jump, residual = [],[],[],[],[]
        for i in range(self.nx):
            x = (i+.5)*self.length/self.nx
            eta,w,cap = 0.,0.,0.
            for (a,b,u,v),(k,*_) in zip(self.state,self.modes):
                c,s = math.cos(k*x),math.sin(k*x)
                elevation = a*c+b*s
                eta += elevation
                w += u*c+v*s
                cap += self.tension*k*k*elevation
            p = [self.sample(j,x,d)[2]-rho*self.gravity*eta
                 for j,(d,rho) in enumerate(zip(self.depth,self.density))]
            surface.append(eta)
            speed.append(w)
            pressure.append(p)
            jump.append(cap)
            residual.append(p[0]-p[1]-cap)

        bulk = [[list(self.sample(j,(i+.5)*self.length/self.nx,depth/2))
                 for i in range(self.nx)] for j,depth in enumerate(self.depth)]
        dz = math.fsum(self.depth)/self.ny
        fractions = [min(1.,max(0.,(eta-(self.depth[1]-(j+1)*dz))/dz))
                     for j in range(self.ny) for eta in surface]
        cell_volume = self.area/self.nx*dz
        masses = [self.density[0]*cell_volume*math.fsum(fractions),
                  self.density[1]*cell_volume*math.fsum(1.-x for x in fractions)]
        mass_residual = [m-rho*self.area*h for m,rho,h in zip(masses,self.density,self.depth)]
        energy = self.energy()
        kinetic_power = [self.area/2*math.fsum(-m[2][j]*m[-1]**2*(s[0]*s[2]+s[1]*s[3])
                         for s,m in zip(self.state,self.modes)) for j in (0,1)]
        return dict(tick=self.tick,time_s=self.tick*self.dt,
            concentration_mol_m3=[[f*self.density[0]/MOLAR_MASS[0],(1.-f)*self.density[1]/MOLAR_MASS[1]]
                                  for f in fractions],
            totals_mol=[m/mw for m,mw in zip(masses,MOLAR_MASS)],extent_mol=[],
            species_budget_residual_mol=[m/mw for m,mw in zip(mass_residual,MOLAR_MASS)],
            capillary=dict(elevation_m=surface,normal_velocity_m_s=speed,interface_pressure_pa=pressure,
                capillary_jump_pa=jump,max_jump_residual_pa=max(map(abs,residual)),
                mid_depth_u_w_dynamic_pressure=bulk,
                phase_mass_kg=masses,mass_residual_kg=mass_residual,kinetic_j=energy[:2],
                gravity_j=energy[2],surface_j=energy[3],kinetic_power_w=kinetic_power,
                total_energy_residual_j=math.fsum([*energy,-self.initial_energy]),
                omega_rad_s=[m[-1] for m in self.modes]))
