"""Planar liquid–pure-vapor interface with conservative moving-cell Euler transport."""
import copy
import ctypes as C
import hashlib
import math
from pathlib import Path

from chemistry_reactor import digest
from chemistry_water import MOLAR_MASS, Properties
from chemistry_water_interface import LIQUID_HEAT_CAPACITY, REFERENCE_K, SPECIES, VAPOR_HEAT_CAPACITY

SCHEMA = 'rc7-stefan-interface/v1'
R = 461.5
CV = VAPOR_HEAT_CAPACITY
GAMMA = 1 + R/CV


def example(cells=24, condense=False):
    return dict(schema=SCHEMA, chemistry=copy.deepcopy(SPECIES), n=[cells+1,1,1],
                area_m2=.01, height_m=.03, liquid_kg=.06, liquid_temperature_k=REFERENCE_K,
                gas_pressure_pa=[3330. if condense else 3010.]*cells,
                gas_temperature_k=[REFERENCE_K]*cells, gas_velocity_m_s=[0.]*cells,
                permeance_kg_m2_s_pa=2e-5, dt_s=2e-7)


class StefanInterface:
    def __init__(self, library, config):
        if set(config)!=set(example()) or config['schema']!=SCHEMA or config['chemistry']!=SPECIES:
            raise ValueError('Stefan configuration')
        nx,ny,nz = config['n']
        if type(nx) is not int or not 5<=nx<=257 or [ny,nz]!=[1,1]:
            raise ValueError('Stefan grid')
        self.cells = nx-1
        self.config = copy.deepcopy(config)
        self.area,self.height,self.dt,self.permeance = (config[key] for key in (
            'area_m2','height_m','dt_s','permeance_kg_m2_s_pa'))
        for value,low,high in ((self.area,1e-6,1.),(self.height,.001,1.),(self.dt,1e-10,.01),
                              (self.permeance,0.,.001),(config['liquid_kg'],1e-8,1000.),
                              (config['liquid_temperature_k'],283.15,323.15)):
            if type(value) not in (int,float) or not math.isfinite(value) or not low<=value<=high:
                raise ValueError('Stefan parameter bounds')
        lib = Path(library).resolve()
        self.engine = hashlib.sha256(lib.read_bytes()).hexdigest()
        self.lib = C.CDLL(str(lib))
        self.get_properties = getattr(self.lib,"chem_water_properties",None)
        if self.get_properties is None:
            raise ValueError("selected engine lacks RC7 water properties")
        self.get_properties.argtypes = [C.c_double,C.POINTER(Properties)]
        self.get_properties.restype = C.c_bool
        reference = self.properties(REFERENCE_K)
        self.rho_liquid = reference.liquid_density
        self.latent = reference.vapor_u-reference.liquid_u
        self.adapter = digest({name: hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
                               for name in ('chemistry_stefan_interface.py','chemistry_water_interface.py',
                                            'chemistry_water.py','chemistry_reactor.py')})
        liquid = config['liquid_kg']
        _,dx = self.geometry(liquid)
        fields = [config[key] for key in ('gas_pressure_pa','gas_temperature_k','gas_velocity_m_s')]
        if any(len(a)!=self.cells for a in fields):
            raise ValueError('Stefan initial field shape')
        gas = []
        for pressure,temperature,velocity in zip(*fields):
            if not all(type(v) in (int,float) and math.isfinite(v) for v in (pressure,temperature,velocity)):
                raise ValueError('Stefan initial field')
            self.admit_gas(pressure,temperature,velocity)
            mass = pressure/(R*temperature)*self.area*dx
            gas.extend([mass,mass*velocity,mass*(self.latent+CV*(temperature-REFERENCE_K)+.5*velocity**2)])
        self.state = [liquid,liquid*LIQUID_HEAT_CAPACITY*(config['liquid_temperature_k']-REFERENCE_K),
                      *gas,0.,0.,0.]
        self.baseline = self.totals(self.state)
        self.tick = 0
        self.step(self.state)

    def properties(self, temperature):
        result = Properties()
        if not self.get_properties(temperature,result):
            raise ValueError('Stefan liquid temperature bounds')
        return result

    def geometry(self, liquid):
        position = liquid/(self.rho_liquid*self.area)
        if not 0.<position<=.9*self.height:
            raise ValueError('Stefan liquid or gas inventory geometry')
        return position,(self.height-position)/self.cells

    @staticmethod
    def admit_gas(pressure, temperature, velocity):
        if not (500.<=pressure<=15000. and 283.15<=temperature<=323.15 and abs(velocity)<=10.):
            raise ValueError('Stefan gas pressure, temperature or speed bounds')

    def fields(self, state):
        if (len(state)!=5+3*self.cells or not all(math.isfinite(v) for v in state)
                or min(state[-3:-1])<0.):
            raise ValueError('Stefan state')
        position,dx = self.geometry(state[0])
        temperature = REFERENCE_K+state[1]/(state[0]*LIQUID_HEAT_CAPACITY)
        saturation = self.properties(temperature).pressure_pa
        gas = []
        for i in range(2,len(state)-3,3):
            mass,momentum,energy = state[i:i+3]
            if mass<=0.:
                raise ValueError('Stefan gas cell exhausted')
            rho = mass/(self.area*dx)
            velocity = momentum/mass
            t = REFERENCE_K+(energy/mass-self.latent-.5*velocity**2)/CV
            pressure = rho*R*t
            self.admit_gas(pressure,t,velocity)
            gas.append((rho,velocity,t,pressure,energy/mass))
        return position,dx,temperature,saturation,gas

    def interface(self, gas, saturation):
        rho,u,_,p,_ = gas
        impedance = math.sqrt(GAMMA*p*rho)
        beta = 1/rho-1/self.rho_liquid
        j = self.permeance*(saturation-p+impedance*u)/(1+self.permeance*impedance*beta)
        speed = -j/self.rho_liquid
        vapor_speed = j*beta
        pressure = p+impedance*(vapor_speed-u)
        temperature = pressure/(rho*R)
        self.admit_gas(pressure,temperature,vapor_speed)
        liquid_pressure = pressure+j*vapor_speed
        vapor_energy = self.latent+CV*(temperature-REFERENCE_K)+.5*vapor_speed**2
        return j,speed,vapor_speed,pressure,liquid_pressure,vapor_energy

    @staticmethod
    def flux(left, right, mesh_speed):
        rho_l,u_l,_,p_l,e_l = left
        rho_r,u_r,_,p_r,e_r = right
        a = max(abs(u_l-mesh_speed)+math.sqrt(GAMMA*p_l/rho_l),
                abs(u_r-mesh_speed)+math.sqrt(GAMMA*p_r/rho_r))
        ul = (rho_l,rho_l*u_l,rho_l*e_l)
        ur = (rho_r,rho_r*u_r,rho_r*e_r)
        fl = (ul[0]*(u_l-mesh_speed),ul[1]*(u_l-mesh_speed)+p_l,ul[2]*(u_l-mesh_speed)+p_l*u_l)
        fr = (ur[0]*(u_r-mesh_speed),ur[1]*(u_r-mesh_speed)+p_r,ur[2]*(u_r-mesh_speed)+p_r*u_r)
        return [.5*(l+r-a*(b-c)) for l,r,b,c in zip(fl,fr,ur,ul)]

    def derivative(self, state):
        _,dx,_,saturation,gas = self.fields(state)
        j,speed,u,p,traction,e = self.interface(gas[0],saturation)
        if self.dt*max(abs(g[1])+abs(speed)+math.sqrt(GAMMA*g[3]/g[0]) for g in gas)>.4*dx:
            raise ValueError('Stefan acoustic timestep bound')
        # Gas energy flux is j*(e+p/rho) + p*interface_speed.
        exchange = j*e+p*u
        faces = [[j,traction,exchange]]
        for i,(left,right) in enumerate(zip(gas,gas[1:]),1):
            faces.append(self.flux(left,right,speed*(1-i/self.cells)))
        rho,u,t,p,e = gas[-1]
        # Reflection has zero mass and energy flux at the rigid upper wall.
        wall = self.flux(gas[-1],(rho,-u,t,p,e),0.)[1]
        faces.append([0.,wall,0.])
        return [-self.area*j,-self.area*exchange,
                *(self.area*(a-b) for l,r in zip(faces,faces[1:]) for a,b in zip(l,r)),
                self.area*max(0.,j),self.area*max(0.,-j),self.area*(traction-wall)]

    def totals(self, state):
        return (math.fsum([state[0],*state[2:-3:3]]),math.fsum(state[3:-3:3]),
                math.fsum([state[1],*state[4:-3:3]]))

    def residuals(self, state):
        mass,momentum,energy = self.totals(state)
        return mass-self.baseline[0],math.fsum([momentum,-self.baseline[1],-state[-1]]),energy-self.baseline[2]

    def validate(self, state):
        self.derivative(state)
        mass,momentum,energy = self.residuals(state)
        extent = math.fsum([state[0],-self.config['liquid_kg'],state[-3],-state[-2]])
        if (max(abs(mass),abs(extent))>2e-11*self.baseline[0] or abs(momentum)>1e-11 or
                abs(energy)>2e-11*max(1.,abs(self.baseline[2]))):
            raise ValueError('Stefan conservation ledger')

    def step(self, state):
        first = self.derivative(state)
        stage = [a+self.dt*b for a,b in zip(state,first)]
        second = self.derivative(stage)
        accepted = [a+.5*self.dt*(b+c) for a,b,c in zip(state,first,second)]
        self.validate(accepted)
        return accepted

    def advance(self, steps):
        if type(steps) is not int or not 0<=steps<=10000 or self.tick+steps>2**53:
            raise ValueError('Stefan step count')
        candidate = self.state
        for _ in range(steps):
            candidate = self.step(candidate)
        self.state = candidate
        self.tick += steps

    def checkpoint(self):
        p = dict(schema=SCHEMA, config=digest(self.config), adapter=self.adapter, engine=self.engine,
                 state=self.state.copy(),tick=self.tick)
        return dict(payload=p,sha256=digest(p))

    def restore(self, packet):
        p = packet['payload']
        expected = self.checkpoint()['payload']
        if (set(p)!=set(expected) or packet['sha256']!=digest(p) or
                any(p[k]!=expected[k] for k in ('schema','config','adapter','engine')) or
                type(p['tick']) is not int or not 0<=p['tick']<=2**53):
            raise ValueError('Stefan checkpoint identity')
        self.validate(p['state'])
        self.state = p['state'].copy()
        self.tick = p['tick']

    def report(self):
        state = self.state
        height,dx,t,saturation,gas = self.fields(state)
        j,speed,u,p,traction,e = self.interface(gas[0],saturation)
        mass_error,momentum_error,energy_error = self.residuals(state)
        liquid,vapor = state[0],math.fsum(state[2:-3:3])
        return dict(tick=self.tick,time_s=self.tick*self.dt,
            totals_mol=[liquid/MOLAR_MASS,vapor/MOLAR_MASS],
            concentration_mol_m3=[[self.rho_liquid/MOLAR_MASS,0.],*[[0.,g[0]/MOLAR_MASS] for g in gas]],
            extent_mol=[state[-3]/MOLAR_MASS,state[-2]/MOLAR_MASS],
            species_budget_residual_mol=[mass_error/MOLAR_MASS,0.],
            thermal=dict(temperature_k=[t,*[g[2] for g in gas]],
                         property_id='IAPWS and ideal water vapor',internal_ledger_residual_j=energy_error),
            water_interface=dict(position_m=height,
                edges_m=[0.,*(height+i*dx for i in range(self.cells)),self.height],
                displacement_m=height-self.geometry(self.config['liquid_kg'])[0],
                saturation_pressure_pa=saturation,liquid_temperature_k=t,
                bath_heat_into_chamber_j=0.,energy_residual_j=energy_error),
            stefan=dict(mass_flux_kg_m2_s=j,interface_velocity_m_s=speed,vapor_face_velocity_m_s=u,
                vapor_face_density_kg_m3=gas[0][0],vapor_pressure_pa=p,liquid_pressure_pa=traction,
                interface_energy_flux_w_m2=j*e+p*u,
                gas_pressure_pa=[g[3] for g in gas],gas_velocity_m_s=[g[1] for g in gas],
                liquid_kg=liquid,vapor_kg=vapor,evaporated_kg=state[-3],condensed_kg=state[-2],
                wall_impulse_kg_m_s=state[-1],momentum_kg_m_s=self.totals(state)[1],
                mass_residual_kg=mass_error,momentum_residual_kg_m_s=momentum_error,
                energy_residual_j=energy_error))
