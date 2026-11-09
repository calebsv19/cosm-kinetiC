"""Depth-averaged normal momentum for one droplet above a prewetted strip."""
import hashlib
import math
from pathlib import Path

from chemistry_film_capture import FilmCapture, SCHEMA as FLIGHT_SCHEMA, example as capture_example
from chemistry_reactor import digest
from chemistry_stefan_interface import StefanInterface

SCHEMA = 'rc7-normal-film-capture/v1'


def example():
    return dict(capture_example(), schema=SCHEMA, gas_normal_velocity_m_s=0.,
                wall_relaxation_s=2e-4)


class NormalFilmCapture(FilmCapture):
    def __init__(self, library, config):
        if set(config) != set(example()) or config['schema'] != SCHEMA:
            raise ValueError('normal film capture configuration')
        base = dict(config, schema=FLIGHT_SCHEMA)
        speed = base.pop('gas_normal_velocity_m_s')
        self.wall_relaxation = base.pop('wall_relaxation_s')
        flight = FilmCapture(library, base)
        # Start from the established film/parcel setup and keep its event layout.
        self.__dict__.update(flight.__dict__)
        self.config = dict(flight.config, schema=SCHEMA, gas_normal_velocity_m_s=speed,
                           wall_relaxation_s=self.wall_relaxation)
        self.adapter = digest([flight.adapter, hashlib.sha256(Path(__file__).read_bytes()).hexdigest()])
        if (type(speed) not in (int, float) or not abs(speed) <= .1 or
                type(self.wall_relaxation) not in (int, float) or
                not 4*self.dt <= self.wall_relaxation <= 1.):
            raise ValueError('normal film gas velocity or wall relaxation bounds')
        self.normal_specific_ke = 0.  # Normal kinetic energy is now state-dependent.
        self.normal_start = self.film_end + 6
        self.drop_normal = self.normal_start + self.film.cells
        normal = []
        for i in range(0, self.gas_end, 4):
            mass = self.state[i] + self.state[i+1]
            normal.append(mass*speed)
            self.state[i+3] += .5*mass*speed**2
        # Gas normal momenta, droplet momentum, impact and traction impulses,
        # and gas impulses from parcel drag and phase exchange, respectively.
        self.state[-6:-6] = [*normal, -self.initial_drop_mass*self.speed, 0., 0., 0., 0.]
        self.initial_gas_normal = math.fsum(normal)
        self.initial_normal = self.normal_total(self.state)
        self.baseline = self.totals(self.state)
        self.validate(self.state)

    def gas_kinematics(self, state):
        normal = state[self.normal_start:self.drop_normal]
        mass = [state[i]+state[i+1] for i in range(0, self.gas_end, 4)]
        if min(mass) <= 0.:
            raise ValueError('normal film gas inventory')
        velocity = [p/m for p,m in zip(normal,mass)]
        if max(map(abs,velocity)) > .2:
            raise ValueError('normal film gas velocity bound')
        kinetic = [.5*p*w for p,w in zip(normal,velocity)]
        reduced = state[:self.film_end]
        for i,energy in enumerate(kinetic):
            reduced[4*i+3] -= energy
        return reduced, normal, velocity, kinetic

    def derivative(self, state):
        reduced, normal, velocity, kinetic = self.gas_kinematics(state)
        rate = self.film.derivative(reduced)
        gas = self.film.gas.gas_fields(reduced)
        normal_flux, energy_flux = [], []
        for i,(_,u,_,_,_,sound) in enumerate(gas):
            j = (i+1) % self.film.cells
            other_u, other_sound = gas[j][1], gas[j][5]
            wave = max(abs(u)+sound,abs(other_u)+other_sound)
            scale = .5/self.film.dx
            normal_flux.append(scale*(u*normal[i]+other_u*normal[j]-wave*(normal[j]-normal[i])))
            energy_flux.append(scale*(u*kinetic[i]+other_u*kinetic[j]-wave*(kinetic[j]-kinetic[i])))
        traction = [p/self.wall_relaxation for p in normal]
        normal_rate = [normal_flux[i-1]-flux-traction[i] for i,flux in enumerate(normal_flux)]
        for i,flux in enumerate(energy_flux):
            rate[4*i+3] += energy_flux[i-1]-flux
        # Stationary wall traction thermalizes the lost kinetic energy locally.
        parcel_rate = [0.]*6
        w_drop = dp = drag = phase = 0.
        if state[-5] == 0.:
            parcel_state = [*reduced[:self.gas_end], *state[self.film_end:self.normal_start]]
            coupled = self.flight.derivative(parcel_state)
            carrier = self.film.gas.derivative(reduced[:self.gas_end])
            for i,(new,old) in enumerate(zip(coupled,carrier)):
                rate[i] += new-old
            parcel_rate = coupled[self.gas_end:]
            mass,u_drop,_,x,radius,_ = self.flight.particle(parcel_state,0)
            stencil = self.flight.stencil(x)
            w_drop = state[self.drop_normal]/mass
            w_gas = math.fsum(w*velocity[i] for i,w in stencil)
            u_gas = math.fsum(w*gas[i][1] for i,w in stencil)
            density = math.fsum(w*gas[i][0] for i,w in stencil)
            slip = math.hypot(u_gas-u_drop,w_gas-w_drop)
            viscosity = self.flight.viscosity
            properties = self.flight.exchange.config
            if (abs(w_drop) > .2 or max(2*radius*density*slip/viscosity,
                    2*radius*slip/properties['diffusivity_m2_s'],
                    2*radius*density*1010.*slip/properties['conductivity_w_m_k']) > .1):
                raise ValueError('normal film vector drag or transport bound')
            force = 6*math.pi*viscosity*radius*(w_gas-w_drop)
            drag = -force
            phase = -w_drop*parcel_rate[0]
            dp = -drag-phase
            power = w_drop*force + .5*w_drop**2*parcel_rate[0]
            for i,w in stencil:
                normal_rate[i] -= w*dp
                rate[4*i+3] -= w*power
        return [*rate, *parcel_rate, *normal_rate, dp, 0., math.fsum(traction), drag, phase,
                w_drop, 0., 0., 0., 0., 0.]

    def capture(self, state, time):
        state = state.copy()
        momentum = state[self.drop_normal]
        state[self.film_end+2] += .5*momentum**2/state[self.film_end]
        state[self.drop_normal] = 0.
        state[self.drop_normal+1] = momentum
        return super().capture(state,time)

    def normal_total(self, state):
        return math.fsum(state[self.normal_start:self.drop_normal+3])

    def totals(self, state):
        dry,water,px,energy = super().totals(state)
        mass = state[self.film_end]
        if mass > 0.:
            energy += .5*state[self.drop_normal]**2/mass
        return dry,water,px,energy

    def normal_errors(self, state):
        gas = math.fsum(state[self.normal_start:self.drop_normal])
        _,_,traction,drag,phase = state[self.drop_normal:self.drop_normal+5]
        return self.normal_total(state)-self.initial_normal, math.fsum(
            (gas,traction,-self.initial_gas_normal,-drag,-phase))

    def validate(self, state):
        if len(state)!=self.drop_normal+11 or not all(math.isfinite(x) for x in state):
            raise ValueError('normal film capture state shape or value')
        self.derivative(state)
        # Keep the inherited workspace readout admissible before its temperature
        # is replaced with the gas temperature with normal kinetic energy removed.
        self.film.gas.gas_fields(state)
        time,captured,_,_,evap_at_impact = state[-5:]
        mass,px,energy,_,evap,cond = state[self.film_end:self.normal_start]
        pz,impact = state[self.drop_normal:self.drop_normal+2]
        if (min(time,captured,evap,cond,evap_at_impact)<0. or
                (time==0. and (any(state[-4:]) or impact or self.clearance(state)<=0.)) or
                (time>0. and (captured<=0. or any((mass,px,pz,energy)) or
                              evap_at_impact>state[self.film_end-3]))):
            raise ValueError('normal film capture event state')
        parcel_error = math.fsum((mass,captured,evap,-cond,-self.initial_drop_mass))
        film_error = self.film.phase_residual(state[:self.film_end])-captured
        normal_scale = max(abs(self.initial_normal),abs(self.initial_gas_normal),1e-16)
        if (abs(parcel_error)>2e-10*self.initial_drop_mass or
                abs(film_error)>2e-10*self.film.initial_film_mass or
                max(map(abs,self.normal_errors(state)))>2e-10*normal_scale or
                any(abs(r)>2e-10*max(abs(b),f) for r,b,f in zip(
                    self.residuals(state),self.baseline,(1e-18,1e-18,1e-16,1e-12)))):
            raise ValueError('normal film capture conservation ledger')

    def checkpoint(self):
        payload = super().checkpoint()['payload']
        payload['schema'] = SCHEMA
        return dict(payload=payload,sha256=digest(payload))

    def restore(self, packet):
        payload = packet['payload']
        tick = payload['tick']
        if type(tick) is int and len(payload['state'])==len(self.state) and payload['state'][-5]>tick*self.dt:
            raise ValueError('normal film capture event after checkpoint')
        StefanInterface.restore(self,packet)

    def report(self):
        report = super().report()
        reduced,normal,velocity,_ = self.gas_kinematics(self.state)
        report['thermal']['temperature_k'] = [g[2] for g in self.film.gas.gas_fields(reduced)]
        pz,impact,traction,drag,phase = self.state[self.drop_normal:self.drop_normal+5]
        mass = self.state[self.film_end]
        balance,exchange = self.normal_errors(self.state)
        report['normal_momentum'] = dict(gas_velocity_m_s=velocity, gas_momentum_kg_m_s=math.fsum(normal),
            drop_velocity_m_s=pz/mass if mass else None, drop_momentum_kg_m_s=pz,
            impact_impulse_kg_m_s=impact, wall_traction_impulse_kg_m_s=traction,
            drag_impulse_into_gas_kg_m_s=drag, phase_impulse_into_gas_kg_m_s=phase,
            total_kg_m_s=self.normal_total(self.state), residual_kg_m_s=balance,
            gas_exchange_residual_kg_m_s=exchange)
        return report
