import copy,json,math,os,subprocess,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling import mixture as m,mixture_cases as cases,hotgas
from fire_lowmach_qualification import save
WORKER=os.environ['MIXTURE_WORKER'];OUT=Path(os.environ['MIXTURE_TEST_OUTPUT']);OUT.mkdir(parents=True,exist_ok=False)

def call(args,values):
    p=subprocess.run([WORKER,*args],input=' '.join(format(v,'.17g') for v in values),text=True,capture_output=True,timeout=60)
    if p.returncode:raise ValueError('native probe refused '+str(p.returncode))
    return json.loads(p.stdout)

class MixtureTests(unittest.TestCase):
    def test_native_caloric_and_formation_reference(self):
        probe=json.loads(Path(os.environ['MIXTURE_NATIVE_PROBE']).read_text())
        for row in probe:
            y=[0.]*5;y[row['species']]=1.;p=m.properties(y,row['t'])
            self.assertAlmostEqual(row['e'],p['energy'],delta=1e-7)
            self.assertAlmostEqual(row['cp'],p['cp'],delta=1e-8)
            self.assertAlmostEqual(row['formation'],m.FORMATION[row['species']],delta=1e-8)
        # Independent NIST recommended ideal-vapor Cp table, not NASA coefficients.
        table=[(200,39.71),(298.15,44.06),(300,44.17),(400,51.63),(500,59.70),(600,67.19),(700,73.86),(800,79.76),(900,84.95),(1000,89.54),(1100,93.57),(1200,97.12),(1300,100.24),(1400,102.98),(1500,105.40)]
        errors=[]
        for t,cp in table:
            row=call(['properties'],[0,0,0,0,1,t]);errors.append(abs(row['cp']*m.MASS[4]/cp-1));self.assertLess(errors[-1],.006)
        # Gas-water products. NIST methanol flame calorimetry's -763.68 kJ/mol
        # is NOT this gas-water reaction enthalpy; no liquid/vapor conflation.
        dh=m.FORMATION[2]*m.MASS[2]+2*m.FORMATION[3]*m.MASS[3]-m.FORMATION[4]*m.MASS[4]
        reference=-393522.4-2*241826.4+201490.
        self.assertLess(abs(dh/reference-1),.001)
        save(OUT,'property-reference',dict(max_methanol_cp_relative_error=max(errors),gaseous_reaction_enthalpy_j_mol=dh,nist_formation_reference_j_mol=reference))

    def test_zero_sum_diffusion_and_enthalpy_flux(self):
        yl=[.7,.2,0,0,.1];yr=[.6,.2,0,0,.2];l=m.conserved(yl,1.,600.);r=m.conserved(yr,1.,700.)
        for axis in (0,1,2):
            row=call(['flux',str(axis),'.01','.2','.1'],l+r);d=row['diffusive'];j=[-.01*(b-a)/.1 for a,b in zip(yl,yr)]
            self.assertAlmostEqual(math.fsum(d[5:]),0.,delta=1e-15)
            for actual,expected in zip(d[5:],j):self.assertAlmostEqual(actual,expected,delta=1e-15)
            h=[m.pure(i,650.)[0]+m.R/m.MASS[i]*650 for i in range(5)]
            expected=-.2*(700-600)/.1+math.fsum(a*b for a,b in zip(h,j))
            self.assertAlmostEqual(d[4],expected,delta=1e-7)
            reverse=call(['flux',str(axis),'.01','.2','.1'],r+l)
            for a,b in zip(d,reverse['diffusive']):self.assertAlmostEqual(a,-b,delta=1e-8)
        save(OUT,'diffusive-flux',dict(row=row,expected_energy_flux_w_m2=expected,scope='Prescribed equal-D closure; full formation+sensible species enthalpy included.'))

    def test_diffusion_increment_refinement(self):
        errors=[]
        for n in (16,32,64):
            row=cases.diffusion(WORKER,n);save(OUT,'diffusion-'+str(n),row);errors.append(row['relative_diffusion_increment_l1'])
        self.assertLess(errors[-1],.001)
        for a,b in zip(errors,errors[1:]):self.assertGreater(a/b,3.5)

    def test_evolving_species_advection_and_conservation(self):
        meshes=(32,) if os.environ.get('MIXTURE_BOUNDED_SANITIZER') else (16,32,64)
        errors=[]
        for n in meshes:
            row=cases.advect(WORKER,n);save(OUT,'advection-'+str(n),row);errors.append(row['relative_fuel_l1'])
            self.assertLess(row['max_pressure_error_pa'],10.)
            self.assertLess(row['relative_energy_disturbance_l1'],.02)
            before=row['receipts'][0]['elements_before'];after=row['receipts'][-1]['elements_after']
            for a,b in zip(before,after):self.assertAlmostEqual(a,b,delta=1e-10)
        self.assertLess(errors[-1],.01)
        if len(errors)>1:
            for a,b in zip(errors,errors[1:]):self.assertGreater(a/b,1.7)

    def test_stationary_and_moving_composition_contact(self):
        row=cases.contact(WORKER);save(OUT,'stationary-contact',row)
        self.assertLess(row['max_pressure_error_pa'],1e-6)
        self.assertLess(row['max_state_relative_error'],1e-12)
        moving=cases.contact(WORKER,10.);save(OUT,'moving-contact-assessment',moving)
        # Numerical conservation does not establish isobaric mixing accuracy.
        self.assertTrue(math.isfinite(moving['max_pressure_error_pa']))

    def test_chemical_accounting_sources_restart_and_refusal(self):
        row=cases.chemical_accounting(WORKER);save(OUT,'chemical-accounting',row)
        self.assertGreater(row['final_temperature_k'],500.);self.assertLess(row['final_temperature_k'],1500.)
        self.assertEqual(row['initial']['conserved'][4],row['final']['conserved'][4])
        receipt=row['receipts'][0]
        for a,b in zip(receipt['elements_before'],receipt['elements_after']):self.assertAlmostEqual(a,b,delta=1e-10)
        for v in receipt['source_elements']:self.assertAlmostEqual(v,0.,delta=1e-12)
        # Independent fixed-volume energy root using pure-species reference.
        out=row['final']['conserved'][:10];y=[v/out[0] for v in out[5:]];lo,hi=500.,1500.
        for _ in range(50):
            t=(lo+hi)/2
            if m.properties(y,t)['energy']<row['initial']['conserved'][4]:lo=t
            else:hi=t
        self.assertAlmostEqual(row['final_temperature_k'],(lo+hi)/2,delta=1e-8)
        c=cases.config(8,diffusion=.001,conductivity=.05);ys=m.mass_fractions([3.76,1,.02,.01,.1]);u=[]
        for q in range(128):u+=m.conserved(ys,1.,500.+5*math.sin(2*math.pi*(q%8+.5)/8),(1.,0.,0.))
        source=[0.]*len(u);a=m.initial(c,WORKER,u,source,(1,1000000));full,_=m.advance(c,WORKER,a,source,20,(1,1000000))
        mid,_=m.advance(c,WORKER,a,source,7,(1,1000000));rest,_=m.advance(c,WORKER,json.loads(json.dumps(mid)),source,20,(1,1000000));self.assertEqual(full,rest)
        changed=copy.deepcopy(c);changed['diffusion_m2_s']=.002
        with self.assertRaises(ValueError):m.advance(changed,WORKER,mid,source,20,(1,1000000))
        with self.assertRaises(ValueError):m.advance(c,WORKER,mid,[1.]*len(u),20,(1,1000000))
        bad=copy.deepcopy(a);bad['conserved'][5]=-1.;bad['digest']=hotgas.digest(bad)
        with self.assertRaises(ValueError):m.advance(c,WORKER,bad,source,20,(1,1000000))
        # Uniform injected fuel: mass, species and formation+sensible energy agree.
        pure=m.conserved([0,0,0,0,1],.001,400.);inject=pure*128
        uniform=m.conserved(ys,1.,500.)*128
        admitted=m.initial(c,WORKER,uniform,inject,(1,1000000))
        inj,ledger=m.advance(c,WORKER,admitted,inject,1,(1,1000000))
        self.assertAlmostEqual(inj['conserved'][0],1.001,delta=1e-13)
        self.assertAlmostEqual(inj['conserved'][9],uniform[9]+.001,delta=1e-13)
        self.assertAlmostEqual(inj['conserved'][4],uniform[4]+pure[4],delta=1e-8)
        save(OUT,'fuel-injection',dict(config=c,initial=admitted,final=inj,receipt=ledger))
        save(OUT,'restart',dict(config=c,full=full,recovered=rest,exact_restart=True))

if __name__=='__main__':unittest.main()
