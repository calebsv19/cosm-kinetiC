"""Control source-only packet grouping at fixed physical wind and native dt."""
import base64,json,math,os,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from coupling.framework import load_fire
class FirePartitionControl(unittest.TestCase):
    def test_constant_wind_native_state_and_emission_partition(self):
        root=Path(os.environ['OVERNIGHT_FIRE_PARTITION_ROOT']);root.mkdir(parents=True,exist_ok=False)
        baseline=Path(os.environ['OVERNIGHT_BASELINE_ROOT']);config=json.loads((baseline/'config.json').read_text());fire=load_fire(config['growth_root']);worker=config['fire_worker'];outcomes=[]
        for velocity in (0.,.25):
            folder=root/('zero-wind' if velocity==0 else 'constant-wind');folder.mkdir();wind={'schema':'growth_sim_fire_wind_input/v1','units':'m/s','tangent_velocity_m_s':[[velocity,0.] for _ in range(17*17)]};(folder/'wind.json').write_text(json.dumps(wind));finals=[];emissions=[]
            for ticks in (20,10):
                parent=fire.initial(config['source'],worker,'source-partition-init-'+str(ticks));(folder/('initial-'+str(ticks)+'.json')).write_text(json.dumps(parent));amount=[]
                for i in range(80//ticks):
                    parent=fire.advance(parent,worker,ticks,'source-partition-'+str(ticks)+'-'+str(i),wind=wind);fire.bundle_valid(parent);amount.append(parent['frame']['totals']['energy_transferred_j']);(folder/('ticks-%d-frame-%02d.json'%(ticks,i))).write_text(json.dumps(parent))
                finals.append(parent);emissions.append(math.fsum(amount))
            self.assertEqual(finals[0]['checkpoint']['native_state_b64'],finals[1]['checkpoint']['native_state_b64'])
            native=fire.state_decode(base64.b64decode(finals[0]['checkpoint']['native_state_b64']));self.assertEqual(native['tick'],80);self.assertLess(abs(emissions[0]-emissions[1]),1e-11)
            outcomes.append({'constant_wind_x_m_s':velocity,'final_native_sha256':finals[0]['checkpoint']['native_state_sha256'],'native_tick':80,'whole_vs_split_emitted_j':emissions,'scope':'Same fixed native Fire timestep and wind;20vs10tick source packets. Bundle lineage differs intentionally; physical native bytes must match.'})
        (root/'result.json').write_text(json.dumps({'status':'passed','controls':outcomes},indent=2)+'\n')
if __name__=='__main__':unittest.main()
