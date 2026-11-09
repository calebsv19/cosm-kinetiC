"""Conservative sessile phase change with a mobile contact line."""
import copy
import functools
import hashlib
import math
from pathlib import Path

from chemistry_droplet_deposition import DropletDeposition, example as wall_example
from chemistry_reactor import digest
from chemistry_wetted_wall import WettedWall, cap_diffusion_factor

SCHEMA = 'rc7-dynamic-wetting/v1'


def example():
    wall = wall_example()
    return dict(schema=SCHEMA, wall=wall, n=wall['n'], chemistry=wall['chemistry'],
                initial_angle_deg=115., equilibrium_angle_deg=65.,
                mobility_m2_n_s=.2, hysteresis_half_width_deg=0., exchange_enabled=True)


@functools.lru_cache(maxsize=91)
def diffusion_node(degrees):
    return cap_diffusion_factor(math.radians(degrees))


class SessileStage(WettedWall):
    def cap(self,mass):
        radius,footprint,area,energy = super().cap(mass)
        energy += self.core.exchange.tension*(self.cosine-self.young_cosine)*footprint
        return radius,footprint,area,energy


class DynamicWetting:
    def __init__(self,library,config):
        if set(config)!=set(example()) or config['schema']!=SCHEMA:
            raise ValueError('dynamic wetting configuration')
        self.config = copy.deepcopy(config)
        self.wall = DropletDeposition(library,config['wall'])
        if (self.wall.periodic or any(config[k]!=self.wall.config[k] for k in ('n','chemistry'))
                or type(config['exchange_enabled']) is not bool):
            raise ValueError('dynamic wetting wall and exchange configuration')
        initial, equilibrium, self.mobility = (config[k] for k in (
            'initial_angle_deg','equilibrium_angle_deg','mobility_m2_n_s'))
        for value,lo,hi in ((initial,46.,134.),(equilibrium,46.,134.),(self.mobility,0.,10.)):
            if type(value) not in (int,float) or not math.isfinite(value) or not lo<=value<=hi:
                raise ValueError('dynamic wetting angle or mobility bounds')
        hysteresis = config['hysteresis_half_width_deg']
        if (type(hysteresis) not in (int,float) or not math.isfinite(hysteresis) or hysteresis<0.
                or equilibrium-hysteresis<45. or equilibrium+hysteresis>135.):
            raise ValueError('dynamic wetting hysteresis bounds')
        self.initial_angle = math.radians(initial)
        self.young_cosine = math.cos(math.radians(equilibrium))
        self.advancing_cosine = math.cos(math.radians(equilibrium+hysteresis))
        self.receding_cosine = math.cos(math.radians(equilibrium-hysteresis))
        self.j = self.wall.core.gas_end
        self.dt,self.engine = self.wall.dt,self.wall.engine
        self.adapter = digest([self.wall.adapter,*(hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
                              for name in ('chemistry_dynamic_wetting.py','chemistry_wetted_wall.py'))])
        # Wall state followed by contact radius and accumulated contact-line heat.
        self.state = [*self.wall.state,0.,0.]
        self.tick = 0
        self.validate(self.state)

    def stage(self,state):
        contact = state[-2]
        if not contact>0. or not state[self.j]>0.:
            raise ValueError('dynamic wetting contact radius or mass')
        volume = state[self.j]/self.wall.core.exchange.rho
        # V = pi*h*(3*a*a+h*h)/6, solved for the cap height h.
        angle = 2*math.atan(2*math.sinh(math.asinh(3*volume/(math.pi*contact**3))/3))
        degrees = math.degrees(angle)
        if not 45.<=degrees<=135.:
            raise ValueError('dynamic wetting instantaneous angle bounds')
        lower = min(134,math.floor(degrees))
        blend = degrees-lower
        view = SessileStage.__new__(SessileStage)
        view.__dict__.update(self.wall.__dict__)
        view.angle = angle
        view.cosine = math.cos(angle)
        view.young_cosine = self.young_cosine
        view.shape = (1-blend)*diffusion_node(lower)+blend*diffusion_node(lower+1)
        return view

    def derivative(self,state):
        view = self.stage(state)
        core = self.wall.core
        force_per_length = core.exchange.tension*(self.young_cosine-view.cosine)
        threshold = min(self.receding_cosine,max(self.advancing_cosine,view.cosine))
        speed = self.mobility*core.exchange.tension*(threshold-view.cosine)
        if self.dt*abs(speed)>.02*state[-2]:
            raise ValueError('dynamic wetting contact line timestep bound')
        rates = (view.derivative(state[:-2]) if self.config['exchange_enabled']
                 else self.wall.derivative(state[:-2]))
        heat = core.counts[0]*2*math.pi*state[-2]*force_per_length*speed
        # Contact-line heat comes from decreasing surface energy. The total
        # liquid energy already contains both reservoirs, so do not add it twice.
        return [*rates,speed,heat]

    def wet_step(self,state,dt):
        first = self.derivative(state)
        stage = [x+dt*k for x,k in zip(state,first)]
        second = self.derivative(stage)
        result = [x+.5*dt*(a+b) for x,a,b in zip(state,first,second)]
        radius = math.cbrt(3*result[self.j]/(4*math.pi*self.wall.core.exchange.rho))
        result[self.j+3] = self.wall.core.length-radius if result[-4]>0. else radius
        return result

    def step(self,state,time):
        if state[-4]:
            result = self.wet_step(state,self.dt)
        else:
            dry = state[:-2]
            candidate = self.wall.step(dry,time)
            if not candidate[-2]:
                result = [*candidate,0.,0.]
            else:
                elapsed = min(self.dt,max(0.,candidate[-1]-time))
                hit = self.wall.rk2(dry,elapsed)
                # The wall solve locates contact and books the impact once.
                # Replace its dry remainder with the active wet evolution.
                hit[self.j+1] = 0.
                hit[self.j+3] = candidate[self.j+3]
                hit[-4:] = candidate[-4:]
                c = math.cos(self.initial_angle)
                volume = hit[self.j]/self.wall.core.exchange.rho
                contact = math.sin(self.initial_angle)*math.cbrt(3*volume/(math.pi*(1-c)**2*(2+c)))
                result = self.wet_step([*hit,contact,0.],self.dt-elapsed)
        self.validate(result)
        return result

    def validate(self,state):
        if (len(state)!=len(self.wall.state)+2 or not all(math.isfinite(v) for v in state)
                or min(state[-2:])<0. or (not state[-4] and any(state[-2:]))):
            raise ValueError('dynamic wetting state')
        self.wall.validate(state[:-2])
        if state[-4]:
            view = self.stage(state)
            view.wall_exchange(state[:-2])
            self.derivative(state)

    advance = DropletDeposition.advance
    restore = DropletDeposition.restore

    def checkpoint(self):
        payload = dict(schema=SCHEMA,config=digest(self.config),adapter=self.adapter,engine=self.engine,
                       state=self.state.copy(),tick=self.tick)
        return dict(payload=payload,sha256=digest(payload))

    def report(self):
        wall = copy.copy(self.wall)
        wall.state = self.state[:-2]
        wall.tick = self.tick
        report = wall.report()
        if self.state[-4]:
            view = self.stage(self.state)
            _,footprint,area,surface,t,saturation,*_ = view.wall_exchange(self.state[:-2])
            rates = self.derivative(self.state)
            report['wall_deposition']['deposited_parcel']['temperature_k'] = t
            report['dynamic_wetting'] = dict(contact_angle_deg=math.degrees(view.angle),
                equilibrium_angle_deg=self.config['equilibrium_angle_deg'],
                hysteresis_half_width_deg=self.config['hysteresis_half_width_deg'],
                contact_radius_m=self.state[-2],contact_speed_m_s=rates[-2],
                footprint_m2=footprint,liquid_vapor_area_m2=area,
                contact_line_heat_j=self.state[-1],surface_energy_j=wall.core.counts[0]*surface,
                saturation_pressure_pa=saturation,temperature_k=t,
                vapor_flux_into_gas_kg_s=-wall.core.counts[0]*rates[self.j])
        return report
