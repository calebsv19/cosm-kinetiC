"""PhysicsSim-specific qualification geometry and reference quantities (SI).
References are targets, never injected forces or measured drag coefficients.
"""
import hashlib
import json
import math

FLUIDS = {
    'air_300k': {'density_kg_m3': 1.1612, 'dynamic_viscosity_pa_s': 1.85e-5,
                'temperature_k': 300, 'pressure_pa': 100000,
                'note': 'Rounded dry-air reference; density from ideal gas R=287.05 J/(kg K).',
                'source': 'https://tsapps.nist.gov/publication/get_pdf.cfm?pub_id=926439'},
    'water_293k': {'density_kg_m3': 998.2, 'dynamic_viscosity_pa_s': .001002,
                  'temperature_k': 293.15, 'pressure_pa': 101325,
                  'note': 'Rounded liquid-water reference; single-phase incompressible test, no free surface.',
                  'source': 'https://webbook.nist.gov/chemistry/fluid/'},
    'viscous_test': {'density_kg_m3': 1000., 'dynamic_viscosity_pa_s': 1.,
                    'note': 'Synthetic Newtonian calibration fluid, not a named material.'}}


def mesh(shape):
    vertices, faces = [], []
    if shape == 'cube':
        vertices = [(x,y,z) for x in (-.5,.5) for y in (-.5,.5) for z in (-.5,.5)]
        for q in [(0,1,3,2),(4,6,7,5),(0,4,5,1),(2,3,7,6),(0,2,6,4),(1,5,7,3)]:
            faces.extend([(q[0],q[1],q[2]),(q[0],q[2],q[3])])
    elif shape == 'sphere':
        vertices = [(-.5,0,0),(.5,0,0)]
        for j in range(1,16):
            t=math.pi*j/16
            vertices.extend([(.5*math.cos(t),.5*math.sin(t)*math.cos(2*math.pi*i/32),
                              .5*math.sin(t)*math.sin(2*math.pi*i/32)) for i in range(32)])
        for i in range(32):
            k=(i+1)%32
            faces.extend([(1,2+i,2+k),(0,2+14*32+k,2+14*32+i)])
            for j in range(14):
                a=2+j*32+i;b=2+j*32+k;c=a+32;d=b+32
                faces.extend([(a,c,d),(a,d,b)])
    elif shape == 'cone':
        # Length=diameter=1, point faces upstream (-X); base normal +X.
        vertices=[(-.5,0,0),(.5,0,0)]+[(.5,.5*math.cos(2*math.pi*i/32),
                                              .5*math.sin(2*math.pi*i/32)) for i in range(32)]
        for i in range(32): faces.extend([(0,2+i,2+(i+1)%32),(1,2+(i+1)%32,2+i)])
    else: raise ValueError('unknown qualification shape')
    # Orient consistently outwards using an interior point (all shapes convex).
    center=(.25,0,0) if shape=='cone' else (0,0,0)
    oriented=[]
    for face in faces:
        a,b,c=[vertices[k] for k in face]
        u=[b[k]-a[k] for k in range(3)];v=[c[k]-a[k] for k in range(3)]
        n=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
        oriented.append(face if sum(n[k]*(a[k]-center[k]) for k in range(3))>0 else (face[0],face[2],face[1]))
    return vertices,oriented


def write_shape(root, shape):
    vertices,faces=mesh(shape)
    folder=root/'assets';folder.mkdir(exist_ok=True)
    stl=['solid '+shape]
    for face in faces:
        a,b,c=[vertices[i] for i in face]
        u=[b[k]-a[k] for k in range(3)];v=[c[k]-a[k] for k in range(3)]
        n=[u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]]
        length=math.sqrt(sum(x*x for x in n));n=[x/length for x in n]
        stl+=['facet normal '+' '.join(format(x,'.17g') for x in n),'outer loop']
        stl+=['vertex '+' '.join(format(x,'.17g') for x in p) for p in (a,b,c)]
        stl+=['endloop','endfacet']
    stl+=['endsolid '+shape]
    (folder/'shape.stl').write_text('\n'.join(stl)+'\n')
    doc={'schema_family':'codework_geometry','schema_variant':'mesh_asset_runtime_v1','schema_version':1,
         'asset_id':'qualification_shape','source_asset_id':'qualification_shape','asset_type':'solid_mesh',
         'compile_meta':{'profile':'runtime_default'},
         'local_bounds':{'min':dict.fromkeys('xyz',-.5),'max':dict.fromkeys('xyz',.5)},
         'mesh':{'vertex_count':len(vertices),'triangle_count':len(faces),
                 'vertices':[dict(zip('xyz',p)) for p in vertices],
                 'triangles':[dict(zip('abc',f),surface_group_id='shell') for f in faces]},
         'surface_groups':[{'group_id':'shell','triangle_span':{'start':0,'count':len(faces)}}],
         'topology_flags':{'closed_volume':True,'manifold_expected':True},'extensions':{}}
    (folder/'shape.runtime.json').write_text(json.dumps(doc,sort_keys=True)+'\n')
    return {str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(folder.iterdir())}


def reference(shape, diameter, speed, density, viscosity):
    re=density*speed*diameter/viscosity
    area=0 if shape=='empty' else (diameter**2 if shape=='cube' else math.pi*diameter**2/4)
    result={'reynolds_number':re,'characteristic_length_m':diameter,'reference_area_m2':area,
            'dynamic_pressure_pa':.5*density*speed**2,'kinematic_viscosity_m2_s':viscosity/density,
            'measured_drag_n':None,'measured_drag_coefficient':None,
            'drag_status':'unavailable: no validated surface stress integration',
            'target_drag_coefficient':None,'target_drag_n':None,
            'reference_scope':'No universal Cd: Reynolds number, orientation, confinement and surface matter.',
            'source':'https://www1.grc.nasa.gov/beginners-guide-to-aeronautics/shape-effects-on-drag/'}
    if shape=='sphere' and re <= .1:
        result.update(target_drag_coefficient=24/re,target_drag_n=3*math.pi*viscosity*speed*diameter,
                      reference_scope='Stokes asymptotic target, Re <= 0.1; isolated smooth sphere, unbounded steady creeping flow. Finite tunnel results are not equivalent.',
                      source='https://doi.org/10.2514/1.J060153')
    return result
