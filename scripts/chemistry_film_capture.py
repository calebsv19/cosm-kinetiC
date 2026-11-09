"""Single inelastic droplet capture into a continuously evolving wet film."""
import copy
import hashlib
import math
from pathlib import Path

from chemistry_droplet_carrier import DropletCarrier
from chemistry_reactor import digest
from chemistry_stefan_interface import MOLAR_MASS, StefanInterface
from chemistry_wall_film import WallFilm, example as film_example

SCHEMA = 'rc7-film-capture/v1'


def example():
    film = film_example()
    film.update(width_m=2e-6, deposit=dict(cell=12, mass_kg=0., total_energy_j=0.))
    film['carrier']['gas_velocity_m_s'] = [.005]*24
    return dict(schema=SCHEMA, film=film, chemistry=film['chemistry'], n=film['n'],
                droplet=dict(position_m=.00125, velocity_m_s=.01, radius_m=4e-6,
                             temperature_k=301.15, count=1),
                center_height_m=17e-6, approach_speed_m_s=.08)


class FilmCapture:
    def __init__(self, library, config):
        if set(config)!=set(example()) or config['schema']!=SCHEMA:
            raise ValueError('film capture configuration')
        self.config = copy.deepcopy(config)
        self.film = WallFilm(library, config['film'])
        if any(config[key]!=self.film.config[key] for key in ('n','chemistry')):
            raise ValueError('film capture display configuration')
        parcel = config['droplet']
        if parcel.get('count')!=1:
            raise ValueError('film capture requires one droplet')
        self.flight = DropletCarrier(library, dict(self.film.config['carrier'], droplets=[parcel]))
        self.engine = self.film.engine
        self.adapter = digest([self.film.adapter, self.flight.adapter,
                               hashlib.sha256(Path(__file__).read_bytes()).hexdigest()])
        self.dt = self.film.dt
        self.gas_end = self.film.j
        self.film_end = len(self.film.state)
        self.speed = config['approach_speed_m_s']
        height = config['center_height_m']
        if (type(self.speed) not in (float,int) or not 0.<self.speed<=.1 or
                type(height) not in (float,int) or not 0.<height<=self.film.dx):
            raise ValueError('film capture normal approach bounds')
        self.normal_specific_ke = .5*self.speed**2
        drop = self.flight.state[self.gas_end:]
        self.initial_drop_mass = drop[0]
        # Tail after film and the carrier parcel: normal center height, then
        # event time, captured mass, energy, wall impulse and film evap at impact.
        self.state = [*self.film.state, *drop, height, 0.,0.,0.,0.,0.]
        self.tick = 0
        self.baseline = self.totals(self.state)
        if self.clearance(self.state)<=0.:
            raise ValueError('film capture requires an initially airborne droplet')
        self.validate(self.state)

    def flight_state(self, state):
        return [*state[:self.gas_end], *state[self.film_end:self.film_end+6]]

    def clearance(self, state):
        drop = self.flight.particle(self.flight_state(state), 0)
        heights = self.film.fields(state[:self.film_end])[0]
        surface = math.fsum(heights[i]*w for i,w in self.flight.stencil(drop[3]))
        return state[-6]-drop[4]-surface

    def derivative(self, state):
        film = state[:self.film_end]
        rate = self.film.derivative(film)
        if state[-5]>0.:
            return [*rate, *([0.]*12)]
        # The two component derivatives share the same gas transport operator.
        # Keep it once, adding only the droplet exchange to the film's gas rate.
        coupled = self.flight.derivative(self.flight_state(state))
        carrier = self.film.gas.derivative(state[:self.gas_end])
        for i,(new,old) in enumerate(zip(coupled,carrier)):
            rate[i] += new-old
        # The prescribed normal speed gives exchanged liquid mass a normal
        # kinetic energy. The 1D gas ledger receives it without normal momentum.
        for i,w in self.flight.stencil(state[self.film_end+3]):
            rate[4*i+3] -= w*coupled[self.gas_end]*self.normal_specific_ke
        return [*rate, *coupled[self.gas_end:], -self.speed, 0.,0.,0.,0.,0.]

    def integrate(self, state, dt):
        first = self.derivative(state)
        second = self.derivative([x+dt*r for x,r in zip(state,first)])
        return [x+.5*dt*(a+b) for x,a,b in zip(state,first,second)]

    def capture(self, state, time):
        state = state.copy()
        m,px,energy,x = state[self.film_end:self.film_end+4]
        energy += m*self.normal_specific_ke
        _,_,_,old_potential = self.film.fields(state[:self.film_end])
        stencil = self.flight.stencil(x)
        masses = state[self.gas_end:self.gas_end+self.film.cells]
        for i,w in stencil:
            masses[i] += w*m
        new_heights = [mass/self.film.mass_per_height for mass in masses]
        changes = [new-old for new,old in zip(self.film.potential(new_heights),old_potential)]
        sensible = energy-math.fsum(changes)
        energy_start = self.gas_end+self.film.cells
        for i,change in enumerate(changes):
            state[energy_start+i] += change
        for i,w in stencil:
            state[energy_start+i] += w*sensible
        state[self.gas_end:energy_start] = masses
        state[self.film_end:self.film_end+3] = [0.,0.,0.]
        state[-5:] = [time,m,energy,px,state[self.film_end-3]]
        self.validate(state)
        return state

    def step(self, state, time):
        end = self.integrate(state, self.dt)
        if state[-5]==0. and self.clearance(end)<=0.:
            low,high = 0.,self.dt
            for _ in range(24):
                mid = .5*(low+high)
                if self.clearance(self.integrate(state,mid))>0.:
                    low = mid
                else:
                    high = mid
            impact = self.capture(self.integrate(state,high),time+high)
            end = self.integrate(impact,self.dt-high)
        self.validate(end)
        return end

    def advance(self, steps):
        if type(steps) is not int or not 0<=steps<=10000 or self.tick+steps>2**53:
            raise ValueError('film capture step count')
        candidate = self.state
        for tick in range(self.tick,self.tick+steps):
            candidate = self.step(candidate,tick*self.dt)
        self.state = candidate
        self.tick += steps

    def totals(self, state):
        dry,water,px,energy = self.film.totals(state[:self.film_end])
        m,p,e = state[self.film_end:self.film_end+3]
        return dry,math.fsum((water,m)),math.fsum((px,p,state[-2])),math.fsum((energy,e,m*self.normal_specific_ke))

    def residuals(self, state):
        return [x-y for x,y in zip(self.totals(state),self.baseline)]

    def validate(self, state):
        if len(state)!=self.film_end+12 or not all(math.isfinite(x) for x in state):
            raise ValueError('film capture state shape or value')
        self.derivative(state)
        time,mass,energy,px,evap_at_impact = state[-5:]
        m,p,e,_,evap,cond = state[self.film_end:self.film_end+6]
        if (min(time,mass,evap,cond,evap_at_impact)<0. or
                (time==0. and (any(state[-4:]) or self.clearance(state)<=0.)) or
                (time>0. and (mass<=0. or any((m,p,e)) or evap_at_impact>state[self.film_end-3]))):
            raise ValueError('film capture event state')
        parcel_error = math.fsum((m,mass,evap,-cond,-self.initial_drop_mass))
        film_error = self.film.phase_residual(state[:self.film_end])-mass
        if (abs(parcel_error)>2e-10*self.initial_drop_mass or
                abs(film_error)>2e-10*self.film.initial_film_mass or
                any(abs(r)>2e-10*max(abs(b),f) for r,b,f in zip(
                    self.residuals(state),self.baseline,(1e-18,1e-18,1e-16,1e-12)))):
            raise ValueError('film capture conservation ledger')

    def checkpoint(self):
        payload = dict(schema=SCHEMA, config=digest(self.config), adapter=self.adapter, engine=self.engine,
                       state=self.state.copy(), tick=self.tick)
        return dict(payload=payload, sha256=digest(payload))

    def restore(self, packet):
        payload = packet['payload']
        tick = payload['tick']
        if type(tick) is int and len(payload['state'])==self.film_end+12 and payload['state'][-5]>tick*self.dt:
            raise ValueError('film capture event after checkpoint')
        StefanInterface.restore(self,packet)

    def report(self):
        state = self.state
        view = copy.copy(self.film)
        view.state,view.tick = state[:self.film_end],self.tick
        report = view.report()
        film = report['wall_film']
        _,water,px,energy = self.totals(state)
        air_error,water_error,px_error,energy_error = self.residuals(state)
        drop_mass,_,_,x,evap,cond = state[self.film_end:self.film_end+6]
        time,mass,impact_energy,impulse,evap_at_impact = state[-5:]
        report['totals_mol'][0] += drop_mass/MOLAR_MASS
        report['extent_mol'][0] += evap/MOLAR_MASS
        report['extent_mol'][1] += cond/MOLAR_MASS
        report['species_budget_residual_mol'] = [0.,water_error/MOLAR_MASS,air_error/MOLAR_MASS]
        report['thermal']['internal_ledger_residual_j'] = energy_error
        film.update(water_residual_kg=water_error,energy_residual_j=energy_error,
                    dry_air_residual_kg=air_error,phase_residual_kg=film['phase_residual_kg']-mass)
        report['film_capture'] = dict(airborne_water_kg=drop_mass, captured=time>0.,
            capture_time_s=time if time else None, position_m=x%self.film.gas.length,
            captured_water_kg=mass, captured_energy_j=impact_energy, wall_impulse_kg_m_s=impulse,
            airborne_evaporated_kg=evap, airborne_condensed_kg=cond,
            film_evaporated_after_impact_kg=film['evaporated_kg']-evap_at_impact if time else 0.,
            total_water_kg=water, total_energy_j=energy, total_tangential_momentum_kg_m_s=px,
            water_residual_kg=water_error, energy_residual_j=energy_error,
            tangential_momentum_residual_kg_m_s=px_error)
        return report
