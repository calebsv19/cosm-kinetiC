"""Finite liquid/vapor water in one rigid, well-mixed chamber."""
import argparse
import copy
import ctypes as C
import hashlib
import json
import math
from pathlib import Path

from chemistry_reactor import digest

SCHEMA = 'rc7-water/v1'
MOLAR_MASS = .01801528


class Properties(C.Structure):
    _fields_ = [(key, C.c_double) for key in (
        'pressure_pa', 'liquid_density', 'vapor_density', 'liquid_u', 'vapor_u')]


class Model(C.Structure):
    _fields_ = [(key, C.c_double) for key in (
        'volume_m3', 'water_kg', 'thermal_capacity_j_k', 'relaxation_s')]
    _fields_.append(('insulated', C.c_bool))


class State(C.Structure):
    _fields_ = [(key, C.c_double) for key in ('temperature_k', 'vapor_kg', 'energy_j')]


def example():
    return dict(schema=SCHEMA, n=[1, 1, 1],
                chemistry={'species': [{'id': 'H2O_liquid'}, {'id': 'H2O_vapor'}]},
                volume_m3=.01, water_kg=.01, thermal_capacity_j_k=100.,
                relaxation_s=10., insulated=True, temperature_k=298.15,
                initial_vapor_kg=.00005, dt_s=.5)


