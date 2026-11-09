"""Low-Reynolds-number droplet motion with humid-air phase exchange."""
import copy
import hashlib
import math
from pathlib import Path

from chemistry_humid_droplet import HumidDroplet, example as humid_example
from chemistry_reactor import digest

SCHEMA = 'rc7-settling-droplet/v1'


def example():
    exchange = dict(humid_example(.8),initial_radius_m=10e-6)
    return dict(schema=SCHEMA, exchange=exchange, n=exchange['n'], chemistry=exchange['chemistry'],
                air_velocity_m_s=0., initial_velocity_m_s=0., gravity_m_s2=9.81,
                viscosity_pa_s=1.81e-5)


class SettlingDroplet:
    def __init__(self, library, config):
        if set(config)!=set(example()) or config['schema']!=SCHEMA:
            raise ValueError('settling droplet configuration')
        self.config = copy.deepcopy(config)
        self.exchange = HumidDroplet(library,config['exchange'])
        self.dt = self.exchange.dt
        self.u,self.gravity,self.viscosity,v = (config[k] for k in (
            'air_velocity_m_s','gravity_m_s2','viscosity_pa_s','initial_velocity_m_s'))
        for value,lo,hi in ((self.u,-1.,1.),(v,-1.,1.),(self.gravity,0.,20.),(self.viscosity,1e-6,1e-4)):
            if type(value) not in (int,float) or not math.isfinite(value) or not lo<=value<=hi:
                raise ValueError('settling droplet parameters')
        if any(config[k]!=self.exchange.config[k] for k in ('n','chemistry')):
            raise ValueError('settling droplet display config')
        self.initial_momentum = self.exchange.state[0]*v
        self.initial_kinetic = .5*self.exchange.state[0]*v*v
        self.engine = self.exchange.engine
        self.adapter = digest([self.exchange.adapter,hashlib.sha256(Path(__file__).read_bytes()).hexdigest()])
        # Thermodynamic state, velocity, displacement, gravity/drag/mass impulses,
        # gravity work, carrier work, drag dissipation, transferred kinetic energy.
        self.state = [*self.exchange.state,v,*([0.]*8)]
        self.tick = 0
        self.validate(self.state)

    def residuals(self, state):
        m,v = state[0],state[7]
        _,ig,drag,transfer,wg,wa,heat,kinetic = state[8:]
        return (math.fsum([m*v,-self.initial_momentum,-ig,-drag,-transfer]),
                math.fsum([.5*m*v*v,-self.initial_kinetic,-wg,-wa,heat,-kinetic]))

    def validate(self, state):
        if len(state)!=16 or not all(math.isfinite(x) for x in state) or state[14]<0.:
            raise ValueError('settling droplet state')
        self.exchange.validate(state[:7])
        radius = self.exchange.fields(state[:7])[0]
        slip = abs(state[7]-self.u)
        reynolds = 2*radius*self.exchange.gas_density*slip/self.viscosity
        peclet = 2*radius*slip/self.exchange.config['diffusivity_m2_s']
        thermal_peclet = (2*radius*slip*self.exchange.gas_density*1010./
                          self.exchange.config['conductivity_w_m_k'])
        if max(reynolds,peclet,thermal_peclet)>.1:
            raise ValueError('settling droplet creeping-flow or transport bound')
        p,e = self.residuals(state)
        if abs(p)>2e-20 or abs(e)>2e-21:
            raise ValueError('settling droplet mechanical ledger')

    def step(self, state):
        initial = state[:7]
        final = self.exchange.step(initial)
        m = .5*(initial[0]+final[0])
        radius = math.cbrt(3*m/(4*math.pi*self.exchange.rho))
        beta = 6*math.pi*self.viscosity*radius
        rate = beta/m
        if self.dt*rate>.25:
            raise ValueError('settling droplet timestep bound')
        v = state[7]
        acceleration = self.gravity*(1-self.exchange.gas_density/self.exchange.rho)
        v_next = v+self.dt*(acceleration+rate*(self.u-v))/(1+.5*self.dt*rate)
        midpoint = .5*(v+v_next)
        travel = self.dt*midpoint
        gravity_impulse = m*acceleration*self.dt
        drag_impulse = beta*(self.u-midpoint)*self.dt
        delta_mass = final[0]-initial[0]
        rates = [travel,gravity_impulse,drag_impulse,delta_mass*midpoint,
                 gravity_impulse*midpoint,drag_impulse*self.u,beta*(self.u-midpoint)**2*self.dt,
                 .25*delta_mass*(v*v+v_next*v_next)]
        candidate = [*final,v_next,*[x+y for x,y in zip(state[8:],rates)]]
        self.validate(candidate)
        return candidate

    advance = HumidDroplet.advance
    restore = HumidDroplet.restore

    def checkpoint(self):
        p = dict(schema=SCHEMA,config=digest(self.config),adapter=self.adapter,engine=self.engine,
                 state=self.state.copy(),tick=self.tick)
        return dict(payload=p,sha256=digest(p))

    def report(self):
        # The exchange inspector is a read-only view of the thermodynamic state.
        view = copy.copy(self.exchange)
        view.state = self.state[:7]
        view.tick = self.tick
        report = view.report()
        p,e = self.residuals(self.state)
        v,z,ig,drag,transfer,wg,wa,heat,kinetic = self.state[7:]
        radius = report['humid_droplet']['radius_m']
        report['settling_droplet'] = dict(velocity_m_s=v,downward_displacement_m=z,
            reynolds=2*radius*self.exchange.gas_density*abs(v-self.u)/self.viscosity,
            gravity_impulse_kg_m_s=ig,drag_impulse_kg_m_s=drag,phase_impulse_kg_m_s=transfer,
            gravity_work_j=wg,carrier_work_j=wa,drag_dissipation_j=heat,phase_kinetic_transfer_j=kinetic,
            momentum_residual_kg_m_s=p,mechanical_energy_residual_j=e)
        return report
