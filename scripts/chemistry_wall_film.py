"""Conservative periodic lubrication film coupled to the solved moist carrier."""
import copy
import hashlib
import math
from pathlib import Path

from chemistry_droplet_carrier import AIR_MOLAR_MASS, DropletCarrier, example as gas_example
from chemistry_humid_droplet import AIR_R
from chemistry_reactor import digest
from chemistry_stefan_interface import CV, LIQUID_HEAT_CAPACITY, MOLAR_MASS, R, REFERENCE_K, StefanInterface

SCHEMA = 'rc7-wall-film/v1'


def example(cells=24):
    gas = gas_example(cells)
    gas.update(droplets=[],length_m=.0024,area_m2=1e-7,dt_s=5e-8,
               gas_pressure_pa=[101325.]*cells,gas_velocity_m_s=[0.]*cells)
    return dict(schema=SCHEMA,carrier=gas,n=gas['n'],chemistry=gas['chemistry'],
                width_m=.001,viscosity_pa_s=.001,gravity_m_s2=9.81,tension_n_m=.072,
                exchange_length_m=2e-4,exchange_enabled=True,
                film_height_m=[10e-6]*cells,film_temperature_k=[298.15]*cells,
                deposit=dict(cell=cells//2,mass_kg=4e-10,total_energy_j=0.))


class WallFilm:
    def __init__(self,library,config):
        if set(config)!=set(example()) or config['schema']!=SCHEMA:
            raise ValueError('wall film configuration')
        self.config = copy.deepcopy(config)
        self.gas = DropletCarrier(library,config['carrier'])
        if self.gas.counts or any(config[k]!=self.gas.config[k] for k in ('n','chemistry')):
            raise ValueError('wall film requires a gas-only carrier and matching display')
        self.cells,self.dx,self.dt = self.gas.cells,self.gas.dx,self.gas.dt
        self.engine = self.gas.engine
        self.adapter = digest([self.gas.adapter,hashlib.sha256(Path(__file__).read_bytes()).hexdigest()])
        self.rho = self.gas.exchange.rho
        self.j = self.gas.gas_end
        for key,lo,hi in (('width_m',1e-6,1.),('viscosity_pa_s',1e-4,1.),
                          ('gravity_m_s2',0.,30.),('tension_n_m',0.,.2),
                          ('exchange_length_m',1e-6,.1)):
            x = config[key]
            if type(x) not in (int,float) or not math.isfinite(x) or not lo<=x<=hi:
                raise ValueError(f'wall film parameter bounds for {key}')
        if type(config['exchange_enabled']) is not bool:
            raise ValueError('wall film exchange flag')
        self.width = config['width_m']
        self.area = self.width*self.dx
        self.mass_per_height = self.rho*self.area
        self.gravity = config['gravity_m_s2']
        self.sigma = config['tension_n_m']
        self.mobility_scale = self.rho**2*self.width/(3*config['viscosity_pa_s']*self.dx)
        self.bond_scale = self.sigma*self.width/self.dx
        heights,temperatures = config['film_height_m'],config['film_temperature_k']
        if (len(heights)!=self.cells or len(temperatures)!=self.cells or
                any(type(x) not in (int,float) or not math.isfinite(x) for x in [*heights,*temperatures])):
            raise ValueError('wall film initial profile')
        mass = [h*self.mass_per_height for h in heights]
        energy = [m*LIQUID_HEAT_CAPACITY*(t-REFERENCE_K) for m,t in zip(mass,temperatures)]
        deposit = config['deposit']
        if set(deposit)!={'cell','mass_kg','total_energy_j'}:
            raise ValueError('wall film deposit fields')
        cell,amount,heat = (deposit[k] for k in ('cell','mass_kg','total_energy_j'))
        if (type(cell) is not int or not 0<=cell<self.cells or any(
                type(x) not in (int,float) or not math.isfinite(x) for x in (amount,heat))
                or amount<0. or (not amount and heat)):
            raise ValueError('wall film deposit inventory')
        background_potential = math.fsum(self.potential(heights))
        mass[cell] += amount
        potential = self.potential([m/self.mass_per_height for m in mass])
        # Geometry is part of the incoming deposit's total energy.
        energy[cell] += heat-(math.fsum(potential)-background_potential)
        self.initial_film_mass = math.fsum(mass)
        self.state = [*self.gas.state,*mass,*[e+p for e,p in zip(energy,potential)],0.,0.,0.]
        self.tick = 0
        self.baseline = self.totals(self.state)
        self.step(self.state)

    def potential(self,height):
        bonds = [.5*self.bond_scale*(height[(i+1)%self.cells]-h)**2 for i,h in enumerate(height)]
        return [.5*self.mass_per_height*self.gravity*h*h+.5*(bonds[i-1]+bonds[i])
                for i,h in enumerate(height)]

    def fields(self,state):
        if (len(state)!=self.j+2*self.cells+3 or not all(math.isfinite(v) for v in state)
                or min(state[-3:])<0.):
            raise ValueError('wall film state')
        mass = state[self.j:self.j+self.cells]
        height = [m/self.mass_per_height for m in mass]
        if (min(height)<1e-7 or max(height)>.2*self.dx or
                max(abs(h-height[i-1]) for i,h in enumerate(height))>.1*self.dx):
            raise ValueError('wall film wet thickness or slope bounds')
        potential = self.potential(height)
        specific_u = [(e-p)/m for e,p,m in zip(state[self.j+self.cells:-3],potential,mass)]
        temperature = [REFERENCE_K+u/LIQUID_HEAT_CAPACITY for u in specific_u]
        if any(not 283.15<=t<=323.15 for t in temperature):
            raise ValueError('wall film temperature bounds')
        return height,specific_u,temperature,potential

    def derivative(self,state):
        height,u,t,_ = self.fields(state)
        gas = self.gas.gas_fields(state)
        gas_rate = self.gas.derivative(state[:self.j])
        n = self.cells
        potential = [self.gravity*h-self.sigma/(self.rho*self.dx**2)*
                     (height[i-1]-2*h+height[(i+1)%n]) for i,h in enumerate(height)]
        mobility = [self.mobility_scale*(.5*(h+height[(i+1)%n]))**3 for i,h in enumerate(height)]
        if self.dt*max(mobility)/self.mass_per_height*(4*self.gravity+16*self.sigma/(self.rho*self.dx**2))>.2:
            raise ValueError('wall film lubrication timestep bound')
        flux,energy_flux,heat = [],[],[]
        for i,k in enumerate(mobility):
            right = (i+1)%n
            jump = potential[i]-potential[right]
            flow = k*jump
            flux.append(flow)
            energy_flux.append(flow*((u[i] if flow>=0. else u[right])+.5*(potential[i]+potential[right])))
            heat.append(flow*jump)
        mass_rate = [flux[i-1]-flux[i] for i in range(n)]
        energy_rate = [energy_flux[i-1]-energy_flux[i] for i in range(n)]
        evaporated,condensed = [],[]
        for i,(rho,_,ambient,pressure,y,_) in enumerate(gas):
            transfer = energy = 0.
            if self.config['exchange_enabled']:
                saturation = self.gas.exchange.properties(t[i]).pressure_pa
                vapor = saturation/R
                ys = vapor/(vapor+(pressure-saturation)/AIR_R)
                scale = self.area/self.config['exchange_length_m']
                transfer = scale*self.gas.exchange.config['diffusivity_m2_s']*rho*(ys-y)
                energy = transfer*(self.gas.latent+R*REFERENCE_K+(CV+R)*(t[i]-REFERENCE_K))
                energy -= scale*self.gas.exchange.config['conductivity_w_m_k']*(ambient-t[i])
            mass_rate[i] -= transfer
            energy_rate[i] -= energy
            gas_rate[4*i+1] += transfer
            gas_rate[4*i+3] += energy
            evaporated.append(max(0.,transfer))
            condensed.append(max(0.,-transfer))
        # Each cell owns half of each capillary bond. Account for the
        # change of that split as both heights evolve.
        for i,h in enumerate(height):
            right = (i+1)%n
            bond_work = .5*self.bond_scale*(height[right]-h)*(mass_rate[i]+mass_rate[right])/self.mass_per_height
            energy_rate[i] += bond_work
            energy_rate[right] -= bond_work
        return [*gas_rate,*mass_rate,*energy_rate,math.fsum(evaporated),math.fsum(condensed),math.fsum(heat)]

    def totals(self,state):
        return (math.fsum(state[:self.j:4]),
                math.fsum([*state[1:self.j:4],*state[self.j:self.j+self.cells]]),
                math.fsum(state[2:self.j:4]),
                math.fsum([*state[3:self.j:4],*state[self.j+self.cells:-3]]))

    def residuals(self,state):
        return [a-b for a,b in zip(self.totals(state),self.baseline)]

    def phase_residual(self,state):
        return math.fsum([*state[self.j:self.j+self.cells],state[-3],-state[-2],-self.initial_film_mass])

    def validate(self,state):
        self.derivative(state)
        phase_error = self.phase_residual(state)
        if abs(phase_error)>2e-10*self.initial_film_mass or any(abs(r)>2e-10*max(abs(b),f) for r,b,f in zip(
                self.residuals(state),self.baseline,(1e-18,1e-18,1e-16,1e-12))):
            raise ValueError('wall film conservation ledger')

    step = StefanInterface.step
    advance = StefanInterface.advance
    restore = StefanInterface.restore

    def checkpoint(self):
        payload = dict(schema=SCHEMA,config=digest(self.config),adapter=self.adapter,engine=self.engine,
                       state=self.state.copy(),tick=self.tick)
        return dict(payload=payload,sha256=digest(payload))

    def report(self):
        gas = self.gas.gas_fields(self.state)
        height,u,t,potential = self.fields(self.state)
        dry,water,_,energy = self.totals(self.state)
        air_residual,water_residual,momentum_residual,energy_residual = self.residuals(self.state)
        liquid = self.mass_per_height*math.fsum(height)
        return dict(tick=self.tick,time_s=self.tick*self.dt,
            totals_mol=[liquid/MOLAR_MASS,(water-liquid)/MOLAR_MASS,dry/AIR_MOLAR_MASS],
            concentration_mol_m3=[[self.rho*h*self.area/self.gas.volume/MOLAR_MASS,
                                    rho*y/MOLAR_MASS,rho*(1-y)/AIR_MOLAR_MASS] for h,(rho,_,_,_,y,_) in zip(height,gas)],
            extent_mol=[x/MOLAR_MASS for x in self.state[-3:-1]],
            species_budget_residual_mol=[0.,water_residual/MOLAR_MASS,air_residual/AIR_MOLAR_MASS],
            thermal=dict(temperature_k=[g[2] for g in gas],property_id='Moist gas and wall film',
                         internal_ledger_residual_j=energy_residual),
            wall_film=dict(height_m=height,liquid_temperature_k=t,liquid_kg=liquid,
                gas_vapor_kg=math.fsum(self.state[1:self.j:4]),surface_and_gravity_j=math.fsum(potential),
                liquid_sensible_j=math.fsum(v*h*self.mass_per_height for v,h in zip(u,height)),
                viscous_heat_j=self.state[-1],evaporated_kg=self.state[-3],condensed_kg=self.state[-2],
                total_energy_j=energy,water_residual_kg=water_residual,energy_residual_j=energy_residual,
                phase_residual_kg=self.phase_residual(self.state),dry_air_residual_kg=air_residual,gas_momentum_residual_kg_m_s=momentum_residual))
