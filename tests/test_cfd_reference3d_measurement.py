#!/usr/bin/env python3
"""Independent analytic surface-stress and exact affine volume-clipping tests."""
import json
import tempfile
import sys
import unittest
from pathlib import Path
import numpy as np
from skfem import MeshTet,ElementTetP1,ElementTetP2,Basis

ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'scripts'))
from cfd_reference3d_pressure_projection import integrate_box
from cfd_reference3d_traction import traction


class ReferenceMeasurement(unittest.TestCase):
    def test_exact_affine_clipping(self):
        mesh=MeshTet.init_tensor(np.linspace(0,4,9),np.linspace(0,2,7),np.linspace(0,2,7))
        vertices=mesh.p.T;cells=mesh.t.T
        p=.03+.004*vertices[:,0]+.002*vertices[:,1]-.001*vertices[:,2]
        boxes=[([.13,.21,.37],[2.43,1.74,1.81]),([1.37,.5,.5],[1.5,1.5,1.5]),
               ([2.5,.5,.5],[2.5+1/24,1.5,1.5]),([1.5,.5,.5],[2.5,1.5,1.5])]
        for lower,upper in boxes:
            lower=np.array(lower);upper=np.array(upper);row=integrate_box(vertices,cells,p,lower,upper)
            volume=np.prod(upper-lower);center=(upper+lower)/2
            mean=.03+.004*center[0]+.002*center[1]-.001*center[2]
            self.assertLess(abs(row['volume_m3']/volume-1),1e-10)
            self.assertLess(abs(row['pressure_integral_pa_m3']/(volume*mean)-1),1e-10)

    def test_piecewise_affine_clipping(self):
        mesh=MeshTet.init_tensor(np.linspace(0,4,9),np.linspace(0,2,7),np.linspace(0,2,7))
        vertices=mesh.p.T;cells=mesh.t.T
        pressure=.03+.004*vertices[:,0]+.008*np.abs(vertices[:,0]-2)
        lower=np.array([.37,.23,.41]);upper=np.array([3.43,1.67,1.82])
        volume=np.prod(upper-lower)
        primitive=lambda x:.5*(x-2)*abs(x-2)
        mean=.03+.004*(upper[0]+lower[0])/2+.008*(primitive(upper[0])-primitive(lower[0]))/(upper[0]-lower[0])
        row=integrate_box(vertices,cells,pressure,lower,upper)
        self.assertLess(abs(row['pressure_integral_pa_m3']/(volume*mean)-1),1e-10)
        gauge=integrate_box(vertices,cells,pressure+3.2,lower,upper)
        self.assertLess(abs((gauge['pressure_integral_pa_m3']-row['pressure_integral_pa_m3'])/(3.2*volume)-1),1e-10)

    def test_exact_planar_traction(self):
        records=[]
        for length in (4,8):
            cx=length/2;lo=np.array([cx-.5,.5,.5]);hi=lo+1
            mesh=MeshTet.init_tensor(np.linspace(0,length,int(length*2)+1),np.linspace(0,2,5),np.linspace(0,2,5))
            centers=mesh.p[:,mesh.t].mean(axis=1)
            mesh=mesh.remove_elements(np.nonzero(np.all((centers>lo[:,None])&(centers<hi[:,None]),axis=0))[0])
            def body(x):
                return np.all((x>=lo[:,None]-1e-10)&(x<=hi[:,None]+1e-10),axis=0)&np.any(np.isclose(x,lo[:,None])|np.isclose(x,hi[:,None]),axis=0)
            mesh=mesh.with_boundaries({'body':body})
            ub,pb=Basis(mesh,ElementTetP2()),Basis(mesh,ElementTetP1())
            x=ub.doflocs;u=np.array([(x[1]-.5)*(x[1]-1.5),np.zeros(ub.N),np.zeros(ub.N)])
            x=pb.doflocs;p=.03+.004*x[0]+.002*x[1]-.001*x[2]
            for order in (2,4,8):
                row=traction(mesh,ub,pb,u,p,.1,lo,hi,order)
                self.assertLess(np.max(np.abs(np.array(row['pressure_force_n'])-[-.004,-.002,.001])),1e-12)
                self.assertLess(np.max(np.abs(np.array(row['raw_viscous_force_n'])-[.2,0,0])),1e-12)
                for face in row['faces']:
                    self.assertAlmostEqual(face['area_m2'],1,places=12)
                    a,side=face['axis'],face['side'];normal=np.zeros(3);normal[a]=1 if side==0 else -1
                    center=(lo+hi)/2;center[a]=(lo if side==0 else hi)[a]
                    exact_p=(.03+.004*center[0]+.002*center[1]-.001*center[2])*normal
                    gradient=np.zeros((3,3));gradient[0,1]=2*center[1]-2
                    exact_v=-.1*(gradient+gradient.T)@normal
                    self.assertLess(np.max(np.abs(np.array(face['pressure_force_n'])-exact_p)),1e-12)
                    self.assertLess(np.max(np.abs(np.array(face['raw_viscous_force_n'])-exact_v)),1e-12)
                    # Edge bands partition each independently integrated plane.
                    for name,total in [('pressure_force_n',face['pressure_force_n']),('raw_viscous_force_n',face['raw_viscous_force_n'])]:
                        self.assertLess(np.max(np.abs(np.sum([b[name] for b in face['edge_bands']],axis=0)-total)),1e-12)
                records.append({'length':length,'quadrature_order':order,'pressure_force_n':row['pressure_force_n'],'raw_viscous_force_n':row['raw_viscous_force_n']})
        with tempfile.TemporaryDirectory(prefix='physics-measurement-') as directory:
            path=Path(directory)/'measurement-known-answer.json'
            payload={'passed':True,'records':records,'scope':'exact P2 planar shear, affine pressure, analytic closed loads and independent affine clipped-volume controls; Y planes are no-slip'}
            path.write_text(json.dumps(payload,indent=2)+'\n')
            self.assertEqual(json.loads(path.read_text()),payload)

if __name__=='__main__':unittest.main()
