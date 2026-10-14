import copy,json,math,os,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling import caloric as c,caloric_cases as cases,gas_mixture as nist,hotgas,hotgas_domain as domain
from coupling.hotgas_domain_cases import face
WORKER=os.environ['HOTGAS_DOMAIN_WORKER']

class CaloricTests(unittest.TestCase):
    def test_independent_nist_and_quadrature(self):
        probe=json.loads(Path(os.environ['CALORIC_NATIVE_PROBE']).read_text());mixtures=[[1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1],c.AIR,c.PRODUCTS]
        for row in probe:
            moles=mixtures[row['mixture']];t=row['temperature'];p=c.properties(moles,t)
            self.assertAlmostEqual(row['cp'],p['cp_j_kg_k'],delta=1e-8)
            self.assertAlmostEqual(row['energy'],p['energy_j_kg'],delta=1e-8)
            if t>=500:
                mapped={k:v for k,v in zip(('N2','O2','CO2','H2O'),moles) if v}
                self.assertLess(abs(row['cp']/nist.properties(mapped,t)['cp_j_kg_k']-1),.005)
        for moles in (c.AIR,c.PRODUCTS):
            # Independent midpoint integration of Cv, split at the published joins.
            energy=0.
            for lo,hi in ((300,500),(500,1000),(1000,1300)):
                step=(hi-lo)/1000;energy+=step*math.fsum(c.properties(moles,lo+(i+.5)*step)['cv_j_kg_k'] for i in range(1000))
            exact=c.properties(moles,1300)['energy_j_kg']-c.properties(moles,300)['energy_j_kg']
            self.assertAlmostEqual(exact,energy,delta=.02)
            for join in (500,1000):
                left=c.properties(moles,join-1e-7);right=c.properties(moles,join+1e-7)
                self.assertLess(abs(left['cp_j_kg_k']-right['cp_j_kg_k']),1e-5)
                self.assertAlmostEqual(c.invert(moles,right['energy_j_kg']),join+1e-7,delta=1e-8)

    def test_nist_heating_and_ambient_product_cooling(self):
        for moles,begin in ((c.AIR,300.),(c.PRODUCTS,500.)):
            r=cases.heating(WORKER,moles,begin);self.assertLess(abs(r['final_temperature_k']-1300),1.)
            self.assertLess(r['max_conservation_residual'],1e-8)
        row=cases.heating(WORKER,c.PRODUCTS,1300.,300.,False)
        self.assertAlmostEqual(row['final_temperature_k'],300.,delta=1e-8)

    def test_contact_and_smooth_energy_transport(self):
        for moles in (c.AIR,c.PRODUCTS):
            row=cases.thermal_contact(WORKER,moles)
            self.assertLess(row['max_density_error'],1e-12);self.assertLess(row['max_pressure_error_pa'],1e-6)
        rows=[cases.transport(WORKER,c.PRODUCTS,n) for n in (16,32,64)]
        errors=[r['relative_density_disturbance_l1'] for r in rows]
        self.assertLess(errors[-1],.01)
        self.assertGreater(errors[0]/errors[1],2.)
        self.assertGreater(errors[1]/errors[2],2.)
        self.assertLess(rows[-1]['max_pressure_error_pa'],1.)

    def test_boundary_enthalpy_and_conduction(self):
        moles=c.PRODUCTS;c0=cases.selected(moles);c0['domain']['faces']=[face('reservoir',temperature=900,velocity=(0,0,2)) for _ in range(6)]
        rho=101325/(c0['gas']['gas_constant_j_kg_k']*900);u=c.conserved(moles,rho,900,(0,0,2))*256
        row=cases.evolve(WORKER,c0,u,100);b=row['receipts'][-1]['boundary_budget'];dt=100e-6;area=.01
        expected=dt*area*rho*2*(c.properties(moles,900)['enthalpy_j_kg']+2)
        self.assertAlmostEqual(b['outward'][5][4],expected,delta=1e-7)
        c0=cases.selected(moles);c0['domain']['conductivity_w_m_k']=.026;c0['domain']['faces'][4]=face(wall_temperature=300)
        u=c.conserved(moles,1,1300)*256;source=[0.]*len(u);state=domain.initial(c0,WORKER,u,source,(1,1000000))
        obs=domain.observe(c0,WORKER,state,source,(1,1000000),[0.])['native_samples'][0]
        self.assertAlmostEqual(obs['conducted_downward_w_m2'][0],8320.,delta=1e-6)
        row=cases.evolve(WORKER,c0,u,100);self.assertGreater(row['receipts'][-1]['boundary_budget']['conducted_outward_j'][4],0.)
        self.assertFalse(domain.observe(c0,WORKER,row['final'],source,(1,1000000),[0.])['reciprocal_thermal_exchange_admissible'])

    def test_hydrostatic_restart_and_unsupported_controls(self):
        c0=cases.selected(c.AIR,gravity=(0,0,-9.81));u=cases.hydrostatic(c0);row=cases.evolve(WORKER,c0,u,300)
        self.assertLess(max(abs(a-b) for a,b in zip(u,row['final']['conserved'])),1e-7)
        source=row['source'];middle,_=domain.advance(c0,WORKER,row['initial'],source,71,row['rate'])
        final,_=domain.advance(c0,WORKER,json.loads(json.dumps(middle)),source,300,row['rate']);self.assertEqual(final,row['final'])
        other=copy.deepcopy(c0);other['gas']=c.gas_config(other['gas'],c.PRODUCTS)
        with self.assertRaises(ValueError):domain.advance(other,WORKER,middle,source,300,row['rate'])
        unsupported=[0.]*len(u);unsupported[4::5]=[1e9]*(len(u)//5)
        with self.assertRaises(ValueError):domain.native(c0,WORKER,u,unsupported,1e-6,1,[])
        with self.assertRaises(ValueError):c.properties(c.PRODUCTS,1500.01)
        with self.assertRaises(ValueError):c.properties([-1,0,0,1],300.)

if __name__=='__main__':unittest.main()
