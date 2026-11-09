"""Conservative point-droplet exchange with solved periodic moist-gas flow."""
import copy
import hashlib
import math
from pathlib import Path

from chemistry_humid_droplet import AIR_R, HumidDroplet, example as humid_example
from chemistry_reactor import digest
from chemistry_stefan_interface import CV, MOLAR_MASS, R, REFERENCE_K, StefanInterface

SCHEMA = 'rc7-droplet-carrier/v1'
AIR_CV = 718.
AIR_MOLAR_MASS = .028965
SPECIES = {'species': [{'id': 'H2O_liquid'}, {'id': 'H2O_vapor'}, {'id': 'dry_air'}]}


def example(cells=24):
    return dict(schema=SCHEMA, chemistry=copy.deepcopy(SPECIES), n=[cells,1,1],
                length_m=.024, area_m2=1e-6, dt_s=5e-7, viscosity_pa_s=1.81e-5,
                gas_pressure_pa=[101325.+2*math.cos(math.tau*(i+.5)/cells) for i in range(cells)],
                gas_temperature_k=[298.15]*cells, gas_velocity_m_s=[.005]*cells,
                gas_relative_humidity=[.6]*cells, droplets=[
                    dict(position_m=.004,velocity_m_s=0.,radius_m=10e-6,temperature_k=298.15,count=1)])


