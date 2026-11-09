"""A spherical water droplet with compressible radial pure-vapor flow."""
import copy
import hashlib
import math
from pathlib import Path

from chemistry_reactor import digest
from chemistry_stefan_interface import (
    CV, GAMMA, LIQUID_HEAT_CAPACITY, MOLAR_MASS, R, REFERENCE_K, SPECIES,
    StefanInterface, example as planar_example,
)

SCHEMA = 'rc7-spherical-droplet/v1'


def example(cells=24, condense=False):
    config = planar_example(cells, condense)
    del config['area_m2'], config['height_m']
    config.update(schema=SCHEMA, chamber_radius_m=.002, tension_n_m=.072,
                  liquid_kg=5.22e-7, dt_s=2e-8)
    return config


class SphericalDroplet(StefanInterface):
    def __init__(self, library, config):
        if set(config) != set(example()) or config['schema'] != SCHEMA or config['chemistry'] != SPECIES:
            raise ValueError('spherical droplet configuration')
        nx, ny, nz = config['n']
        if any(type(n) is not int for n in (nx,ny,nz)) or not 5 <= nx <= 257 or (ny,nz) != (1,1):
            raise ValueError('spherical droplet grid')
        self.cells = nx - 1
        self.config = copy.deepcopy(config)
        self.height = config['chamber_radius_m']
        self.tension = config['tension_n_m']
        self.dt = config['dt_s']
        self.permeance = config['permeance_kg_m2_s_pa']
        liquid = config['liquid_kg']
        t = config['liquid_temperature_k']
        for value, lower, upper in ((self.height,.0002,.1),(self.tension,0.,.2),
                (self.dt,1e-11,.001),(self.permeance,0.,.001),(liquid,1e-12,10.),(t,283.15,323.15)):
            if type(value) not in (int,float) or not math.isfinite(value) or not lower <= value <= upper:
                raise ValueError('spherical droplet parameter bounds')
        # Reuse the planar model's compiled property admission and source pins.
        seed = StefanInterface(library, planar_example())
        self.lib, self.get_properties = seed.lib, seed.get_properties
        self.rho_liquid, self.latent, self.engine = seed.rho_liquid, seed.latent, seed.engine
        self.adapter = digest([seed.adapter, hashlib.sha256(Path(__file__).read_bytes()).hexdigest()])
        radius, _ = self.geometry(liquid)
        _, volumes = self.shells(radius)
        initial = [config[key] for key in ('gas_pressure_pa','gas_temperature_k','gas_velocity_m_s')]
        if any(len(field) != self.cells for field in initial):
            raise ValueError('spherical droplet initial field shape')
        gas = []
        for p, temperature, velocity, volume in zip(*initial,volumes):
            if not all(type(v) in (int,float) and math.isfinite(v) for v in (p,temperature,velocity)):
                raise ValueError('spherical droplet initial field')
            self.admit_gas(p,temperature,velocity)
            mass = p * volume / (R * temperature)
            gas.extend((mass, mass*velocity, mass*(self.latent+CV*(temperature-REFERENCE_K)+.5*velocity**2)))
        self.state = [liquid, liquid*LIQUID_HEAT_CAPACITY*(t-REFERENCE_K)+self.surface_energy(radius),
                      *gas, 0., 0., 0.]
        self.tick = 0
        self.baseline = self.totals(self.state)
        self.step(self.state)

    def geometry(self, liquid):
        if liquid <= 0.:
            raise ValueError('spherical droplet exhausted')
        radius = math.cbrt(3*liquid/(4*math.pi*self.rho_liquid))
        if not .00001 <= radius <= .9*self.height:
            raise ValueError('spherical droplet radius bounds')
        return radius, (self.height-radius)/self.cells

    def surface_energy(self, radius):
        return self.tension * (4*math.pi*radius**2)

    def shells(self, radius):
        spacing = (self.height-radius)/self.cells
        edges = [*(radius+spacing*i for i in range(self.cells)),self.height]
        volumes = [(4*math.pi/3)*(b-a)*(a*a+a*b+b*b) for a,b in zip(edges,edges[1:])]
        return edges, volumes

    def fields(self, state):
        if len(state)!=5+3*self.cells or not all(math.isfinite(x) for x in state) or min(state[-3:-1])<0.:
            raise ValueError('spherical droplet state')
        radius, spacing = self.geometry(state[0])
        t = REFERENCE_K+(state[1]-self.surface_energy(radius))/(state[0]*LIQUID_HEAT_CAPACITY)
        flat = self.properties(t).pressure_pa
        saturation = flat*math.exp(2*self.tension/(radius*self.rho_liquid*R*t))
        gas = []
        _, volumes = self.shells(radius)
        for i,volume in enumerate(volumes):
            mass, momentum, energy = state[2+3*i:5+3*i]
            if mass<=0.:
                raise ValueError('spherical droplet gas shell exhausted')
            rho, velocity = mass/volume, momentum/mass
            temperature = REFERENCE_K+(energy/mass-self.latent-.5*velocity**2)/CV
            pressure = rho*R*temperature
            self.admit_gas(pressure,temperature,velocity)
            gas.append((rho,velocity,temperature,pressure,energy/mass))
        return radius, spacing, t, saturation, gas

    def derivative(self, state):
        radius, spacing, _, saturation, gas = self.fields(state)
        j, speed, velocity, pressure, traction, energy = self.interface(gas[0], saturation)
        if self.dt*max(abs(g[1])+abs(speed)+math.sqrt(GAMMA*g[3]/g[0]) for g in gas)>.4*spacing:
            raise ValueError('spherical droplet acoustic timestep bound')
        edges, _ = self.shells(radius)
        area = [4*math.pi*r*r for r in edges]
        exchange = j*energy+pressure*velocity
        faces = [(j,traction,exchange)]
        for i,(left,right) in enumerate(zip(gas,gas[1:]),1):
            faces.append(self.flux(left,right,speed*(1-i/self.cells)))
        rho,u,t,p,e = gas[-1]
        wall_pressure = self.flux(gas[-1],(rho,-u,t,p,e),0.)[1]
        faces.append((0.,wall_pressure,0.))
        rates, radial_forces = [], []
        for i,(left,right) in enumerate(zip(faces,faces[1:])):
            # Write momentum in pressure differences so constant pressure
            # is balanced exactly by the spherical geometric source.
            p_cell = gas[i][3]
            force = area[i]*(left[1]-p_cell)-area[i+1]*(right[1]-p_cell)
            radial_forces.append(force)
            rates.extend((area[i]*left[0]-area[i+1]*right[0],force,
                          area[i]*left[2]-area[i+1]*right[2]))
        # State[1] stores sensible liquid energy plus sigma*area.
        # Its partition changes with radius, without a second energy source.
        return [-area[0]*j,-area[0]*exchange,*rates,
                area[0]*max(0.,j),area[0]*max(0.,-j),math.fsum(radial_forces)]

    def checkpoint(self):
        payload = super().checkpoint()['payload']
        payload['schema'] = SCHEMA
        return dict(payload=payload,sha256=digest(payload))

    def report(self):
        report = super().report()
        radius,_,temperature,kelvin_pressure,_ = self.fields(self.state)
        surface = self.surface_energy(radius)
        laplace = 2*self.tension/radius
        stefan = report['stefan']
        stefan['liquid_pressure_pa'] += laplace
        stefan['radial_force_impulse_kg_m_s'] = stefan.pop('wall_impulse_kg_m_s')
        report['water_interface']['edges_m'] = [0.,*self.shells(radius)[0]]
        report['droplet'] = dict(radius_m=radius,area_m2=4*math.pi*radius**2,
            laplace_pressure_pa=laplace,flat_saturation_pressure_pa=self.properties(temperature).pressure_pa,
            curved_saturation_pressure_pa=kelvin_pressure,surface_energy_j=surface,
            surface_energy_change_j=surface-self.surface_energy(self.geometry(self.config['liquid_kg'])[0]),
            liquid_internal_energy_j=self.state[1]-surface,total_energy_j=self.totals(self.state)[2])
        return report
