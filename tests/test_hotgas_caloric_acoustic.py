import os,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling import caloric_acoustic as a
from fire_lowmach_qualification import save

class CaloricAcoustic(unittest.TestCase):
    def test_variable_caloric_acoustic_phase(self):
        grid=[64] if os.environ.get('CALORIC_ACOUSTIC_FINE_ONLY') else [16,32,64]
        rows=[]
        for n in grid:
            row=a.case(os.environ['HOTGAS_DOMAIN_WORKER'],n);rows.append(row)
            save(Path(os.environ['CALORIC_ACOUSTIC_OUTPUT']),'acoustic-'+str(n),row)
        self.assertLess(rows[-1]['relative_pressure_error'],.01)
        self.assertLess(rows[-1]['nist_relative_pressure_error'],.01)
        if len(rows)==3:
            for left,right in zip(rows,rows[1:]):self.assertGreater(left['relative_pressure_error']/right['relative_pressure_error'],2.5)

if __name__=='__main__':unittest.main()
