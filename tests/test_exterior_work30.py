import copy,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling import exterior_work30 as w
import open_atmosphere as ordinary

class WorkCapacity(unittest.TestCase):
 def config(self):
  return dict(rig_geometry=dict(schema='physics_sim_exterior_room/v1',window_profile='small',exterior_profile='far',computation_profile=w.PROFILE),grid=[64]*3,length_m=[2]*3,momentum_dt_s=.005,scalar_transport='mc_ssprk2/v1',resource_limits=dict(scalar_work_cells=w.WORK_LIMIT),open_domain_forcing=dict(knots=[dict(step=0,target_m_s=0,strength=0)]))
 def test_whole_horizon_and_no_reset(self):
  c=self.config();w.validate(c);w.validate(c,dict(data=dict(steps=1900,scalar_work_cells=1900*64**3*2)),[{}]*20)
  with self.assertRaises(ValueError):w.validate(c,dict(data=dict(steps=1900,scalar_work_cells=0)))
  with self.assertRaises(ValueError):w.validate(c,dict(data=dict(steps=6000,scalar_work_cells=6000*64**3*2)),[{}])
 def test_closed_capacity_and_unchanged_ordinary_policy(self):
  original=ordinary.integer;private=w.policy();private.integer(w.WORK_LIMIT,100000000,1000000000)
  self.assertIs(ordinary.integer,original)
  with self.assertRaises(ValueError):ordinary.integer(w.WORK_LIMIT,100000000,1000000000)
  for path,value in [('grid',[32]*3),('momentum_dt_s',.01)]:
   c=self.config();c[path]=value
   with self.assertRaises(ValueError):w.validate(c)
  c=self.config();c['resource_limits']['scalar_work_cells']-=1
  with self.assertRaises(ValueError):w.validate(c)
  self.assertEqual(w.WORK_LIMIT,6000*64**3*2*4096)

if __name__=='__main__':unittest.main()
