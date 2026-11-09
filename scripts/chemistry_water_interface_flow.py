"""Periodic channel with a planar water interface and quasi-steady Stokes flow."""
import copy
import hashlib
import math
from pathlib import Path

from chemistry_reactor import digest
from chemistry_water_interface import (
    LIQUID_HEAT_CAPACITY, MOLAR_MASS, REFERENCE_K, VAPOR_HEAT_CAPACITY,
    SCHEMA as COLUMN_SCHEMA, WaterInterface, example as column_example,
)

SCHEMA = 'rc7-water-interface-flow/v1'


def example(axial=12, layers=6):
    config = column_example(layers)
    config.update(schema=SCHEMA, n=[layers + 1, axial, 1],
                  height_m=.0032, length_m=.024, dt_s=.00015,
                  carrier_heat_capacity_j_k=.036, pressure_gradient_pa_m=.5,
                  dynamic_viscosity_pa_s=1.8e-5, carrier_density_kg_m3=1.2,
                  gas_temperature_k=[REFERENCE_K] * (axial * layers),
                  vapor_density_kg_m3=[.008] * (axial * layers))
    return config


def channel_flow(layers, width, viscosity, drive):
    """Cell-centered finite-volume Stokes solve, zero velocity at both walls."""
    rhs = drive * width**2 / viscosity
    upper = []
    solution = []
    for i in range(layers):
        diagonal = (3. if i in (0, layers - 1) else 2.)
        if i:
            diagonal += upper[-1]
        solution.append((rhs + (solution[-1] if i else 0.)) / diagonal)
        upper.append(-1. / diagonal)
    for i in range(layers - 2, -1, -1):
        solution[i] -= upper[i] * solution[i + 1]
    return solution


