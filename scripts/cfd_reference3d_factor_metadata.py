"""Suspend deterministic unused FE metadata; restore exact original bits later."""
import weakref
import numpy as np
from skfem import Basis
from skfem.mapping.mapping_affine import MappingAffine
from cfd_reference3d_shared_factor import storage_sha

class FactorMetadata:
    def __init__(self,system):
        self.system=system;self.bases=(system.ub,system.pb);self.mapping=system.ub.mapping;self.restored=False
        if not isinstance(self.mapping,MappingAffine) or self.mapping is not system.pb.mapping or self.mapping is not system.mesh._mapping():raise ValueError('shared affine FE mapping required')
        self.mesh_sha256=storage_sha(system.mesh.p,system.mesh.t)
        self.dof_sha256=tuple(storage_sha(b.dofs.element_dofs) for b in self.bases)
        self.manifest={};self.references={}
        items=[(b,'doflocs','velocity_locations' if i==0 else 'pressure_locations') for i,b in enumerate(self.bases)]
        items.append((system,'trace_inverse','trace_inverse'))
        items.extend((self.mapping,key,key) for key in ('_A','_b','_detA','_invA') if hasattr(self.mapping,key))
        for owner,key,label in items:
            a=getattr(owner,key)
            if not isinstance(a,np.ndarray) or not a.flags.c_contiguous or not np.all(np.isfinite(a)):raise ValueError('finite contiguous FE metadata required')
            self.manifest[label]=dict(shape=a.shape,dtype=a.dtype.str,sha256=storage_sha(a),bytes=a.nbytes)
            self.references[label]=weakref.ref(a)
        for owner,key,label in items:delattr(owner,key)
        self.metadata=dict(detached_array_bytes=sum(r['bytes'] for r in self.manifest.values()),arrays=self.manifest,
            mesh_sha256=self.mesh_sha256,dof_sha256=self.dof_sha256,old_arrays_detached=False,restored_bitwise=False,
            scope='deterministic FE coordinates/trace inverse/affine mapping caches unused by exact assembled factor and mixed solve; original mesh/DOFs/operators/load remain live')
    def check_detached(self):
        if self.restored:raise ValueError('metadata already restored')
        if any(ref() is not None for ref in self.references.values()):raise ValueError('detached FE array still has an owner')
        if storage_sha(self.system.mesh.p,self.system.mesh.t)!=self.mesh_sha256 or tuple(storage_sha(b.dofs.element_dofs) for b in self.bases)!=self.dof_sha256:raise ValueError('live mesh/DOF metadata changed')
        self.metadata['old_arrays_detached']=True
    def restore(self):
        self.check_detached()
        for key in ('A','b','detA','invA'):
            if '_'+key in self.manifest:getattr(self.mapping,key)
        for b in self.bases:
            restored=Basis(b.mesh,b.elem,mapping=b.mapping,dofs=b.dofs,quadrature=b.quadrature,elements=np.array([0]))
            b.doflocs=restored.doflocs
        inverse=np.full(self.system.ub.N,-1,dtype=np.int32);inverse[self.system.trace]=np.arange(self.system.nt)
        self.system.trace_inverse=inverse
        values=dict(velocity_locations=self.bases[0].doflocs,pressure_locations=self.bases[1].doflocs,trace_inverse=inverse)
        values.update({key:getattr(self.mapping,key) for key in ('_A','_b','_detA','_invA') if key in self.manifest})
        for key,a in values.items():
            row=self.manifest[key]
            if a.shape!=tuple(row['shape']) or a.dtype.str!=row['dtype'] or storage_sha(a)!=row['sha256']:raise ValueError('FE metadata restoration changed original bits: '+key)
        self.restored=True;self.metadata['restored_bitwise']=True
