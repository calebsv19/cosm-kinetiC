"""Conservative planar water interface with a moving gas finite-volume mesh."""
import argparse
import copy
import ctypes as C
import hashlib
import json
import math
from pathlib import Path

from chemistry_reactor import digest
from chemistry_water import MOLAR_MASS, Properties

SCHEMA = 'rc7-water-interface/v1'
REFERENCE_K = 298.15
LIQUID_HEAT_CAPACITY = 4180.0
VAPOR_HEAT_CAPACITY = 1420.0
SPECIES = {'species': [{'id': 'H2O_liquid'}, {'id': 'H2O_vapor'}]}


def example(cells=16):
    return dict(schema=SCHEMA, n=[cells + 1, 1, 1], chemistry=copy.deepcopy(SPECIES),
                area_m2=.01, height_m=.03, liquid_kg=.002,
                liquid_temperature_k=298.15, bath=False,
                gas_temperature_k=[298.15] * cells, vapor_density_kg_m3=[.008] * cells,
                carrier_heat_capacity_j_k=.36, diffusivity_m2_s=2.5e-5,
                conductivity_w_m_k=.026, mass_transfer_velocity_m_s=.03, dt_s=.005)


class WaterInterface:
    def __init__(self, library, config):
        template = example()
        if (set(config) != set(template) or config['schema'] != SCHEMA or
                config['chemistry'] != SPECIES or type(config['bath']) is not bool):
            raise ValueError('water interface configuration')
        grid = config['n']
        if (len(grid) != 3 or grid[1:] != [1, 1] or
                type(grid[0]) is not int or not 2 <= grid[0] <= 257):
            raise ValueError('water interface grid')
        self.cells = grid[0] - 1
        self.config = copy.deepcopy(config)
        for key in ('area_m2', 'height_m', 'liquid_kg', 'carrier_heat_capacity_j_k',
                    'diffusivity_m2_s', 'conductivity_w_m_k', 'mass_transfer_velocity_m_s', 'dt_s',
                    'liquid_temperature_k'):
            number = config[key]
            if type(number) not in (int, float) or not math.isfinite(number):
                raise ValueError(f'invalid interface field {key}')
        if (any(not 1e-8 <= config[key] <= 100 for key in ('area_m2', 'height_m')) or
                not 1e-9 <= config['carrier_heat_capacity_j_k'] <= 1e9 or
                not 0 < config['dt_s'] <= 100 or
                any(not 0 <= config[key] <= 10 for key in (
                    'diffusivity_m2_s', 'conductivity_w_m_k', 'mass_transfer_velocity_m_s'))):
            raise ValueError('water interface parameter bounds')

        library = Path(library).resolve()
        self.engine = hashlib.sha256(library.read_bytes()).hexdigest()
        self.adapter = digest({name: hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
                               for name in ('chemistry_water_interface.py', 'chemistry_water.py',
                                            'chemistry_reactor.py')})
        self.lib = C.CDLL(str(library))
        self.get_properties = getattr(self.lib, 'chem_water_properties', None)
        if self.get_properties is None:
            raise ValueError('selected engine lacks RC7 water properties')
        self.get_properties.argtypes = [C.c_double, C.POINTER(Properties)]
        self.get_properties.restype = C.c_bool
        reference = self.properties(REFERENCE_K)
        self.density_liquid = reference.liquid_density
        self.latent = reference.vapor_u - reference.liquid_u
        self.area = config['area_m2']
        self.height = config['height_m']
        self.carrier_capacity = config['carrier_heat_capacity_j_k'] / self.cells
        self.bath_temperature = config['liquid_temperature_k']
        liquid = config['liquid_kg']
        _, width = self.geometry(liquid)
        temperatures = config['gas_temperature_k']
        concentrations = config['vapor_density_kg_m3']
        for field in (temperatures, concentrations):
            if len(field) != self.cells or any(type(x) not in (int, float) or not math.isfinite(x) for x in field):
                raise ValueError('invalid gas initial field')
        vapor = [rho * self.area * width for rho in concentrations]
        gas_energy = [self.latent * mass + self.gas_capacity(mass) * (temperature - REFERENCE_K)
                      for mass, temperature in zip(vapor, temperatures)]
        liquid_energy = self.liquid_energy(liquid, self.bath_temperature)
        self.state = [liquid, liquid_energy, *vapor, *gas_energy, 0., 0., 0.]
        self.initial_mass = math.fsum([liquid, *vapor])
        self.initial_energy = math.fsum([liquid_energy, *gas_energy])
        self.tick = 0
        self.fields(self.state)
        self.step(self.state)

    def properties(self, temperature):
        result = Properties()
        if not self.get_properties(temperature, result):
            raise ValueError('water interface temperature outside 283.15–323.15 K')
        return result

    def geometry(self, liquid):
        interface = liquid / (self.density_liquid * self.area)
        if not 0 < interface <= .95 * self.height:
            raise ValueError('interface requires positive liquid and at least 5% gas headspace')
        return interface, (self.height - interface) / self.cells

    def gas_capacity(self, mass):
        return self.carrier_capacity + VAPOR_HEAT_CAPACITY * mass

    def liquid_energy(self, mass, temperature):
        return LIQUID_HEAT_CAPACITY * mass * (temperature - REFERENCE_K)

    def fields(self, state):
        if len(state) != 5 + 2 * self.cells or not all(math.isfinite(x) for x in state):
            raise ValueError('invalid interface state')
        liquid, energy = state[:2]
        interface, width = self.geometry(liquid)
        vapor = state[2:2 + self.cells]
        gas_energy = state[2 + self.cells:2 + 2 * self.cells]
        if any(mass < 0 for mass in vapor) or min(state[-3:-1]) < 0:
            raise ValueError('negative interface inventory')
        liquid_temperature = REFERENCE_K + energy / (LIQUID_HEAT_CAPACITY * liquid)
        gas_temperature = [REFERENCE_K + (value - self.latent * mass) / self.gas_capacity(mass)
                           for value, mass in zip(gas_energy, vapor)]
        density = [mass / (self.area * width) for mass in vapor]
        if any(rho > 2 * self.properties(temperature).vapor_density
               for rho, temperature in zip(density, gas_temperature)):
            raise ValueError('gas vapor density exceeds admitted saturation ratio')
        saturation = self.properties(liquid_temperature)
        return interface, width, liquid_temperature, gas_temperature, density, saturation

    def derivative(self, state):
        _, width, liquid_temperature, gas_temperature, density, saturation = self.fields(state)
        config = self.config
        diffusion = config['diffusivity_m2_s']
        speed = config['mass_transfer_velocity_m_s']
        mass_conductance = self.area * diffusion / width
        heat_conductance = self.area * config['conductivity_w_m_k'] / width
        # The half-cell diffusion resistance is in series with the interface law.
        interface_conductance = (self.area / (1 / speed + width / (2 * diffusion))
                                 if speed and diffusion else 0.)
        dt = config['dt_s']
        if (dt * (2 * mass_conductance + interface_conductance) > .8 * self.area * width or
                dt * 3 * heat_conductance > .8 * self.carrier_capacity or
                dt * 2 * heat_conductance > .8 * LIQUID_HEAT_CAPACITY * state[0]):
            raise ValueError('water interface timestep exceeds explicit transport bound')

        transfer = interface_conductance * (saturation.vapor_density - density[0])
        if dt * transfer >= state[0]:
            raise ValueError('water interface step would exhaust the liquid')
        donor_temperature = liquid_temperature if transfer >= 0 else gas_temperature[0]
        vapor_u = self.latent + VAPOR_HEAT_CAPACITY * (donor_temperature - REFERENCE_K)
        energy_transfer = transfer * vapor_u + 2 * heat_conductance * (
            liquid_temperature - gas_temperature[0])

        mass_flux = [transfer]
        energy_flux = [energy_transfer]
        for cell in range(self.cells - 1):
            flux = mass_conductance * (density[cell] - density[cell + 1])
            donor = gas_temperature[cell if flux >= 0 else cell + 1]
            mass_flux.append(flux)
            energy_flux.append(flux * (self.latent + VAPOR_HEAT_CAPACITY * (donor - REFERENCE_K)) +
                               heat_conductance * (gas_temperature[cell] - gas_temperature[cell + 1]))
        mass_flux.append(0.)
        energy_flux.append(0.)
        liquid_energy_change = -energy_transfer
        bath_heat = 0.
        if config['bath']:
            liquid_energy_change = self.liquid_energy(-transfer, self.bath_temperature)
            bath_heat = liquid_energy_change + energy_transfer
        return [-transfer, liquid_energy_change,
                *(left - right for left, right in zip(mass_flux, mass_flux[1:])),
                *(left - right for left, right in zip(energy_flux, energy_flux[1:])),
                max(0., transfer), max(0., -transfer), bath_heat]

    def step(self, state):
        dt = self.config['dt_s']
        first = self.derivative(state)
        stage = [value + dt * rate for value, rate in zip(state, first)]
        second = self.derivative(stage)
        candidate = [value + .5 * dt * (a + b) for value, a, b in zip(state, first, second)]
        self.fields(candidate)
        self.check_balances(candidate)
        return candidate

    def check_balances(self, state):
        vapor = math.fsum(state[2:2 + self.cells])
        energy = math.fsum([state[1], *state[2 + self.cells:2 + 2 * self.cells]])
        liquid_change = self.config['liquid_kg'] - state[0]
        if (abs(state[0] + vapor - self.initial_mass) > 1e-10 * self.initial_mass or
                abs(liquid_change - (state[-3] - state[-2])) > 1e-10 * self.initial_mass or
                abs(energy - self.initial_energy - state[-1]) >
                1e-9 * max(1., abs(self.initial_energy), abs(energy))):
            raise ValueError('water interface conservation ledger')
        temperature = REFERENCE_K + state[1] / (LIQUID_HEAT_CAPACITY * state[0])
        if ((not self.config['bath'] and state[-1] != 0.) or
                (self.config['bath'] and abs(temperature - self.bath_temperature) > 1e-9)):
            raise ValueError('water interface thermal boundary ledger')

    def advance(self, steps):
        if type(steps) is not int or not 0 <= steps <= 10000:
            raise ValueError('water interface step count')
        state = self.state
        for _ in range(steps):
            state = self.step(state)
        self.state = state
        self.tick += steps

    def checkpoint(self):
        payload = dict(schema=SCHEMA, config=digest(self.config), engine=self.engine,
                       adapter=self.adapter, state=self.state.copy(), tick=self.tick)
        return dict(payload=payload, sha256=digest(payload))

    def restore(self, packet):
        payload = packet['payload']
        expected = self.checkpoint()['payload']
        if (set(payload) != set(expected) or digest(payload) != packet['sha256'] or
                any(payload[key] != expected[key] for key in ('schema', 'engine', 'adapter', 'config')) or
                type(payload['tick']) is not int or not 0 <= payload['tick'] <= 2**53):
            raise ValueError('water interface checkpoint identity')
        self.fields(payload['state'])
        self.check_balances(payload['state'])
        self.state = payload['state'].copy()
        self.tick = payload['tick']

    def report(self):
        state = self.state
        position, width, liquid_temperature, gas_temperature, density, saturation = self.fields(state)
        vapor = math.fsum(state[2:2 + self.cells])
        energy = math.fsum([state[1], *state[2 + self.cells:2 + 2 * self.cells]])
        mass_residual = state[0] + vapor - self.initial_mass
        edges = [0., *(position + width * i for i in range(self.cells)), self.height]
        return dict(tick=self.tick, time_s=self.tick * self.config['dt_s'],
                    totals_mol=[state[0] / MOLAR_MASS, vapor / MOLAR_MASS],
                    concentration_mol_m3=[[self.density_liquid / MOLAR_MASS, 0.],
                                          *[[0., value / MOLAR_MASS] for value in density]],
                    extent_mol=[state[-3] / MOLAR_MASS, state[-2] / MOLAR_MASS],
                    species_budget_residual_mol=[mass_residual / MOLAR_MASS, 0.],
                    thermal=dict(temperature_k=[liquid_temperature, *gas_temperature],
                                 internal_ledger_residual_j=energy - self.initial_energy - state[-1],
                                 property_id="IAPWS saturation and linear caloric model"),
                    water_interface=dict(position_m=position, edges_m=edges,
                        displacement_m=position - self.config['liquid_kg'] / (self.density_liquid * self.area),
                        liquid_temperature_k=liquid_temperature, gas_temperature_k=gas_temperature,
                        vapor_density_kg_m3=density, saturation_pressure_pa=saturation.pressure_pa,
                        liquid_kg=state[0], vapor_kg=vapor, mass_residual_kg=mass_residual,
                        internal_energy_j=energy, bath_heat_into_chamber_j=state[-1],
                        energy_residual_j=energy - self.initial_energy - state[-1]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library', required=True)
    parser.add_argument('--config', type=Path, required=True)
    parser.add_argument('--steps', type=int, default=1)
    parser.add_argument('--resume', type=Path)
    parser.add_argument('--checkpoint', type=Path)
    args = parser.parse_args()
    model = WaterInterface(args.library, json.loads(args.config.read_text()))
    if args.resume:
        model.restore(json.loads(args.resume.read_text()))
    model.advance(args.steps)
    print(json.dumps(model.report(), allow_nan=False))
    if args.checkpoint:
        with args.checkpoint.open('x') as output:
            json.dump(model.checkpoint(), output, allow_nan=False)


if __name__ == '__main__':
    main()
