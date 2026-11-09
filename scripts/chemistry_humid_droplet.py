"""Quasi-steady spherical vapor diffusion and heat transfer in humid air."""
import copy
import hashlib
import math
from pathlib import Path

from chemistry_reactor import digest
from chemistry_stefan_interface import CV, LIQUID_HEAT_CAPACITY, MOLAR_MASS, R, REFERENCE_K, SPECIES, StefanInterface, example as planar_example

SCHEMA = 'rc7-humid-droplet/v1'
AIR_R = 287.05


def example(rh=.5):
    return dict(schema=SCHEMA, chemistry=copy.deepcopy(SPECIES), n=[25,1,1],
                initial_radius_m=25e-6, outer_radius_m=.002, pressure_pa=101325.,
                relative_humidity=rh, ambient_temperature_k=298.15,
                liquid_temperature_k=298.15, tension_n_m=.072, diffusivity_m2_s=2.5e-5,
                conductivity_w_m_k=.026, dt_s=.0001, isothermal=False)


class HumidDroplet:
    def __init__(self, library, config):
        if set(config)!=set(example()) or config['schema']!=SCHEMA or config['chemistry']!=SPECIES:
            raise ValueError('humid droplet configuration')
        self.config = copy.deepcopy(config)
        nx,ny,nz = config['n']
        if any(type(x) is not int for x in (nx,ny,nz)) or not 5<=nx<=257 or (ny,nz)!=(1,1):
            raise ValueError('humid droplet grid')
        self.cells = nx-1
        bounds = dict(initial_radius_m=(2e-6,.001),outer_radius_m=(2e-5,.1),pressure_pa=(70000.,120000.),
                      relative_humidity=(0.,1.2),ambient_temperature_k=(283.15,323.15),
                      liquid_temperature_k=(283.15,323.15),tension_n_m=(0.,.2),
                      diffusivity_m2_s=(1e-6,1e-4),conductivity_w_m_k=(.005,.1),dt_s=(1e-8,.1))
        for key,(lo,hi) in bounds.items():
            value = config[key]
            if type(value) not in (int,float) or not math.isfinite(value) or not lo<=value<=hi:
                raise ValueError(f'humid droplet parameter bounds for {key}')
        if type(config['isothermal']) is not bool:
            raise ValueError('humid droplet thermal boundary')
        self.radius0 = config['initial_radius_m']
        self.outer = config['outer_radius_m']
        self.ambient = config['ambient_temperature_k']
        self.tension = config['tension_n_m']
        self.dt = config['dt_s']
        self.pressure = config['pressure_pa']
        seed = StefanInterface(library,planar_example())
        self.properties = seed.properties
        self.rho = seed.rho_liquid
        self.latent = seed.latent
        self.engine = seed.engine
        self.adapter = digest([seed.adapter,hashlib.sha256(Path(__file__).read_bytes()).hexdigest()])
        ambient_vapor_pressure = config['relative_humidity']*self.properties(self.ambient).pressure_pa
        self.y_outer = self.mass_fraction(ambient_vapor_pressure)
        self.gas_density = (self.pressure-ambient_vapor_pressure)/(AIR_R*self.ambient)+ambient_vapor_pressure/(R*self.ambient)
        mass = 4*math.pi/3*self.rho*self.radius0**3
        self.initial = [mass,self.drop_energy(mass,self.radius0,config['liquid_temperature_k'])]
        # Liquid mass, liquid plus surface energy, evaporated, condensed,
        # gas enthalpy received, conducted heat into drop, bath heat into drop.
        self.state = [*self.initial,0.,0.,0.,0.,0.]
        self.tick = 0
        self.step(self.state)

    def mass_fraction(self, vapor_pressure):
        if not 0.<=vapor_pressure<.2*self.pressure:
            raise ValueError('humid droplet dilute vapor bound')
        vapor = vapor_pressure/R
        return vapor/(vapor+(self.pressure-vapor_pressure)/AIR_R)

    def drop_energy(self, mass, radius, temperature):
        return mass*LIQUID_HEAT_CAPACITY*(temperature-REFERENCE_K)+4*math.pi*self.tension*radius**2

    def fields(self, state):
        if len(state)!=7 or not all(math.isfinite(v) for v in state) or state[0]<=0. or min(state[2:4])<0.:
            raise ValueError('humid droplet state')
        mass,energy = state[:2]
        radius = math.cbrt(3*mass/(4*math.pi*self.rho))
        if not 2e-6<=radius<=.5*self.outer:
            raise ValueError('humid droplet radius or chamber bound')
        surface = 4*math.pi*self.tension*radius**2
        temperature = REFERENCE_K+(energy-surface)/(mass*LIQUID_HEAT_CAPACITY)
        p_sat = self.properties(temperature).pressure_pa*math.exp(2*self.tension/(radius*self.rho*R*temperature))
        return radius,temperature,surface,self.mass_fraction(p_sat)

    def derivative(self, state):
        radius,t,_,ys = self.fields(state)
        geometry = 4*math.pi/(1/radius-1/self.outer)
        # Steady spherical Stefan diffusion with zero dry-air mass flux.
        transfer = geometry*self.config['diffusivity_m2_s']*self.gas_density*(
            math.log1p(-self.y_outer)-math.log1p(-ys))
        heat_coeff = geometry*self.config['conductivity_w_m_k']
        if self.dt*heat_coeff>.05*state[0]*LIQUID_HEAT_CAPACITY or self.dt*abs(transfer)>.05*state[0]:
            raise ValueError('humid droplet timestep bound')
        conducted = heat_coeff*(self.ambient-t)
        enthalpy = transfer*(self.latent+R*REFERENCE_K+(CV+R)*(t-REFERENCE_K))
        bath = 0.
        if self.config['isothermal']:
            required = -transfer*(LIQUID_HEAT_CAPACITY*(t-REFERENCE_K)+2*self.tension/(self.rho*radius))
            bath = required-conducted+enthalpy
        return [-transfer,conducted-enthalpy+bath,max(transfer,0.),max(-transfer,0.),enthalpy,conducted,bath]

    def residuals(self, state):
        return (math.fsum([state[0],-self.initial[0],state[2],-state[3]]),
                math.fsum([state[1],-self.initial[1],state[4],-state[5],-state[6]]))

    def validate(self, state):
        self.derivative(state)
        m,e = self.residuals(state)
        if abs(m)>2e-11*self.initial[0] or abs(e)>2e-11*max(1e-10,abs(self.initial[1]),abs(state[4])):
            raise ValueError('humid droplet conservation ledger')

    def step(self, state):
        first = self.derivative(state)
        mid = [x+.5*self.dt*v for x,v in zip(state,first)]
        final = [x+self.dt*v for x,v in zip(state,self.derivative(mid))]
        if self.config['isothermal']:
            radius = math.cbrt(3*final[0]/(4*math.pi*self.rho))
            target = self.drop_energy(final[0],radius,self.config['liquid_temperature_k'])
            final[6] += target-final[1]
            final[1] = target
        self.validate(final)
        return final

    advance = StefanInterface.advance
    restore = StefanInterface.restore

    def checkpoint(self):
        p = dict(schema=SCHEMA,config=digest(self.config),adapter=self.adapter,engine=self.engine,
                 state=self.state.copy(),tick=self.tick)
        return dict(payload=p,sha256=digest(p))

    def report(self):
        s = self.state
        radius,t,surface,ys = self.fields(s)
        mass_error,energy_error = self.residuals(s)
        dx = (self.outer-radius)/self.cells
        centers = [radius+(i+.5)*dx for i in range(self.cells)]
        weights = [(1/x-1/self.outer)/(1/radius-1/self.outer) for x in centers]
        y = [-math.expm1(w*math.log1p(-ys)+(1-w)*math.log1p(-self.y_outer)) for w in weights]
        return dict(tick=self.tick,time_s=self.tick*self.dt,totals_mol=[s[0]/MOLAR_MASS,0.],
            concentration_mol_m3=[[self.rho/MOLAR_MASS,0.],*[[0.,v*self.gas_density/MOLAR_MASS] for v in y]],
            extent_mol=[s[2]/MOLAR_MASS,s[3]/MOLAR_MASS],species_budget_residual_mol=[mass_error/MOLAR_MASS,0.],
            thermal=dict(temperature_k=[t,*[self.ambient+w*(t-self.ambient) for w in weights]],
                         property_id='IAPWS and humid-air diffusion',internal_ledger_residual_j=energy_error),
            water_interface=dict(position_m=radius,edges_m=[0.,*(radius+i*dx for i in range(self.cells)),self.outer],
                displacement_m=radius-self.radius0,saturation_pressure_pa=self.properties(t).pressure_pa,
                liquid_temperature_k=t,bath_heat_into_chamber_j=s[6],energy_residual_j=energy_error),
            humid_droplet=dict(radius_m=radius,temperature_k=t,surface_energy_j=surface,liquid_kg=s[0],
                vapor_mass_fraction=y,evaporated_kg=s[2],condensed_kg=s[3],net_mass_into_air_kg=s[2]-s[3],
                vapor_enthalpy_into_air_j=s[4],conductive_heat_into_drop_j=s[5],bath_heat_into_drop_j=s[6],
                mass_residual_kg=mass_error,energy_residual_j=energy_error))
