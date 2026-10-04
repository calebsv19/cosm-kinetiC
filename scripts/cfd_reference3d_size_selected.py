"""Use lossless coefficient sharing only where declared joint saving is useful."""
import numpy as np
from cfd_reference3d_vector_storage import VectorTriangle
from cfd_reference3d_encoded_operator import EncodedTriangle
THRESHOLD_BYTES=32*2**20

def decision(count,mode='auto'):
    if type(count) is not int or not 1<=count<=50000000 or mode not in ('auto','legacy'):raise ValueError('invalid coefficient selection request')
    potential=4*count
    return dict(mode=mode,coefficient_count=count,predicted_joint_saved_bytes=potential,threshold_bytes=THRESHOLD_BYTES,encoded_selected=mode=='auto' and potential>=THRESHOLD_BYTES,scope='reference coefficient-storage policy only; complete full FE and resource gates remain authority')

def select(triangle,library,mode='auto',check=None):
    if not isinstance(triangle,VectorTriangle) or not triangle.input_unchanged() or triangle.values.dtype!=np.dtype('float64') or not triangle.values.flags.c_contiguous:raise ValueError('unchanged original complete Double triangle required')
    choice=decision(len(triangle.values),mode)
    return (EncodedTriangle(triangle,library,check) if choice['encoded_selected'] else triangle),choice
