"""Hydraulic momentum and phase transport in two communicating water columns."""
import copy
import hashlib
import math
from pathlib import Path

from chemistry_reactor import digest
from chemistry_water_interface import MOLAR_MASS, WaterInterface, example as column_example

SCHEMA = 'rc7-water-momentum/v1'
GRAVITY = 9.81


def example(cells=8):
    left, right = column_example(cells), column_example(cells)
    left.update(liquid_kg=.126, dt_s=.001)
    right.update(liquid_kg=.114, dt_s=.001, vapor_density_kg_m3=[.032] * cells)
    return dict(schema=SCHEMA, n=[cells + 1, 2, 1], chemistry=left['chemistry'],
                columns=[left, right], inertial_length_m=.12,
                damping_per_s=1.5, initial_flow_kg_s=0.)


class WaterMomentum(WaterInterface):
    def __init__(self, library, config):
        if set(config) != set(example()) or config['schema'] != SCHEMA or len(config['columns']) != 2:
            raise ValueError('water momentum configuration')
        self.config = copy.deepcopy(config)
        self.columns = tuple(WaterInterface(library, column) for column in config['columns'])
        left, right = self.columns
        if (left.cells != right.cells or left.area != right.area or left.height != right.height or
                left.config['dt_s'] != right.config['dt_s'] or any(c.config['bath'] for c in self.columns) or
                config['n'] != [left.cells + 1, 2, 1] or config['chemistry'] != left.config['chemistry']):
            raise ValueError('water momentum requires matching insulated columns')
        self.dt = left.config['dt_s']
        self.mass_per_m = left.area * left.density_liquid
        length, self.damping, flow = (config[key] for key in (
            'inertial_length_m', 'damping_per_s', 'initial_flow_kg_s'))
        for value, low, high in ((length, .01, 10.), (self.damping, 0., 50.),
                                 (flow, -.5*self.mass_per_m, .5*self.mass_per_m)):
            if type(value) not in (int, float) or not math.isfinite(value) or not low <= value <= high:
                raise ValueError('hydraulic parameter bounds')
        self.inertance = length / self.mass_per_m
        if self.dt * max(math.sqrt(2*GRAVITY/length), self.damping) > .2:
            raise ValueError('hydraulic timestep bound')
        self.column_size = len(left.state)
        self.state = [*left.state, *right.state, flow, 0., 0.]
        self.engine = left.engine
        self.adapter = digest([left.adapter, hashlib.sha256(Path(__file__).read_bytes()).hexdigest()])
        self.tick = 0
        self.initial_energy = math.fsum(self.energies(self.state))
        self.step(self.state)

    def split(self, state):
        return state[:self.column_size], state[self.column_size:-3]

    def fields(self, state):
        if (len(state) != 2*self.column_size + 3 or not all(math.isfinite(x) for x in state) or
                state[-1] < 0. or abs(state[-3]) > .5*self.mass_per_m):
            raise ValueError('invalid hydraulic state')
        return [column.fields(local) for column, local in zip(self.columns, self.split(state))]

    def derivative(self, state):
        fields = self.fields(state)
        local = self.split(state)
        flow = state[-3]
        donor = 0 if flow >= 0. else 1
        if self.dt * abs(flow) > .2 * local[donor][0]:
            raise ValueError('hydraulic donor timestep bound')
        liquid_energy_flux = flow * (local[donor][1] / local[donor][0])
        drag_heat = self.inertance * self.damping * flow**2
        rates = []
        for index, (column, part, field) in enumerate(zip(self.columns, local, fields)):
            rate = column.derivative(part)
            phase_mass_rate = rate[0]
            direction = -1 if index == 0 else 1
            rate[0] += direction * flow
            rate[1] += direction * liquid_energy_flux + .5 * drag_heat
            # Include the phase exchange of liquid gravitational energy in the gas energy.
            rate[2 + column.cells] -= GRAVITY * field[0] * phase_mass_rate
            rates.extend(rate)
        pressure_head = GRAVITY * (fields[0][0] - fields[1][0])
        return [*rates, pressure_head / self.inertance - self.damping*flow, flow, drag_heat]

    def step(self, state):
        # A common implicit midpoint makes the quadratic mechanical ledger
        # satisfy the same discrete work balance as the thermal update.
        half_dt = .5 * self.dt
        midpoint = state
        for _ in range(32):
            rates = self.derivative(midpoint)
            updated = [x + half_dt * rate for x, rate in zip(state, rates)]
            if all(abs(a-b) <= 8 * math.ulp(max(1., abs(a), abs(b)))
                   for a,b in zip(midpoint, updated)):
                candidate = [x + self.dt * rate for x, rate in zip(state, rates)]
                self.fields(candidate)
                self.check_balances(candidate)
                return candidate
            midpoint = updated
        raise ValueError('hydraulic midpoint did not converge')

    def energies(self, state):
        local = self.split(state)
        internal = math.fsum(
            value for part, column in zip(local, self.columns)
            for value in [part[1], *part[2 + column.cells:-3]])
        gravity = .5 * GRAVITY / self.mass_per_m * math.fsum(part[0]**2 for part in local)
        kinetic = .5 * self.inertance * state[-3]**2
        return internal, gravity, kinetic

    def mass_residuals(self, state):
        residuals = []
        for index, (column, part) in enumerate(zip(self.columns, self.split(state))):
            gain = state[-2] if index == 1 else -state[-2]
            vapor = math.fsum(part[2:2 + column.cells])
            total = math.fsum([part[0], vapor, -column.initial_mass, -gain])
            phase = math.fsum([part[0], -column.config['liquid_kg'], -gain, part[-3], -part[-2]])
            residuals.append((total, phase))
        return residuals

    def check_balances(self, state):
        self.derivative(state)
        residuals = self.mass_residuals(state)
        energy = math.fsum([*self.energies(state), -self.initial_energy])
        for column, part, residual in zip(self.columns, self.split(state), residuals):
            if max(map(abs, residual)) > 1e-10 * column.initial_mass or part[-1] != 0.:
                raise ValueError('hydraulic water or phase ledger')
        if abs(energy) > 2e-11 * max(1., abs(self.initial_energy)):
            raise ValueError('hydraulic total energy ledger')

    def checkpoint(self):
        payload = dict(schema=SCHEMA, config=digest(self.config), engine=self.engine,
                       adapter=self.adapter, state=self.state.copy(), tick=self.tick)
        return dict(payload=payload, sha256=digest(payload))

    def report(self):
        state = self.state
        reports = []
        concentration, temperature = [], []
        for column, part in zip(self.columns, self.split(state)):
            height, width, liquid_temperature, gas_temperature, density, saturation = column.fields(part)
            vapor = math.fsum(part[2:2 + column.cells])
            concentration.extend([[column.density_liquid / MOLAR_MASS, 0.],
                                  *[[0., rho / MOLAR_MASS] for rho in density]])
            temperature.extend([liquid_temperature, *gas_temperature])
            reports.append(dict(position_m=height,
                edges_m=[0., *(height + i*width for i in range(column.cells)), column.height],
                surface_velocity_m_s=(column.derivative(part)[0] +
                    (-state[-3] if not reports else state[-3])) / self.mass_per_m,
                liquid_kg=part[0], vapor_kg=vapor, vapor_density_kg_m3=density,
                liquid_temperature_k=liquid_temperature, saturation_pressure_pa=saturation.pressure_pa,
                evaporated_kg=part[-3], condensed_kg=part[-2]))
        residuals = self.mass_residuals(state)
        internal, gravity, kinetic = self.energies(state)
        energy_residual = math.fsum([internal, gravity, kinetic, -self.initial_energy])
        total_mass_residual = math.fsum(pair[0] for pair in residuals)
        return dict(tick=self.tick, time_s=self.tick*self.dt,
                    totals_mol=[math.fsum(r[key] for r in reports)/MOLAR_MASS
                                for key in ('liquid_kg', 'vapor_kg')],
                    concentration_mol_m3=concentration,
                    extent_mol=[math.fsum(r[key] for r in reports)/MOLAR_MASS
                                for key in ('evaporated_kg', 'condensed_kg')],
                    species_budget_residual_mol=[total_mass_residual/MOLAR_MASS, 0.],
                    thermal=dict(temperature_k=temperature,
                                 property_id='IAPWS saturation and linear caloric model'),
                    hydraulic=dict(columns=reports, flow_kg_s=state[-3], transfer_kg=state[-2],
                        drag_heat_j=state[-1], internal_energy_j=internal,
                        gravitational_energy_j=gravity, kinetic_energy_j=kinetic,
                        column_water_residual_kg=[pair[0] for pair in residuals],
                        phase_residual_kg=[pair[1] for pair in residuals],
                        total_water_residual_kg=total_mass_residual, total_energy_residual_j=energy_residual))
