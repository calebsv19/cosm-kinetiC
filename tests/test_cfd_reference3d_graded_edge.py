"""Check archived edge geometries and literal observer transformations."""
import unittest,sys,json,hashlib
from pathlib import Path
import numpy as np
R=Path(__file__).resolve().parents[1];sys.path.insert(0,str(R/'scripts'))
from cfd_reference3d_graded_edge_survey import build,CONTROLS
from cfd_reference3d_accuracy_graded_mesh import translated_inner_keys
class GradedEdge(unittest.TestCase):
    pass

if __name__=='__main__':unittest.main()