class DropletCarrier:
    def __init__(self, library, config):
        if set(config)!=set(example()) or config['schema']!=SCHEMA or config['chemistry']!=SPECIES:
            raise ValueError('droplet carrier configuration')
        self.config = copy.deepcopy(config)
        self.cells,ny,nz = config['n']
        if any(type(n) is not int for n in (self.cells,ny,nz)) or not 8<=self.cells<=128 or (ny,nz)!=(1,1):
            raise ValueError('droplet carrier grid')
        self.length,self.area,self.dt,self.viscosity = (config[k] for k in (
            'length_m','area_m2','dt_s','viscosity_pa_s'))
        for value,low,high in ((self.length,.001,2.),(self.area,1e-9,1.),
                               (self.dt,1e-10,.01),(self.viscosity,1e-6,1e-4)):
            if type(value) not in (int,float) or not math.isfinite(value) or not low<=value<=high:
                raise ValueError('droplet carrier parameter bounds')
        self.dx = self.length/self.cells
        self.volume = self.dx*self.area
        self.exchange = HumidDroplet(library,dict(humid_example(),dt_s=min(self.dt,.0001)))
        self.engine = self.exchange.engine
        self.adapter = digest([self.exchange.adapter,hashlib.sha256(Path(__file__).read_bytes()).hexdigest()])
        self.latent = self.exchange.latent
        fields = [config[k] for k in ('gas_pressure_pa','gas_temperature_k','gas_velocity_m_s','gas_relative_humidity')]
        if any(len(f)!=self.cells for f in fields):
            raise ValueError('droplet carrier initial field shape')
        gas = []
        for p,t,u,rh in zip(*fields):
            if not all(type(v) in (int,float) and math.isfinite(v) for v in (p,t,u,rh)) or not 0.<=rh<=1.2:
                raise ValueError('droplet carrier initial field')
            vapor_pressure = rh*self.exchange.properties(t).pressure_pa
            dry = (p-vapor_pressure)*self.volume/(AIR_R*t)
            vapor = vapor_pressure*self.volume/(R*t)
            mass = dry+vapor
            energy = (AIR_CV*dry+CV*vapor)*(t-REFERENCE_K)+self.latent*vapor+.5*mass*u*u
            gas.extend([dry,vapor,mass*u,energy])
        self.gas_end = len(gas)
        particles = []
        self.counts = []
        if len(config['droplets'])>16:
            raise ValueError('droplet carrier parcel count')
        for parcel in config['droplets']:
            if set(parcel)!=set(example()['droplets'][0]):
                raise ValueError('droplet carrier parcel fields')
            x,v,r,t,count = (parcel[k] for k in ('position_m','velocity_m_s','radius_m','temperature_k','count'))
            if (any(type(y) not in (int,float) or not math.isfinite(y) for y in (x,v,r,t)) or
                    not 0<=x<self.length or not 2e-6<=r<=.05*self.dx or
                    type(count) is not int or not 1<=count<=10000):
                raise ValueError('droplet carrier parcel bounds')
            mass = 4*math.pi/3*self.exchange.rho*r**3
            particles.extend([mass,mass*v,self.exchange.drop_energy(mass,r,t)+.5*mass*v*v,x,0.,0.])
            self.counts.append(count)
        self.state = [*gas,*particles]
        self.tick = 0
        self.baseline = self.totals(self.state)
        self.step(self.state)

    def gas_fields(self,state):
        gas = []
        for i in range(0,self.gas_end,4):
            dry,vapor,momentum,energy = state[i:i+4]
            if dry<=0. or vapor<0.:
                raise ValueError('droplet carrier gas inventory')
            mass = dry+vapor
            velocity = momentum/mass
            capacity = dry*AIR_CV+vapor*CV
            gas_constant = dry*AIR_R+vapor*R
            temperature = REFERENCE_K+(energy-self.latent*vapor-.5*mass*velocity**2)/capacity
            pressure = gas_constant*temperature/self.volume
            if not (70000.<=pressure<=120000. and 283.15<=temperature<=323.15 and abs(velocity)<=2.):
                raise ValueError('droplet carrier gas pressure, temperature or speed bounds')
            sound = math.sqrt((1+gas_constant/capacity)*pressure*self.volume/mass)
            gas.append((mass/self.volume,velocity,temperature,pressure,vapor/mass,sound))
        return gas

    def stencil(self,x):
        left = (x%self.length)/self.dx-.5
        i = math.floor(left)
        weight = left-i
        return ((i%self.cells,1-weight),((i+1)%self.cells,weight))

    def particle(self,state,index):
        start = self.gas_end+6*index
        mass,momentum,energy,x,evap,cond = state[start:start+6]
        if mass<=0. or min(evap,cond)<0.:
            raise ValueError('droplet carrier liquid inventory')
        velocity = momentum/mass
        thermal = energy-.5*mass*velocity**2
        radius,t,_,_ = self.exchange.fields([mass,thermal,0.,0.,0.,0.,0.])
        if radius>.05*self.dx:
            raise ValueError('droplet carrier point-particle size bound')
        return mass,velocity,thermal,x,radius,t

    def derivative(self,state):
        if len(state)!=self.gas_end+6*len(self.counts) or not all(math.isfinite(x) for x in state):
            raise ValueError('droplet carrier state')
        gas = self.gas_fields(state)
        if self.dt*max(abs(g[1])+g[5] for g in gas)>.4*self.dx:
            raise ValueError('droplet carrier acoustic timestep bound')
        faces = []
        for i,left in enumerate(gas):
            j = (i+1)%self.cells
            right = gas[j]
            a = max(abs(left[1])+left[5],abs(right[1])+right[5])
            ul,ur = state[4*i:4*i+4],state[4*j:4*j+4]
            fl = [q*left[1] for q in ul]
            fr = [q*right[1] for q in ur]
            fl[2] += left[3]*self.volume
            fr[2] += right[3]*self.volume
            fl[3] += left[3]*left[1]*self.volume
            fr[3] += right[3]*right[1]*self.volume
            faces.append([.5*(l+r-a*(b-c))/self.dx for l,r,b,c in zip(fl,fr,ur,ul)])
        rates = [a-b for i in range(self.cells) for a,b in zip(faces[i-1],faces[i])]
        parcel_rates = []
        # Local carrier properties replace the prescribed ambient reservoir for
        # each stage. The same weights sample and deposit at the particle.
        exchange = copy.copy(self.exchange)
        for i,count in enumerate(self.counts):
            mass,v,thermal,x,radius,_ = self.particle(state,i)
            stencil = self.stencil(x)
            rho,u,t,p,y = (math.fsum(gas[cell][key]*w for cell,w in stencil) for key in range(5))
            exchange.gas_density,exchange.ambient,exchange.pressure,exchange.y_outer = rho,t,p,y
            slip = u-v
            beta = 6*math.pi*self.viscosity*radius
            if (max(2*radius*rho*abs(slip)/self.viscosity,
                    2*radius*abs(slip)/exchange.config['diffusivity_m2_s'],
                    2*radius*rho*1010.*abs(slip)/exchange.config['conductivity_w_m_k'])>.1
                    or self.dt*beta/mass>.25):
                raise ValueError('droplet carrier drag or transport bound')
            dm,du,evap,cond,*_ = exchange.derivative([mass,thermal,0.,0.,0.,0.,0.])
            force = beta*slip
            dp = force+v*dm
            de = du+v*force+.5*v*v*dm
            for cell,w in stencil:
                rates[4*cell+1] -= count*w*dm
                rates[4*cell+2] -= count*w*dp
                rates[4*cell+3] -= count*w*de
            parcel_rates.extend([dm,dp,de,v,evap,cond])
        return [*rates,*parcel_rates]

    def totals(self,state):
        gas = state[:self.gas_end]
        parcel = [state[self.gas_end+6*i:self.gas_end+6*i+6] for i in range(len(self.counts))]
        dry = math.fsum(gas[::4])
        combined = [math.fsum([*gas[k+1::4],*(n*p[k] for n,p in zip(self.counts,parcel))])
                    for k in range(3)]
        return (dry,*combined)

    def residuals(self,state):
        return [a-b for a,b in zip(self.totals(state),self.baseline)]

    def validate(self,state):
        self.derivative(state)
        for residual,scale,floor in zip(self.residuals(state),self.baseline,(1e-18,1e-18,1e-16,1e-12)):
            if abs(residual)>2e-10*max(abs(scale),floor):
                raise ValueError('droplet carrier conservation ledger')
        for i,count in enumerate(self.counts):
            start = self.gas_end+6*i
            initial = self.config['droplets'][i]['radius_m']
            initial_mass = 4*math.pi/3*self.exchange.rho*initial**3
            if abs(math.fsum([state[start],-initial_mass,state[start+4],-state[start+5]]))>2e-10*initial_mass:
                raise ValueError('droplet carrier phase extent ledger')

    step = StefanInterface.step
    advance = StefanInterface.advance
    restore = StefanInterface.restore

    def checkpoint(self):
        p = dict(schema=SCHEMA,config=digest(self.config),adapter=self.adapter,engine=self.engine,
                 state=self.state.copy(),tick=self.tick)
        return dict(payload=p,sha256=digest(p))

    def report(self):
        gas = self.gas_fields(self.state)
        particles = []
        for i,n in enumerate(self.counts):
            m,v,_,x,r,t = self.particle(self.state,i)
            start = self.gas_end+6*i
            particles.append(dict(count=n,mass_kg=m,radius_m=r,temperature_k=t,position_m=x%self.length,
                velocity_m_s=v,evaporated_kg=self.state[start+4],condensed_kg=self.state[start+5]))
        dry,water,momentum,energy = self.totals(self.state)
        liquid = math.fsum(p['count']*p['mass_kg'] for p in particles)
        air_residual,water_residual,momentum_residual,energy_residual = self.residuals(self.state)
        return dict(tick=self.tick,time_s=self.tick*self.dt,
            totals_mol=[liquid/MOLAR_MASS,(water-liquid)/MOLAR_MASS,dry/AIR_MOLAR_MASS],
            concentration_mol_m3=[[0.,rho*y/MOLAR_MASS,rho*(1-y)/AIR_MOLAR_MASS] for rho,_,_,_,y,_ in gas],
            extent_mol=[math.fsum(p['count']*p[k] for p in particles)/MOLAR_MASS
                        for k in ('evaporated_kg','condensed_kg')],
            species_budget_residual_mol=[0.,water_residual/MOLAR_MASS,air_residual/AIR_MOLAR_MASS],
            thermal=dict(temperature_k=[g[2] for g in gas],property_id='Ideal moist gas and IAPWS',
                         internal_ledger_residual_j=energy_residual),
            droplet_carrier=dict(parcels=particles,gas_velocity_m_s=[g[1] for g in gas],
                gas_pressure_pa=[g[3] for g in gas],gas_vapor_kg=self.state[1:self.gas_end:4],
                total_momentum_kg_m_s=momentum,total_energy_j=energy,dry_air_residual_kg=air_residual,
                water_residual_kg=water_residual,momentum_residual_kg_m_s=momentum_residual,
                energy_residual_j=energy_residual))
