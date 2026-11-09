"""RC3 trusted local native-workspace client. One private request/result per job."""
import argparse,copy,json,math,os,tempfile
from pathlib import Path
from chemistry_reactor import fixture,digest
from chemistry_transport import Spatial

def preset(draft):
 if set(draft)!={'dimensions','diffusion','speed','dt','rate'}:raise ValueError('draft fields')
 dim=draft['dimensions']
 if type(dim)!=int or not 1<=dim<=3:raise ValueError('dimensions')
 for k,lo,hi in [('diffusion',0,.1),('speed',-.5,.5),('dt',.00001,.02),('rate',0,5)]:
  if not math.isfinite(draft[k]) or not lo<=draft[k]<=hi:raise ValueError('draft range: '+k)
 n=[16,8 if dim>=2 else 1,4 if dim==3 else 1];h=[1/x for x in n];v=math.prod(h);chem=fixture()
 for r in chem['reactions']:r['k']*=draft['rate']
 rows=[]
 for i in range(math.prod(n)):
  x=i%n[0];rows.append([v*(1.5 if x<8 else .2),v*.2,v*(.2 if x<8 else 1.2),v*.1,v*.05,0,0])
 return dict(schema='rc2/v1',chemistry=chem,n=n,boundary=[0]*3,spacing_m=h,velocity_m_s=[draft['speed'],0,0],diffusion_m2_s=draft['diffusion'],dt_s=draft['dt'],reacting=True,initial_mol=rows)
def make_model(library,config):
 if config.get("schema")=="rc7-normal-film-capture/v1":
  from chemistry_normal_film_capture import NormalFilmCapture
  return NormalFilmCapture(library,config)
 if config.get("schema")=="rc7-film-capture/v1":
  from chemistry_film_capture import FilmCapture
  return FilmCapture(library,config)
 if config.get("schema")=="rc7-wall-film/v1":
  from chemistry_wall_film import WallFilm
  return WallFilm(library,config)
 if config.get("schema")=="rc7-dynamic-wetting/v1":
  from chemistry_dynamic_wetting import DynamicWetting
  return DynamicWetting(library,config)
 if config.get("schema")=="rc7-wetted-wall/v1":
  from chemistry_wetted_wall import WettedWall
  return WettedWall(library,config)
 if config.get("schema")=="rc7-droplet-deposition/v1":
  from chemistry_droplet_deposition import DropletDeposition
  return DropletDeposition(library,config)
 if config.get("schema")=="rc7-droplet-carrier/v1":
  from chemistry_droplet_carrier import DropletCarrier
  return DropletCarrier(library,config)
 if config.get("schema")=="rc7-settling-droplet/v1":
  from chemistry_settling_droplet import SettlingDroplet
  return SettlingDroplet(library,config)
 if config.get("schema")=="rc7-humid-droplet/v1":
  from chemistry_humid_droplet import HumidDroplet
  return HumidDroplet(library,config)
 if config.get("schema")=="rc7-spherical-droplet/v1":
  from chemistry_spherical_droplet import SphericalDroplet
  return SphericalDroplet(library,config)
 if config.get("schema")=="rc7-stefan-interface/v1":
  from chemistry_stefan_interface import StefanInterface
  return StefanInterface(library,config)
 if config.get("schema")=="rc7-capillary-interface/v1":
  from chemistry_capillary_interface import CapillaryInterface
  return CapillaryInterface(config)
 if config.get("schema")=="rc7-water-surface-pressure/v1":
  from chemistry_water_surface_pressure import WaterSurfacePressure
  return WaterSurfacePressure(library,config)
 if config.get("schema")=="rc7-water-momentum/v1":
  from chemistry_water_momentum import WaterMomentum
  return WaterMomentum(library,config)
 if config.get("schema")=="rc7-water-interface-flow/v1":
  from chemistry_water_interface_flow import FlowingWaterInterface
  return FlowingWaterInterface(library,config)
 if config.get("schema")=="rc7-water-interface/v1":
  from chemistry_water_interface import WaterInterface
  return WaterInterface(library,config)
 if config.get("schema")=="rc7-water/v1":
  from chemistry_water import WaterChamber
  return WaterChamber(library,config)
 if config.get("schema")=="rc7/v1":
  from chemistry_phase import Partition
  return Partition(library,config)
 if config.get("schema")=="rc6/v1":
  from chemistry_aqueous import Aqueous
  return Aqueous(library,config)
 if config.get("schema")=="rc5/v1":
  from chemistry_thermal import ThermalCoupled
  return ThermalCoupled(library,config)
 if config.get("schema")=="rc4/v1":
  from chemistry_flow import Coupled
  return Coupled(library,config)
 return Spatial(library,config)
def packet(model):
 return dict(schema='rc3-session/v1',config=model.config,checkpoint=model.checkpoint(),report=model.report())
def restore(library,session):
 if set(session)!={'schema','config','checkpoint','report'} or session['schema']!='rc3-session/v1':raise ValueError('session schema')
 m=make_model(library,session['config']);m.restore(session['checkpoint'])
 if session['report']!=m.report():raise ValueError('saved readout mismatch')
 return m

def publish_new(path,value):
 path=Path(path);data=json.dumps(value,allow_nan=False).encode();fd,tmp=tempfile.mkstemp(prefix='.chemistry-',dir=path.parent)
 try:
  with os.fdopen(fd,'wb') as f:f.write(data);f.flush();os.fsync(f.fileno())
  os.link(tmp,path) # complete-file publication, refuses existing target
 finally:os.unlink(tmp)
def execute(library,request):
 if set(request)!={'token','action','args'} or type(request['token'])!=int or request['token']<0:raise ValueError('request')
 action,args=request['action'],request['args']
 if action=='create':m=Spatial(library,preset(args))
 elif action=='open':
  value=json.loads(Path(args['path']).read_text())
  m=restore(library,value) if value.get('schema')=='rc3-session/v1' else make_model(library,value)
 else:
  m=restore(library,args['session'])
  if action=='advance':
   steps=args['steps']
   if type(steps)!=int or not 0<=steps<=1000:raise ValueError('batch steps')
   m.advance(steps)
  elif action=='reset':m=make_model(library,m.config)
  elif action=='save':publish_new(args['path'],packet(m))
  else:raise ValueError('action')
 return dict(token=request['token'],session=packet(m))
def accept(token,response,current):
 """Same publication guard used by the native host: rejected results keep state."""
 return response['session'] if response.get('token')==token and 'session' in response else current

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--library',required=True);p.add_argument('--input',required=True);p.add_argument('--output',required=True);a=p.parse_args()
 request=json.loads(Path(a.input).read_text())
 try:response=execute(a.library,request)
 except (ValueError,KeyError,TypeError,OSError,OverflowError) as e:response=dict(token=request.get('token'),error=str(e))
 publish_new(a.output,response)
if __name__=='__main__':main()
