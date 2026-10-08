"""Verify selected worker paths cannot silently fall back to another build."""
import os
from pathlib import Path
import sys
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from passive_atmosphere import atmosphere_worker_path

ROUTES={'passive':('PHYSICS_SIM_PASSIVE_WORKER','passive-atmosphere/physics_sim_passive_worker'),
        'evolving':('PHYSICS_SIM_ATMOSPHERE_WORKER','evolving-atmosphere/physics_sim_atmosphere_worker'),
        'open':('PHYSICS_SIM_OPEN_ATMOSPHERE_WORKER','open-atmosphere/physics_sim_open_atmosphere_worker')}

class WorkerSelection(unittest.TestCase):
    def test_default_and_exact_selection_without_existence_fallback(self):
        with patch.dict(os.environ,{},clear=True):
            for role,(variable,relative) in ROUTES.items():
                self.assertEqual(atmosphere_worker_path(role),ROOT/'build'/relative)
                selected=ROOT/'build/nonexistent-selected-profile'/relative
                with patch.dict(os.environ,{variable:str(selected)}):
                    self.assertEqual(atmosphere_worker_path(role),selected)
    def test_invalid_selected_paths_hold(self):
        for role,(variable,_) in ROUTES.items():
            for value in ('','relative/worker'):
                with self.subTest(role=role,value=value),patch.dict(os.environ,{variable:value}):
                    with self.assertRaisesRegex(ValueError,'absolute path'):
                        atmosphere_worker_path(role)
    def test_families_have_independent_selection(self):
        with patch.dict(os.environ,{v:str(ROOT/'build'/role/'worker') for role,(v,_) in ROUTES.items()},clear=True):
            for role in ROUTES:self.assertEqual(atmosphere_worker_path(role),ROOT/'build'/role/'worker')

if __name__=='__main__':unittest.main()
