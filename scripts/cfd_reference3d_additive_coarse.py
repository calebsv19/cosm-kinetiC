"""Fixed additive SPD sum of exact cubic Galerkin correction and local sweep."""
import numpy as np
from cfd_reference3d_cubic_coarse import BalancedCubicVelocity,macro_cubic_interpolation


class AdditiveCoarseVelocity(BalancedCubicVelocity):
    def __init__(self,velocity,Z,library,cycles=1,ordering='metis',grouping='u_vw',interpolation_metadata=None,local_kind='two_block'):
        super().__init__(velocity,Z,library,cycles,ordering,grouping,interpolation_metadata,local_kind)
        self.metadata.update(kind='additive_coarse_velocity',
            scope='fixed SPD sum of exact coarse Galerkin inverse and exact positive principal coupled sweep; no balanced coarse-action claim; physical mixed operator/full FE authority unchanged')

    def solve(self,x):
        if self.closed:raise ValueError('additive inverse is closed')
        rhs=np.asarray(x,dtype=float)
        if rhs.shape!=self.velocity.shape[:1] or not np.all(np.isfinite(rhs)):raise ValueError('invalid additive RHS')
        z=self.local.solve(rhs)+self.coarse_action(rhs)
        if not np.all(np.isfinite(z)):raise ValueError('nonfinite additive inverse')
        return z
