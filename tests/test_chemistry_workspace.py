import copy,json,os,sys,tempfile,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from chemistry_workspace import execute,restore,accept,preset,publish_new
from chemistry_transport import Spatial
LIB=os.environ['CHEMISTRY_TRANSPORT_LIBRARY']
DRAFT=dict(dimensions=2,diffusion=.01,speed=.1,dt=.001,rate=1)
class Tests(unittest.TestCase):
 def request(self,action,args,token=1):return execute(LIB,dict(token=token,action=action,args=args))['session']
 def test_exact_readout_and_save_reopen(self):
  s=self.request('create',DRAFT);c=s['config'];direct=Spatial(LIB,c);direct.advance(37)
  s=self.request('advance',dict(session=s,steps=37));self.assertEqual(s['report'],direct.report())
  with tempfile.TemporaryDirectory() as t:
   p=str(Path(t)/'experiment.json');self.request('save',dict(session=s,path=p))
   opened=self.request('open',dict(path=p));self.assertEqual(opened,s)
   resumed=self.request('advance',dict(session=opened,steps=11));direct.advance(11);self.assertEqual(resumed['report'],direct.report())
   with self.assertRaises(FileExistsError):self.request('save',dict(session=s,path=p))
   self.assertEqual(json.loads(Path(p).read_text()),s)
 def test_invalid_replacement_and_tamper(self):
  s=self.request('create',DRAFT);before=copy.deepcopy(s)
  bad=copy.deepcopy(s);bad['report']['tick']=99
  with self.assertRaises(ValueError):restore(LIB,bad)
  self.assertEqual(s,before)
  bad=copy.deepcopy(DRAFT);bad['dt']=float('nan')
  with self.assertRaises(ValueError):self.request('create',bad)
  bad=copy.deepcopy(s);bad['config']['velocity_m_s'][0]=999
  with self.assertRaises(ValueError):restore(LIB,bad)
 def test_cancel_reset_stale_publication(self):
  s=self.request('create',DRAFT);late=execute(LIB,dict(token=4,action='advance',args=dict(session=s,steps=20)))
  reset=self.request('reset',dict(session=s));self.assertEqual(reset['report']['tick'],0)
  self.assertIs(accept(5,late,reset),reset)
  self.assertEqual(accept(4,late,s),late['session'])
  self.assertEqual(s['report']['tick'],0)
 def test_failed_batch_keeps_accepted(self):
  d=copy.deepcopy(DRAFT);d.update(dt=.02,diffusion=.1);s=self.request('create',d);before=copy.deepcopy(s)
  with self.assertRaises(ValueError):self.request('advance',dict(session=s,steps=10))
  self.assertEqual(s,before)
 def test_dimensions_and_atomic_publication(self):
  for dim in (1,2,3):
   d=copy.deepcopy(DRAFT);d['dimensions']=dim;s=self.request('create',d);self.assertEqual(len(s['report']['amount_mol']),16*(8 if dim>1 else 1)*(4 if dim>2 else 1))
  with tempfile.TemporaryDirectory() as t:
   p=Path(t)/'file.json';publish_new(p,{'old':1})
   with self.assertRaises(FileExistsError):publish_new(p,{'new':2})
   self.assertEqual(json.loads(p.read_text()),{'old':1});self.assertEqual(list(Path(t).iterdir()),[p])
if __name__=='__main__':unittest.main()
