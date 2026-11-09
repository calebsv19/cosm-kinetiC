"""Fixed-width droplet exchange and conservative capture at a rigid wall."""
import copy
import hashlib
import math
from pathlib import Path

from chemistry_droplet_carrier import DropletCarrier, example as carrier_example
from chemistry_reactor import digest

SCHEMA = 'rc7-droplet-deposition/v1'


def example(cells=24, wall=True):
    carrier = carrier_example(cells)
    if wall:
        carrier['gas_pressure_pa'] = [101325.]*cells
        carrier['gas_velocity_m_s'] = [0.]*cells
        carrier['droplets'][0].update(position_m=carrier['length_m']-11e-6,velocity_m_s=.008,count=3)
    return dict(schema=SCHEMA,carrier=carrier,n=carrier['n'],chemistry=carrier['chemistry'],
                boundary='capture' if wall else 'periodic',source_width_m=.003)


class FixedWidthCarrier(DropletCarrier):
    def stencil(self,x):
        if self.periodic:
            x %= self.length
        else:
            x = min(self.length,max(0.,x))
        half = .5*self.source_width
        images = (x-self.length,x,x+self.length) if self.periodic else (x,)
        weights = []
        for center in images:
            left,right = center-half,center+half
            begin = max(0,math.floor(left/self.dx))
            end = min(self.cells,math.ceil(right/self.dx))
            for cell in range(begin,end):
                overlap = min(right,(cell+1)*self.dx)-max(left,cell*self.dx)
                if overlap>0.:
                    weights.append((cell,overlap))
        total = math.fsum(w for _,w in weights)
        return [(cell,w/total) for cell,w in weights]


