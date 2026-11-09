"""Conservative evaporation and condensation of a captured sessile droplet."""
import hashlib
import math
from pathlib import Path

from chemistry_droplet_deposition import DropletDeposition, example as deposition_example
from chemistry_reactor import digest
from chemistry_humid_droplet import AIR_R
from chemistry_stefan_interface import CV, LIQUID_HEAT_CAPACITY, R, REFERENCE_K

SCHEMA = 'rc7-wetted-wall/v1'


def example(angle=90.):
    config = deposition_example()
    config.update(schema=SCHEMA,contact_angle_deg=angle)
    return config


def cap_diffusion_factor(angle):
    """Popov's isothermal spherical-cap diffusion factor."""
    if angle == math.pi/2:
        return 2.
    end, intervals = 32., 2048
    step = end/intervals
    terms = [(math.pi-angle)/math.pi]
    for i in range(1,intervals+1):
        tau = i*step
        value = ((1+math.cosh(2*angle*tau))/math.sinh(2*math.pi*tau)
                 *math.tanh((math.pi-angle)*tau))
        terms.append(value*(1 if i==intervals else 4 if i%2 else 2))
    return math.tan(angle/2)+4*step*math.fsum(terms)/3


class WettedWall(DropletDeposition):
    def __init__(self,library,config):
        if set(config)!=set(example()) or config['schema']!=SCHEMA or config['boundary']!='capture':
            raise ValueError('wetted wall configuration')
        degrees = config['contact_angle_deg']
        if type(degrees) not in (int,float) or not math.isfinite(degrees) or not 45.<=degrees<=135.:
            raise ValueError('wetted wall contact angle bounds')
        self.angle = math.radians(degrees)
        self.cosine = math.cos(self.angle)
        self.shape = cap_diffusion_factor(self.angle)
        base = dict(config,schema=deposition_example()['schema'])
        del base['contact_angle_deg']
        super().__init__(library,base)
        self.config = dict(self.config,schema=SCHEMA,contact_angle_deg=degrees)
        self.adapter = digest([self.adapter,hashlib.sha256(Path(__file__).read_bytes()).hexdigest()])

    def cap(self,mass):
        rho = self.core.exchange.rho
        c = self.cosine
        radius = math.cbrt(3*mass/(math.pi*rho*(1-c)**2*(2+c)))
        footprint = math.pi*(radius*math.sin(self.angle))**2
        area = 2*math.pi*radius**2*(1-c)
        # Solid–liquid replaces solid–gas according to Young's relation.
        energy = self.core.exchange.tension*(area-c*footprint)
        return radius,footprint,area,energy

    def wall_exchange(self,state):
        core = self.core
        exchange = core.exchange
        j = core.gas_end
        mass = state[j]
        radius,footprint,area,surface = self.cap(mass)
        temperature = REFERENCE_K+(state[j+2]-surface)/(mass*LIQUID_HEAT_CAPACITY)
        saturation = exchange.properties(temperature).pressure_pa*math.exp(
            2*exchange.tension/(radius*exchange.rho*R*temperature))
        wall = core.length if state[-2]>0. else 0.
        weights = core.stencil(wall)
        gas = core.gas_fields(state)
        rho,_,ambient,pressure,y = (math.fsum(gas[i][key]*w for i,w in weights) for key in range(5))
        return radius,footprint,area,surface,temperature,saturation,weights,rho,ambient,pressure,y

    def derivative(self,state):
        rates = super().derivative(state)
        if not state[-2]:
            return rates
        core = self.core
        exchange = core.exchange
        j = core.gas_end
        radius,_,_,_,temperature,saturation,weights,rho,ambient,pressure,y = self.wall_exchange(state)
        if not 0.<saturation<.2*pressure or not 0.<=y<.2:
            raise ValueError('wetted wall dilute vapor bound')
        surface_vapor = saturation/R
        ys = surface_vapor/(surface_vapor+(pressure-saturation)/AIR_R)
        conductance = math.pi*radius*math.sin(self.angle)*self.shape
        transfer = conductance*exchange.config['diffusivity_m2_s']*rho*(math.log1p(-y)-math.log1p(-ys))
        heat_coeff = conductance*exchange.config['conductivity_w_m_k']
        if self.dt*max(abs(transfer),heat_coeff/LIQUID_HEAT_CAPACITY)>.05*state[j]:
            raise ValueError('wetted wall timestep bound')
        energy = transfer*(exchange.latent+R*REFERENCE_K+(CV+R)*(temperature-REFERENCE_K))
        energy -= heat_coeff*(ambient-temperature)
        count = core.counts[0]
        for cell,w in weights:
            rates[4*cell+1] += count*w*transfer
            rates[4*cell+3] += count*w*energy
        rates[j] -= transfer
        rates[j+2] -= energy
        rates[j+4] += max(0.,transfer)
        rates[j+5] += max(0.,-transfer)
        return rates

    def rk2(self,state,dt):
        result = super().rk2(state,dt)
        if result[-2]:
            j = self.core.gas_end
            radius = math.cbrt(3*result[j]/(4*math.pi*self.core.exchange.rho))
            result[j+3] = self.core.length-radius if result[-2]>0. else radius
        return result

    def checkpoint(self):
        payload = super().checkpoint()['payload']
        payload['schema'] = SCHEMA
        return dict(payload=payload,sha256=digest(payload))

    def report(self):
        result = super().report()
        if self.state[-2]:
            radius,footprint,area,surface,t,saturation,*_ = self.wall_exchange(self.state)
            deposit = result['wall_deposition']['deposited_parcel']
            deposit['temperature_k'] = t
            result['wetted_wall'] = dict(contact_angle_deg=self.config['contact_angle_deg'],
                contact_radius_m=radius*math.sin(self.angle),footprint_m2=footprint,
                liquid_vapor_area_m2=area,surface_energy_j=self.core.counts[0]*surface,
                saturation_pressure_pa=saturation,temperature_k=t,
                vapor_flux_into_gas_kg_s=-self.core.counts[0]*self.derivative(self.state)[self.core.gas_end])
        return result
