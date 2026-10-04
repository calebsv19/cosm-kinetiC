"""Exact deterministic FE catalogs detached only during assembled solve lifetime."""
import weakref
import numpy as np
from skfem.assembly.dofs import Dofs
from cfd_reference3d_factor_metadata import FactorMetadata
from cfd_reference3d_preconditioner import array_sha
from cfd_reference3d_shared_factor import storage_sha

class CatalogMetadata(FactorMetadata):
    def __init__(self,system):
        super().__init__(system);self.catalog_suspended=True;self.catalog_manifest=[];self.catalog_refs=[]
        expected={'topo','element','nodal_dofs','edge_dofs','facet_dofs','interior_dofs','element_dofs','N'}
        for b in self.bases:
            catalog=b.dofs
            if type(catalog) is not Dofs or set(catalog.__dict__)!=expected or catalog.topo is not system.mesh or catalog.element is not b.elem:raise ValueError('original zero-offset FE catalog required')
            arrays={key:dict(shape=a.shape,dtype=a.dtype.str,sha256=array_sha(a),bytes=a.nbytes) for key,a in catalog.__dict__.items() if isinstance(a,np.ndarray)}
            self.catalog_manifest.append(dict(N=int(catalog.N),N_dtype=np.asarray(catalog.N).dtype.str,arrays=arrays))
            self.catalog_refs.append(weakref.ref(catalog));self.catalog_refs.extend(weakref.ref(getattr(catalog,key)) for key in arrays)
        for b in self.bases:del b.dofs
        self.metadata.update(catalogs=self.catalog_manifest,catalog_array_bytes=sum(a['bytes'] for c in self.catalog_manifest for a in c['arrays'].values()),
            old_catalog_owners_detached=False,catalogs_restored_bitwise=False)
        self.metadata['scope']+='; all original full FE numbering/catalog tables restored bitwise before field reconstruction'
    def check_detached(self):
        if self.restored:raise ValueError('metadata already restored')
        if any(ref() is not None for ref in self.references.values()) or any(ref() is not None for ref in self.catalog_refs):raise ValueError('detached FE array/catalog still has an owner')
        if storage_sha(self.system.mesh.p,self.system.mesh.t)!=self.mesh_sha256:raise ValueError('live mesh/DOF metadata changed')
        if not self.catalog_suspended:super().check_detached()
        self.metadata['old_arrays_detached']=True;self.metadata['old_catalog_owners_detached']=True
    def restore(self):
        self.check_detached()
        for b,row in zip(self.bases,self.catalog_manifest):
            catalog=Dofs(b.mesh,b.elem)
            if int(catalog.N)!=row['N'] or np.asarray(catalog.N).dtype.str!=row['N_dtype']:raise ValueError('FE catalog dimension changed')
            for key,expected in row['arrays'].items():
                a=getattr(catalog,key)
                if a.shape!=tuple(expected['shape']) or a.dtype.str!=expected['dtype'] or array_sha(a)!=expected['sha256']:raise ValueError('FE catalog restoration changed original bits: '+key)
            b.dofs=catalog
        self.catalog_suspended=False;super().restore();self.metadata['catalogs_restored_bitwise']=True