class DropletDeposition:
    def __init__(self, library, config):
        if set(config)!=set(example()) or config['schema']!=SCHEMA:
            raise ValueError('droplet deposition configuration')
        self.config = copy.deepcopy(config)
        self.periodic = config['boundary']=='periodic'
        if config['boundary'] not in ('periodic','capture'):
            raise ValueError('droplet deposition boundary')
        core = FixedWidthCarrier.__new__(FixedWidthCarrier)
        core.periodic = self.periodic
        width = config['source_width_m']
        if type(width) not in (int,float) or not math.isfinite(width) or not 0.<width<=config['carrier']['length_m']:
            raise ValueError('droplet deposition source width')
        core.source_width = width
        core.__init__(library,config['carrier'])
        if any(config[k]!=core.config[k] for k in ('n','chemistry')) or (not self.periodic and len(core.counts)!=1):
            raise ValueError('droplet deposition display or parcel configuration')
        self.core = core
        self.dt,self.engine = core.dt,core.engine
        self.adapter = digest([core.adapter,hashlib.sha256(Path(__file__).read_bytes()).hexdigest()])
        self.tick = 0
        # Core inventories, wall pressure impulse, impact impulse, impact heat,
        # capture side (-1 left, 0 airborne, +1 right), time of capture.
        self.state = [*core.state,0.,0.,0.,0.,0.]
        if not self.periodic and min(self.clearances(self.state))<=0.:
            raise ValueError('droplet deposition initial contact')
        self.step(self.state)

    def clearances(self,state):
        _,_,_,x,r,_ = self.core.particle(state[:-5],0)
        return x-r,self.core.length-x-r

    def seam_flux(self, state, gas):
        core = self.core
        left,right = gas[-1],gas[0]
        ul,ur = state[core.gas_end-4:core.gas_end],state[:4]
        a = max(abs(left[1])+left[5],abs(right[1])+right[5])
        fl = [q*left[1] for q in ul]
        fr = [q*right[1] for q in ur]
        fl[2] += left[3]*core.volume
        fr[2] += right[3]*core.volume
        fl[3] += left[3]*left[1]*core.volume
        fr[3] += right[3]*right[1]*core.volume
        return [.5*(l+r-a*(b-c))/core.dx for l,r,b,c in zip(fl,fr,ur,ul)]

    def derivative(self,state):
        core = self.core
        if state[-2]:
            gas_only = copy.copy(core)
            gas_only.counts = []
            rates = [*gas_only.derivative(state[:core.gas_end]),*([0.]*6)]
        else:
            rates = core.derivative(state[:-5])
        if self.periodic:
            return [*rates,0.,0.,0.,0.,0.]
        gas = core.gas_fields(state)
        seam = self.seam_flux(state,gas)
        last = core.gas_end-4
        for i,flux in enumerate(seam):
            rates[i] -= flux
            rates[last+i] += flux
        # Gauge the pressure before differencing. This avoids cancellation
        # between the common chamber pressure and small wall impulses.
        reference = gas[0][3]
        left,right = gas[0],gas[-1]
        wall_left = core.area*(left[3]-reference+left[0]*left[1]*(left[1]-abs(left[1])-left[5]))
        wall_right = core.area*(right[3]-reference+right[0]*right[1]*(right[1]+abs(right[1])+right[5]))
        fluxes = [wall_left]
        for left,right in zip(gas,gas[1:]):
            rho_l,u_l,_,p_l,_,c_l = left
            rho_r,u_r,_,p_r,_,c_r = right
            wave = max(abs(u_l)+c_l,abs(u_r)+c_r)
            fluxes.append(.5*core.area*((p_l-reference)+(p_r-reference)
                +rho_l*u_l*u_l+rho_r*u_r*u_r-wave*(rho_r*u_r-rho_l*u_l)))
        fluxes.append(wall_right)
        for cell,(left_flux,right_flux) in enumerate(zip(fluxes,fluxes[1:])):
            rates[4*cell+2] = left_flux-right_flux
        if not state[-2]:
            x = state[core.gas_end+3]
            for cell,w in core.stencil(x):
                rates[4*cell+2] -= core.counts[0]*w*rates[core.gas_end+1]
        return [*rates,wall_right-wall_left,0.,0.,0.,0.]

    def residuals(self,state):
        dry,water,momentum,energy = self.core.residuals(state[:-5])
        return (dry,water,math.fsum([momentum,state[-5],state[-4]]),energy)

    def validate(self,state):
        if (len(state)!=len(self.core.state)+5 or not all(math.isfinite(v) for v in state)
                or state[-2] not in (-1.,0.,1.) or state[-3]<0. or state[-1]<0.):
            raise ValueError('droplet deposition state')
        if (self.periodic and any(state[-5:]) or
                (not state[-2] and any(state[-4:]))):
            raise ValueError('droplet deposition wall ledger')
        if not self.periodic:
            clearances = self.clearances(state)
            if state[-2]:
                side = int(state[-2]>0)
                if clearances[side]!=0. and abs(clearances[side])>1e-15 or state[self.core.gas_end+1]!=0.:
                    raise ValueError('droplet deposition captured parcel state')
            elif min(clearances)<-1e-15:
                raise ValueError('droplet deposition missed contact')
        self.derivative(state)
        for error,base,floor in zip(self.residuals(state),self.core.baseline,(1e-18,1e-18,1e-16,1e-12)):
            if abs(error)>2e-10*max(abs(base),floor):
                raise ValueError('droplet deposition conservation ledger')
        for i,parcel in enumerate(self.config['carrier']['droplets']):
            j = self.core.gas_end+6*i
            mass = 4*math.pi/3*self.core.exchange.rho*parcel['radius_m']**3
            if abs(math.fsum([state[j],-mass,state[j+4],-state[j+5]]))>2e-10*mass:
                raise ValueError('droplet deposition phase extent ledger')

    def rk2(self,state,dt):
        first = self.derivative(state)
        stage = [x+dt*k for x,k in zip(state,first)]
        second = self.derivative(stage)
        return [x+.5*dt*(a+b) for x,a,b in zip(state,first,second)]

    def step(self,state,time=0.):
        candidate = self.rk2(state,self.dt)
        if not self.periodic and not state[-2]:
            left,right = self.clearances(candidate)
            if min(left,right)<=0.:
                side = int(right<=0.)
                low,high = 0.,self.dt
                for _ in range(42):
                    mid = .5*(low+high)
                    if self.clearances(self.rk2(state,mid))[side]>0.:
                        low = mid
                    else:
                        high = mid
                hit = self.rk2(state,high)
                j = self.core.gas_end
                m,p = hit[j:j+2]
                radius = self.core.particle(hit[:-5],0)[4]
                count = self.core.counts[0]
                hit[j+1] = 0.
                hit[j+3] = self.core.length-radius if side else radius
                hit[-4] = count*p
                hit[-3] = count*.5*p*p/m
                hit[-2] = 1. if side else -1.
                hit[-1] = time+high
                candidate = self.rk2(hit,self.dt-high)
        self.validate(candidate)
        return candidate

    def advance(self,steps):
        if type(steps) is not int or not 0<=steps<=10000 or self.tick+steps>2**53:
            raise ValueError('droplet deposition step count')
        candidate = self.state
        for i in range(steps):
            candidate = self.step(candidate,(self.tick+i)*self.dt)
        self.state = candidate
        self.tick += steps

    def checkpoint(self):
        p = dict(schema=SCHEMA,config=digest(self.config),adapter=self.adapter,engine=self.engine,
                 state=self.state.copy(),tick=self.tick)
        return dict(payload=p,sha256=digest(p))

    restore = DropletCarrier.restore

    def report(self):
        view = copy.copy(self.core)
        view.state = self.state[:-5]
        view.tick = self.tick
        report = view.report()
        carrier = report['droplet_carrier']
        carrier['momentum_residual_kg_m_s'] = self.residuals(self.state)[2]
        pressure,impulse,heat,side,time = self.state[-5:]
        deposit = carrier['parcels'].pop(0) if side else None
        report['wall_deposition'] = dict(boundary=self.config['boundary'],source_width_m=self.core.source_width,
            captured=bool(side),side=int(side),capture_time_s=time,deposited_parcel=deposit,
            deposited_water_kg=deposit['count']*deposit['mass_kg'] if deposit else 0.,
            deposited_total_energy_j=self.core.counts[0]*self.state[self.core.gas_end+2] if deposit else 0.,
            pressure_impulse_kg_m_s=pressure,impact_impulse_kg_m_s=impulse,impact_heat_j=heat)
        return report
