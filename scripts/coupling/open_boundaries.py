"""Independent layout for ambient lateral reservoirs; preserves legacy indices."""
import math
from surface_sources.growth_fire_v1 import require
MODELS = ('open_xy_ground_open_top_projection3d_v1', 'open_xy_ground_box_open_top_projection3d_v1')

def lateral(config):
    return config['boundary_policy']['horizontal'] == 'ambient_open_xy'

def layout(config):
    x,y,z=config['grid']; plane=x*y
    faces=2*(plane+(y*z+x*z if lateral(config) else 0))
    velocities=3*x*y*z+plane+(y*z+x*z if lateral(config) else 0)
    return velocities,faces

def face(grid,axis,i,j,k,opened=False):
    x,y,z=grid;n=x*y*z
    if opened and axis==0 and i==x:return 3*n+x*y+k*y+j
    if opened and axis==1 and j==y:return 3*n+x*y+y*z+k*x+i
    return axis*n+(k*y+j)*x+i

def boundaries(config):
    x,y,z=config['grid']; p=x*y
    result=[('z_min',2,-1,0,p,[x,y]),('z_max',2,1,p,p,[x,y])]
    if lateral(config):
        result += [('x_min',0,-1,2*p,y*z,[y,z]),('x_max',0,1,2*p+y*z,y*z,[y,z]),
                   ('y_min',1,-1,2*p+2*y*z,x*z,[x,z]),('y_max',1,1,2*p+2*y*z+x*z,x*z,[x,z])]
    return result
