"""Hydrostatic water sheet under independently sealed, compressible gas patches."""
import copy
import hashlib
import math
from pathlib import Path

from chemistry_reactor import digest
from chemistry_water_interface import MOLAR_MASS, WaterInterface, example as column_example
from chemistry_water_momentum import GRAVITY, WaterMomentum

SCHEMA = 'rc7-water-surface-pressure/v1'
DRY_R, DRY_CV, VAPOR_R = 287.05, 718., 461.5


def example(patches=6, cells=8):
    columns = []
    for i in range(patches):
        column = column_example(cells)
        mode = math.cos(math.pi*(i+.5)/patches)
        column.update(liquid_kg=.12, dt_s=.0002,
                      carrier_heat_capacity_j_k=.15*(1+.006*mode),
                      vapor_density_kg_m3=[.020-.010*mode]*cells)
        columns.append(column)
    return dict(schema=SCHEMA, n=[cells+1, patches, 1], chemistry=columns[0]['chemistry'],
                columns=columns, patch_length_m=.12, damping_per_s=2.)


class WaterSurfacePressure(WaterMomentum):
    def __init__(self, library, config):
        if set(config) != set(example()) or config['schema'] != SCHEMA:
            raise ValueError('pressure surface configuration')
        if not 3 <= len(config['columns']) <= 32:
            raise ValueError('pressure surface requires 3–32 patches')
        self.config = copy.deepcopy(config)
        self.columns = tuple(WaterInterface(library, c) for c in config['columns'])
        first = self.columns[0]
        self.count = len(self.columns)
        self.column_size = len(first.state)
        if (config['n'] != [first.cells+1, self.count, 1] or config['chemistry'] != first.config['chemistry'] or
                first.cells > 64 or any(c.cells != first.cells or c.area != first.area or
                c.height != first.height or c.config['dt_s'] != first.config['dt_s'] or
                c.config['bath'] for c in self.columns)):
            raise ValueError('pressure surface requires matching insulated patches')
        self.dt = first.config['dt_s']
        self.rho = first.density_liquid
        self.mass_per_m = first.area*self.rho
        self.depth = math.fsum(c.state[0] for c in self.columns)/(self.count*self.mass_per_m)
        self.dx, self.damping = config['patch_length_m'], config['damping_per_s']
        for value, low, high in ((self.dx, .01, 10.), (self.damping, 0., 50.)):
            if type(value) not in (int,float) or not math.isfinite(value) or not low <= value <= high:
                raise ValueError('pressure surface hydraulic parameter bounds')
        if self.depth > .2*self.dx:
            raise ValueError('pressure surface exceeds shallow depth bound')
        self.inertance = self.dx**2/(self.mass_per_m*self.depth)
        self.end = self.count*self.column_size
        self.faces = self.count-1
        self.state = [*(x for c in self.columns for x in c.state), *([0.]*(2*self.faces+1))]
        self.engine = first.engine
        self.adapter = digest([first.adapter, *(hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
                               for name in ('chemistry_water_momentum.py', 'chemistry_water_surface_pressure.py'))])
        self.tick = 0
        self.initial_energy = math.fsum(self.energies(self.state))
        self.step(self.state)

    def split(self, state):
        return [state[i:i+self.column_size] for i in range(0,self.end,self.column_size)]

    def pressure(self, column, local, field):
        volume = column.area*field[1]
        vapor = local[2:2+column.cells]
        dry = column.carrier_capacity/DRY_CV
        return [(DRY_R*dry+VAPOR_R*m)*t/volume for m,t in zip(vapor,field[3])]

    def fields(self, state):
        if (len(state) != self.end+2*self.faces+1 or not all(math.isfinite(x) for x in state) or state[-1] < 0):
            raise ValueError('invalid pressure surface state')
        if max(map(abs,state[self.end:self.end+self.faces])) > .25*self.mass_per_m:
            raise ValueError('pressure surface flow bound')
        fields = [c.fields(p) for c,p in zip(self.columns,self.split(state))]
        if max(abs(f[0]-self.depth) for f in fields) > .1*self.depth:
            raise ValueError('pressure surface displacement exceeds shallow perturbation bound')
        return fields

    def derivative(self, state):
        fields = self.fields(state)
        local = self.split(state)
        pressures = [self.pressure(c,p,f) for c,p,f in zip(self.columns,local,fields)]
        mean = [math.fsum(p)/len(p) for p in pressures]
        if any(not 50000 <= p <= 150000 for patch in pressures for p in patch):
            raise ValueError('pressure surface gas pressure bounds')
        stiffness = max(GRAVITY+1.5*p/(self.rho*(c.height-f[0]))
                        for p,c,f in zip(mean,self.columns,fields))
        if self.dt*max(2*math.sqrt(stiffness/(self.inertance*self.mass_per_m)),self.damping) > .2:
            raise ValueError('pressure surface elastic timestep bound')
        flow = state[self.end:self.end+self.faces]
        rates = [c.derivative(p) for c,p in zip(self.columns,local)]
        phase = [r[0] for r in rates]
        accelerations, drag = [], []
        for i,q in enumerate(flow):
            donor = i if q >= 0 else i+1
            if self.dt*abs(q) > .1*local[donor][0]:
                raise ValueError('pressure surface liquid transport timestep bound')
            advected = q*local[donor][1]/local[donor][0]
            heat = self.inertance*self.damping*q*q
            rates[i][0] -= q
            rates[i+1][0] += q
            rates[i][1] += .5*heat-advected
            rates[i+1][1] += .5*heat+advected
            head = GRAVITY*(fields[i][0]-fields[i+1][0])+(mean[i]-mean[i+1])/self.rho
            accelerations.append(head/self.inertance-self.damping*q)
            drag.append(heat)
        for c,r,p,f,j,pressure in zip(self.columns,rates,pressures,fields,phase,mean):
            # Liquid-volume change drives gas p dV work. The phase component
            # has its opposite pressure work in the liquid energy equation.
            for k,value in enumerate(p):
                r[2+c.cells+k] += value*r[0]/(self.rho*c.cells)
            r[1] -= pressure*j/self.rho
            r[2+c.cells] -= GRAVITY*f[0]*j
        return [*(x for row in rates for x in row), *accelerations, *flow, math.fsum(drag)]

    def energies(self, state):
        local = self.split(state)
        internal = math.fsum(x for c,p in zip(self.columns,local) for x in [p[1],*p[2+c.cells:-3]])
        gravity = .5*GRAVITY/self.mass_per_m*math.fsum(p[0]**2 for p in local)
        kinetic = .5*self.inertance*math.fsum(q*q for q in state[self.end:self.end+self.faces])
        return internal,gravity,kinetic

    def mass_residuals(self, state):
        transfer = [0.,*state[self.end+self.faces:-1],0.]
        residuals = []
        for i,(c,p) in enumerate(zip(self.columns,self.split(state))):
            gain = transfer[i]-transfer[i+1]
            total = math.fsum([p[0],*p[2:2+c.cells],-c.initial_mass,-gain])
            phase = math.fsum([p[0],-c.config['liquid_kg'],-gain,p[-3],-p[-2]])
            residuals.append((total,phase))
        return residuals

    def check_balances(self, state):
        self.derivative(state)
        for c,p,r in zip(self.columns,self.split(state),self.mass_residuals(state)):
            if max(map(abs,r)) > 1e-10*c.initial_mass or p[-1] != 0.:
                raise ValueError('pressure surface water ledger')
        if abs(math.fsum([*self.energies(state),-self.initial_energy])) > 2e-11*max(1.,abs(self.initial_energy)):
            raise ValueError('pressure surface total energy ledger')

    def checkpoint(self):
        payload = dict(schema=SCHEMA, config=digest(self.config), engine=self.engine,
                       adapter=self.adapter,state=self.state.copy(),tick=self.tick)
        return dict(payload=payload,sha256=digest(payload))

    def report(self):
        state = self.state
        fields = self.fields(state)
        rates = self.derivative(state)
        columns,concentration,temperature = [],[],[]
        for i,(c,p,f) in enumerate(zip(self.columns,self.split(state),fields)):
            h,width,t,gas_t,density,sat = f
            pressure = self.pressure(c,p,f)
            concentration.extend([[self.rho/MOLAR_MASS,0.], *[[0.,x/MOLAR_MASS] for x in density]])
            temperature.extend([t,*gas_t])
            columns.append(dict(position_m=h,edges_m=[0.,*(h+k*width for k in range(c.cells)),c.height],
                surface_velocity_m_s=rates[i*self.column_size]/self.mass_per_m,
                pressure_pa=math.fsum(pressure)/len(pressure),gas_pressure_pa=pressure,
                liquid_kg=p[0],vapor_kg=math.fsum(p[2:2+c.cells]),vapor_density_kg_m3=density,
                gas_temperature_k=gas_t,liquid_temperature_k=t,saturation_pressure_pa=sat.pressure_pa,
                evaporated_kg=p[-3],condensed_kg=p[-2]))
        residuals = self.mass_residuals(state)
        internal,gravity,kinetic = self.energies(state)
        energy_residual = math.fsum([internal,gravity,kinetic,-self.initial_energy])
        water_residual = math.fsum(r[0] for r in residuals)
        return dict(tick=self.tick,time_s=self.tick*self.dt,
            totals_mol=[math.fsum(c[k] for c in columns)/MOLAR_MASS for k in ('liquid_kg','vapor_kg')],
            concentration_mol_m3=concentration,
            extent_mol=[math.fsum(c[k] for c in columns)/MOLAR_MASS for k in ('evaporated_kg','condensed_kg')],
            species_budget_residual_mol=[water_residual/MOLAR_MASS,0.],
            thermal=dict(temperature_k=temperature,property_id='IAPWS and ideal-gas pressure'),
            hydraulic=dict(columns=columns,face_flow_kg_s=state[self.end:self.end+self.faces],
                face_transfer_kg=state[self.end+self.faces:-1],drag_heat_j=state[-1],
                internal_energy_j=internal,gravitational_energy_j=gravity,kinetic_energy_j=kinetic,
                column_water_residual_kg=[r[0] for r in residuals],phase_residual_kg=[r[1] for r in residuals],
                total_water_residual_kg=water_residual,total_energy_residual_j=energy_residual,
                pressure_surface=True))
