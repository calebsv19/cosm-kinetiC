"""Minimal structurally valid exporter-contract fixture; no numerical qualification."""
import ctypes
import json
class Header(ctypes.Structure):
    _fields_=[('magic',ctypes.c_uint32),('version',ctypes.c_uint32),('w',ctypes.c_uint32),('h',ctypes.c_uint32),('d',ctypes.c_uint32),('time',ctypes.c_double),('frame',ctypes.c_uint64),('dt',ctypes.c_double),('ox',ctypes.c_float),('oy',ctypes.c_float),('oz',ctypes.c_float),('voxel',ctypes.c_float),('upx',ctypes.c_float),('upy',ctypes.c_float),('upz',ctypes.c_float),('crc',ctypes.c_uint32),('reserved',ctypes.c_uint32*3)]
def frame_bytes(index=0):
    h=Header();h.magic=0x56463344;h.version=1;h.w=h.h=h.d=1;h.frame=index;h.dt=1/60;h.voxel=h.upz=1;h.crc=(2166136261*16777619)&0xffffffff
    return bytes(h)+bytes(21)
def write_cache(source,indices=(0,)):
    source.mkdir(parents=True,exist_ok=True)
    for index in indices:(source/f'frame_{index:06d}.vf3d').write_bytes(frame_bytes(index))
    (source/'manifest.json').write_text(json.dumps({'manifest_version':2,'frame_contract':'vf3d','space_mode':'3d','grid_w':1,'grid_h':1,'grid_d':1,'frames':[{'frame_index':i,'path':f'frame_{i:06d}.vf3d','frame_contract':'vf3d'} for i in indices]}))
    (source/'scene_bundle.json').write_text(json.dumps({'bundle_type':'physics_scene_bundle_v1','bundle_version':1,'fluid_source':{'kind':'manifest','path':'manifest.json','contract':'vf3d'}}))
