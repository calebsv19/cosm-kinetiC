"""Separate native restart partitioning from coupled exchange/source timing."""
import json,os,sqlite3,sys,unittest
from contextlib import closing
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from coupling.journal import Journal
from coupling.fluid_packets import run
class NativePartitionControl(unittest.TestCase):
    def test_same_accepted_gas_prefix_and_zero_source_are_partition_exact(self):
        baseline=Path(os.environ['OVERNIGHT_BASELINE_ROOT']);c=json.loads((baseline/'config.json').read_text())
        with closing(sqlite3.connect((baseline/'run/joint.sqlite3').resolve().as_uri()+'?mode=ro',uri=True)) as db:
            cp=Journal(c['journal_codec']).decode(db.execute('select payload from checkpoints where revision=4').fetchone()[0])
        self.assertEqual(cp['fluid']['state']['data']['time_s'],1.6)
        root=Path(os.environ['OVERNIGHT_PACKET_TEST_ROOT']);root.mkdir(parents=True,exist_ok=False)
        old=os.environ.get('PHYSICS_SIM_EXPERIMENT_ROOT');os.environ['PHYSICS_SIM_EXPERIMENT_ROOT']=str(root/'attempts')
        self.addCleanup(lambda:os.environ.pop('PHYSICS_SIM_EXPERIMENT_ROOT',None) if old is None else os.environ.__setitem__('PHYSICS_SIM_EXPERIMENT_ROOT',old))
        zero={'energy_j':[0.]*32**3,'smoke_kg':[0.]*32**3};steps=[zero]*80
        whole=run(c['fluid'],cp['fluid']['state'],steps,c['fluid_worker'],80)
        split=run(c['fluid'],cp['fluid']['state'],steps,c['fluid_worker'],40)
        self.assertEqual(whole['state'],split['state'])
        d=whole['state']['data'];self.assertEqual(d['steps'],400);self.assertEqual(d['time_s'],2.)
        self.assertEqual(d['input_energy_j'],cp['fluid']['state']['data']['input_energy_j'])
        (root/'result.json').write_text(json.dumps({'status':'passed','equal_state_digest':whole['state']['digest'],'parent_joint_digest':cp['digest'],'interval_s':[1.6,2.0],'native_partitions':[80,40],'scope':'Real32cubed gas/object field,identical zero source and prescribed wind;this isolates native packet restart from changes in Fire emission timing/feedback hold.'},indent=2)+'\n')
if __name__=='__main__':unittest.main()