class FlowingWaterInterface(WaterInterface):
    def __init__(self, library, config):
        if set(config) != set(example()) or config['schema'] != SCHEMA:
            raise ValueError('flowing water interface configuration')
        grid = config['n']
        if (len(grid) != 3 or grid[2] != 1 or type(grid[0]) is not int or
                type(grid[1]) is not int or not 3 <= grid[0] <= 65 or not 4 <= grid[1] <= 128):
            raise ValueError('flowing water interface grid')
        self.layers, self.axial = grid[0] - 1, grid[1]
        self.cells = self.axial * self.layers
        self.config = copy.deepcopy(config)
        base = copy.deepcopy(config)
        self.length = base.pop('length_m')
        self.drive = base.pop('pressure_gradient_pa_m')
        self.viscosity = base.pop('dynamic_viscosity_pa_s')
        self.rho = base.pop('carrier_density_kg_m3')
        for value, low, high in ((self.length, 1e-4, 10.), (self.drive, -1e4, 1e4),
                                 (self.viscosity, 1e-7, 1.), (self.rho, .01, 100.)):
            if type(value) not in (int, float) or not math.isfinite(value) or not low <= value <= high:
                raise ValueError('channel flow parameter bounds')
        self.dx = self.length / self.axial
        self.area = config['area_m2']
        self.height = config['height_m']
        self.bath_temperature = config['liquid_temperature_k']
        self.carrier_capacity = config['carrier_heat_capacity_j_k'] / self.cells

        for key in ('gas_temperature_k', 'vapor_density_kg_m3'):
            values = config[key]
            if len(values) != self.cells or any(
                    type(x) not in (int, float) or not math.isfinite(x) for x in values):
                raise ValueError('invalid channel initial field')
            # The column object owns vertical fluxes and property admission.
            base[key] = values[:self.layers]
        base.update(schema=COLUMN_SCHEMA, n=[self.layers + 1, 1, 1],
                    area_m2=self.area / self.axial, liquid_kg=config['liquid_kg'] / self.axial,
                    carrier_heat_capacity_j_k=config['carrier_heat_capacity_j_k'] / self.axial)
        self.column = WaterInterface(library, base)
        self.density_liquid = self.column.density_liquid
        self.latent = self.column.latent
        self.engine = self.column.engine
        self.adapter = digest([self.column.adapter, hashlib.sha256(Path(__file__).read_bytes()).hexdigest()])
        liquid = config['liquid_kg']
        _, width = self.geometry(liquid)
        volume = self.column.area * width
        vapor = [volume * rho for rho in config['vapor_density_kg_m3']]
        energy = [self.latent * m + self.gas_capacity(m) * (t - REFERENCE_K)
                  for m, t in zip(vapor, config['gas_temperature_k'])]
        liquid_energy = self.liquid_energy(liquid, self.bath_temperature)
        self.state = [liquid, liquid_energy, *vapor, *energy, 0., 0., 0., 0.]
        self.initial_mass = math.fsum([liquid, *vapor])
        self.initial_energy = math.fsum([liquid_energy, *energy])
        self.tick = 0
        self.step(self.state)

    def geometry(self, liquid):
        return self.column.geometry(liquid / self.axial)

    def fields(self, state):
        if (len(state) != 6 + 2 * self.cells or not all(math.isfinite(x) for x in state) or
                min(state[-4:-1]) < 0):
            raise ValueError('invalid channel state')
        position, width = self.geometry(state[0])
        temperatures, densities, columns = [], [], []
        energy_start = 2 + self.cells
        for row in range(self.axial):
            start = row * self.layers
            end = start + self.layers
            column = [state[0] / self.axial, state[1] / self.axial,
                      *state[2 + start:2 + end], *state[energy_start + start:energy_start + end],
                      0., 0., 0.]
            _, _, liquid_temperature, gas_temperature, density, saturation = self.column.fields(column)
            columns.append(column)
            temperatures.extend(gas_temperature)
            densities.extend(density)
        return position, width, liquid_temperature, temperatures, densities, saturation, columns

    def flow(self, width):
        velocity = channel_flow(self.layers, width, self.viscosity, self.drive)
        reynolds = max(map(abs, velocity)) * width * self.layers * self.rho / self.viscosity
        if reynolds > 20.:
            raise ValueError('channel Reynolds number exceeds selected laminar bound')
        volume = self.column.area * width
        shear_factor = self.viscosity * self.area / width
        dissipation = [0.] * self.layers
        dissipation[0] = 2 * shear_factor * velocity[0]**2
        dissipation[-1] = 2 * shear_factor * velocity[-1]**2
        for layer in range(1, self.layers):
            half = .5 * shear_factor * (velocity[layer] - velocity[layer - 1])**2
            dissipation[layer - 1] += half
            dissipation[layer] += half
        power = self.drive * volume * self.axial * math.fsum(velocity)
        residual = power - math.fsum(dissipation)
        if abs(residual) > 1e-10 * power:
            raise ValueError('channel Stokes mechanical power balance')
        return velocity, reynolds, dissipation, power, residual

    def derivative(self, state):
        _, width, _, temperature, density, _, columns = self.fields(state)
        velocity, _, dissipation, power, _ = self.flow(width)
        dt = self.config['dt_s']
        volume = self.column.area * width
        # Vertical terms have their own 0.8 bound. Reserve 0.2 for axial terms.
        advective_rate = max(map(abs, velocity)) / self.dx
        diffusion = self.config['diffusivity_m2_s'] / self.dx**2
        heat = self.config['conductivity_w_m_k'] * volume / self.dx**2
        if dt * max(advective_rate + 2 * diffusion,
                    advective_rate + 2 * heat / self.carrier_capacity) > .2:
            raise ValueError('channel axial transport timestep bound')

        vertical = [self.column.derivative(column) for column in columns]
        liquid_rate = math.fsum(rate[0] for rate in vertical)
        front_speed = liquid_rate / (self.area * self.density_liquid)
        if abs(front_speed) * width * self.layers * self.rho / self.viscosity > .01:
            raise ValueError('interface motion exceeds quasi-steady Stokes bound')
        mass_rate = [x for rate in vertical for x in rate[2:2 + self.layers]]
        energy_rate = [x + dissipation[layer] / self.axial
                       for rate in vertical
                       for layer, x in enumerate(rate[2 + self.layers:-3])]
        energy_start = 2 + self.cells
        for row in range(self.axial):
            for layer, speed in enumerate(velocity):
                left = row * self.layers + layer
                right = ((row + 1) % self.axial) * self.layers + layer
                donor = left if speed >= 0 else right
                advected_mass = speed / self.dx * state[2 + donor]
                diffused_mass = volume * diffusion * (density[left] - density[right])
                diff_donor = left if diffused_mass >= 0 else right
                vapor_u = self.latent + VAPOR_HEAT_CAPACITY * (temperature[diff_donor] - REFERENCE_K)
                energy_flux = (speed / self.dx * state[energy_start + donor] +
                               diffused_mass * vapor_u + heat * (temperature[left] - temperature[right]))
                mass_flux = advected_mass + diffused_mass
                mass_rate[left] -= mass_flux
                mass_rate[right] += mass_flux
                energy_rate[left] -= energy_flux
                energy_rate[right] += energy_flux
        return [liquid_rate, math.fsum(rate[1] for rate in vertical), *mass_rate, *energy_rate,
                power, *(math.fsum(rate[index] for rate in vertical) for index in (-3, -2, -1))]

    def ledger(self, state):
        vapor = math.fsum(state[2:2 + self.cells])
        energy = math.fsum([state[1], *state[2 + self.cells:-4]])
        mass_residual = math.fsum([state[0], vapor, -self.initial_mass])
        energy_residual = math.fsum([energy, -self.initial_energy, -state[-4], -state[-1]])
        return vapor, energy, mass_residual, energy_residual

    def check_balances(self, state):
        self.flow(self.geometry(state[0])[1])
        _, energy, mass_residual, energy_residual = self.ledger(state)
        extent_residual = math.fsum([self.config['liquid_kg'], -state[0], -state[-3], state[-2]])
        if (max(abs(mass_residual), abs(extent_residual)) > 1e-10 * self.initial_mass or
                abs(energy_residual) > 1e-9 * max(1., abs(energy), abs(self.initial_energy))):
            raise ValueError('channel conservation ledger')
        temperature = REFERENCE_K + state[1] / (LIQUID_HEAT_CAPACITY * state[0])
        if ((not self.config['bath'] and state[-1] != 0.) or
                (self.config['bath'] and abs(temperature - self.bath_temperature) > 1e-9) or
                (self.drive == 0. and state[-4] != 0.)):
            raise ValueError('channel bath or drive ledger')

    def checkpoint(self):
        payload = dict(schema=SCHEMA, config=digest(self.config), engine=self.engine,
                       adapter=self.adapter, state=self.state.copy(), tick=self.tick)
        return dict(payload=payload, sha256=digest(payload))

    def report(self):
        state = self.state
        position, width, liquid_temperature, temperatures, density, saturation, _ = self.fields(state)
        velocity, reynolds, dissipation, power, residual = self.flow(width)
        vapor, energy, mass_residual, energy_residual = self.ledger(state)
        edges = [0., *(position + width * i for i in range(self.layers)), self.height]
        concentration = []
        thermal = []
        for row in range(self.axial):
            start = row * self.layers
            end = start + self.layers
            concentration.extend([[self.density_liquid / MOLAR_MASS, 0.],
                                  *[[0., value / MOLAR_MASS] for value in density[start:end]]])
            thermal.extend([liquid_temperature, *temperatures[start:end]])
        return dict(tick=self.tick, time_s=self.tick * self.config['dt_s'],
                    totals_mol=[state[0] / MOLAR_MASS, vapor / MOLAR_MASS],
                    concentration_mol_m3=concentration,
                    extent_mol=[state[-3] / MOLAR_MASS, state[-2] / MOLAR_MASS],
                    species_budget_residual_mol=[mass_residual / MOLAR_MASS, 0.],
                    thermal=dict(temperature_k=thermal, internal_ledger_residual_j=energy_residual,
                                 property_id='IAPWS saturation and linear caloric model'),
                    water_interface=dict(position_m=position, edges_m=edges,
                        displacement_m=position - self.geometry(self.config['liquid_kg'])[0],
                        liquid_temperature_k=liquid_temperature, gas_temperature_k=temperatures,
                        vapor_density_kg_m3=density, saturation_pressure_pa=saturation.pressure_pa,
                        liquid_kg=state[0], vapor_kg=vapor, mass_residual_kg=mass_residual,
                        internal_energy_j=energy, bath_heat_into_chamber_j=state[-1],
                        energy_residual_j=energy_residual),
                    channel_flow=dict(velocity_m_s=velocity, reynolds=reynolds,
                        drive_power_w=power, viscous_power_w=math.fsum(dissipation),
                        mechanical_power_residual_w=residual, drive_work_j=state[-4]))
