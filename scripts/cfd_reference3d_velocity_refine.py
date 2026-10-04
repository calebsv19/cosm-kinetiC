"""One fixed original-Double residual correction; preconditioner action only."""
import numpy as np

def reserve(nv):
    if type(nv) is not int or nv<1:raise ValueError('invalid velocity refinement reservation')
    return 64*nv

class RefinedVelocity:
    def __init__(self,operator,factor):
        if len(operator.shape)!=2 or operator.shape[0]!=operator.shape[1] or operator.shape[0]<1 or operator.dtype!=np.dtype('float64') or getattr(factor,'n',None)!=operator.shape[0] or not callable(getattr(factor,'solve',None)):raise ValueError('complete Double velocity action and matching live solve required')
        self.operator=operator;self.factor=factor;self.n=operator.shape[0]
        self.metadata=dict(kind='one_fixed_double_residual_velocity_pc_correction',correction_steps=1,extra_reservation_bytes=reserve(self.n),physical_action_dtype='float64',inner_solve='existing approximate Float32 coupled factor in reference engine',calls=0,coarse_observation_limit=10,coarse_accuracy_observations=[],scope='preconditioner only; one fixed correction, not an exact linear SPD Float inverse; full flexible/reconstructed FE residual remains authority')
    def solve(self,x):
        rhs=np.asarray(x,dtype=np.float64)
        if rhs.shape!=(self.n,) or not np.all(np.isfinite(rhs)):raise ValueError('invalid refined velocity RHS')
        first=np.asarray(self.factor.solve(rhs))
        if first.shape!=(self.n,) or first.dtype!=np.dtype('float64') or not np.all(np.isfinite(first)):raise ValueError('invalid first velocity PC output')
        residual=rhs-self.operator@first
        if residual.shape!=(self.n,) or not np.all(np.isfinite(residual)):raise ValueError('invalid physical velocity residual')
        correction=np.asarray(self.factor.solve(residual))
        if correction.shape!=(self.n,) or correction.dtype!=np.dtype('float64') or not np.all(np.isfinite(correction)):raise ValueError('invalid velocity PC correction')
        result=first+correction
        if not np.all(np.isfinite(result)):raise ValueError('nonfinite corrected velocity action')
        calls=self.metadata['calls']+1
        if calls<=10:
            final=rhs-self.operator@result
            if not np.all(np.isfinite(final)):raise ValueError('invalid corrected velocity diagnostic')
            scale=max(float(np.linalg.norm(rhs)),np.finfo(float).tiny)
            self.metadata['coarse_accuracy_observations'].append(dict(call=calls,rhs_l2=scale,first_true_velocity_relative_residual=float(np.linalg.norm(residual)/scale),corrected_true_velocity_relative_residual=float(np.linalg.norm(final)/scale),authority='original physical Double velocity action; coarse setup observation only, not full FE acceptance'))
        self.metadata['calls']=calls
        return result