class WaterChamber:
    def __init__(self, library, config):
        template = example()
        if (set(config) != set(template) or config['schema'] != SCHEMA or
                config['n'] != template['n'] or config['chemistry'] != template['chemistry'] or
                type(config['insulated']) is not bool):
            raise ValueError('water chamber configuration')
        for key in set(template) - {'schema', 'n', 'chemistry', 'insulated'}:
            if type(config[key]) not in (int, float) or not math.isfinite(config[key]):
                raise ValueError(f'invalid water field {key}')
        if not 0 < config['dt_s'] <= 1e9:
            raise ValueError('water timestep')

        self.config = copy.deepcopy(config)
        library = Path(library).resolve()
        self.engine = hashlib.sha256(library.read_bytes()).hexdigest()
        self.adapter = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
        self.lib = C.CDLL(str(library))
        signatures = {
            'chem_water_properties': [C.c_double, C.POINTER(Properties)],
            'chem_water_initialize': [C.POINTER(Model), C.c_double, C.c_double, C.POINTER(State)],
            'chem_water_step': [C.POINTER(Model), C.POINTER(State), C.c_double, C.POINTER(State)],
        }
        for name, arguments in signatures.items():
            function = getattr(self.lib, name, None)
            if function is None:
                raise ValueError('selected engine lacks RC7 water chamber support')
            function.argtypes = arguments
            function.restype = C.c_bool

        self.model = Model(*(config[key] for key, _ in Model._fields_))
        self.state = self.initialize(config['temperature_k'], config['initial_vapor_kg'])
        self.initial_energy = self.state.energy_j
        self.bath_heat = 0.
        self.tick = 0
        self.step(self.state, config['dt_s'])

    def initialize(self, temperature, vapor):
        state = State()
        if not self.lib.chem_water_initialize(self.model, temperature, vapor, state):
            raise ValueError('water chamber state outside model bounds')
        return state

    def step(self, state, dt):
        candidate = State()
        if not self.lib.chem_water_step(self.model, state, dt, candidate):
            raise ValueError('water chamber step outside model bounds')
        return candidate

    def advance(self, steps):
        if type(steps) is not int or not 0 <= steps <= 10000:
            raise ValueError('water step count')
        for _ in range(steps):
            candidate = self.step(self.state, self.config['dt_s'])
            self.bath_heat += candidate.energy_j - self.state.energy_j
            self.state = candidate
            self.tick += 1

    def report(self):
        state = self.state
        properties = Properties()
        if not self.lib.chem_water_properties(state.temperature_k, properties):
            raise ValueError('water property temperature')
        liquid = self.model.water_kg - state.vapor_kg
        liquid_volume = liquid / properties.liquid_density
        headspace = self.model.volume_m3 - liquid_volume
        relative_saturation = state.vapor_kg / (properties.vapor_density * headspace)
        net_vapor = state.vapor_kg - self.config['initial_vapor_kg']
        amounts = [liquid / MOLAR_MASS, state.vapor_kg / MOLAR_MASS]
        concentration = [properties.liquid_density / MOLAR_MASS if liquid else 0.,
                         amounts[1] / headspace]
        residual = liquid + state.vapor_kg - self.model.water_kg
        physical_energy = self.initialize(state.temperature_k, state.vapor_kg).energy_j
        return dict(
            tick=self.tick, time_s=self.tick * self.config['dt_s'],
            totals_mol=amounts, concentration_mol_m3=[concentration],
            extent_mol=[max(0., net_vapor) / MOLAR_MASS, max(0., -net_vapor) / MOLAR_MASS],
            species_budget_residual_mol=[residual / MOLAR_MASS, 0.],
            water=dict(
                temperature_k=state.temperature_k, saturation_pressure_pa=properties.pressure_pa,
                vapor_pressure_pa=relative_saturation * properties.pressure_pa,
                relative_saturation=relative_saturation,
                liquid_kg=liquid, vapor_kg=state.vapor_kg, liquid_volume_m3=liquid_volume,
                headspace_m3=headspace, mass_residual_kg=residual,
                internal_energy_j=state.energy_j, bath_heat_into_chamber_j=self.bath_heat,
                energy_residual_j=physical_energy - self.initial_energy - self.bath_heat,
                latent_internal_energy_j_kg=properties.vapor_u - properties.liquid_u,
                properties='IAPWS SR1-86(1992)', rate_model='specified equilibrium relaxation',
                insulated=self.model.insulated))

    def checkpoint(self):
        payload = dict(schema='rc7-water-checkpoint/v1', engine=self.engine,
                       adapter=self.adapter, config=digest(self.config), tick=self.tick,
                       state=[getattr(self.state, key) for key, _ in State._fields_],
                       bath_heat=self.bath_heat)
        return dict(payload=payload, sha256=digest(payload))

    def restore(self, packet):
        payload = packet['payload']
        expected = self.checkpoint()['payload']
        if (set(payload) != set(expected) or digest(payload) != packet['sha256'] or
                any(payload[key] != expected[key] for key in ('schema', 'engine', 'adapter', 'config'))):
            raise ValueError('water checkpoint identity')
        tick = payload['tick']
        heat = payload['bath_heat']
        values = payload['state']
        if (type(tick) is not int or not 0 <= tick <= 2**53 or len(values) != 3 or
                not all(math.isfinite(x) for x in [*values, heat])):
            raise ValueError('water checkpoint state')
        state = State(*values)
        self.step(state, 0.)
        if self.model.insulated:
            valid = state.energy_j == self.initial_energy and heat == 0.
        else:
            valid = (state.temperature_k == self.config['temperature_k'] and
                     math.isclose(state.energy_j - self.initial_energy, heat,
                                  rel_tol=1e-10, abs_tol=1e-10))
        if not valid:
            raise ValueError('water checkpoint energy ledger')
        self.state, self.bath_heat, self.tick = state, heat, tick


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library', required=True)
    parser.add_argument('--config', type=Path, required=True)
    parser.add_argument('--steps', type=int, default=1)
    parser.add_argument('--resume', type=Path)
    parser.add_argument('--checkpoint', type=Path)
    args = parser.parse_args()
    chamber = WaterChamber(args.library, json.loads(args.config.read_text()))
    if args.resume:
        chamber.restore(json.loads(args.resume.read_text()))
    chamber.advance(args.steps)
    print(json.dumps(chamber.report(), allow_nan=False))
    if args.checkpoint:
        with args.checkpoint.open('x') as output:
            json.dump(chamber.checkpoint(), output, allow_nan=False)


if __name__ == '__main__':
    main()
