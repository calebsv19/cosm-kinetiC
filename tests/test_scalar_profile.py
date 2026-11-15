"""Opt-in profile identity, refusal, and absence-preserving defaults."""
import copy,json,os,sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from coupling.scalar_profile import FIELD,PROFILE,validate,state,fields
from coupling.accuracy_gust_cases import configuration
from coupling.fluid_adapter import validate as fluid_validate
import open_atmosphere
class ScalarProfileTests(unittest.TestCase):
    def test_absence_has_no_receipt(self):
        validate({});state({},{});fields({}, {}, {})
        for method,args in [(state,({}, {FIELD:PROFILE})),(fields,({}, {FIELD:PROFILE}, {}))]:
            with self.assertRaises(ValueError):method(*args)
    def test_closed_profile_and_separate_mode(self):
        for value in (None,True,{},'mc','mc_ssprk2/v2'):
            with self.assertRaises(ValueError):validate({FIELD:value,'transport_scheme':'muscl_minmod'})
        for scheme in ('upwind','muscl_minmod',None):
            validate({FIELD:PROFILE,'transport_scheme':scheme})
        validate({FIELD:PROFILE,'transport_scheme':'upwind','diagnostic_transport':'fixed_mac_velocity/v1'})
        with self.assertRaises(ValueError):validate({FIELD:PROFILE,'diagnostic_transport':'unknown'})
    def test_native_profile_and_field_state_identity(self):
        c={FIELD:PROFILE,'transport_scheme':'muscl_minmod'};state(c,{FIELD:PROFILE});fields(c,{FIELD:PROFILE},{FIELD:PROFILE})
        for data in ({},{FIELD:'wrong'}):
            with self.assertRaises(ValueError):state(c,data)
        with self.assertRaises(ValueError):fields(c,{}, {FIELD:PROFILE})
    def test_configuration_digest_binds_opt_in_without_mutating_default(self):
        w=json.loads(Path(os.environ['REPLAY_WORKERS']).read_text());workers=(w['growth_root'],*[w['binaries'][k]['path'] for k in ('fire','fluid','clock')]);c=configuration(workers,'gust64')['fluid'];original=copy.deepcopy(c);fluid_validate(c)
        candidate=copy.deepcopy(c);candidate[FIELD]=PROFILE;fluid_validate(candidate)
        self.assertNotEqual(open_atmosphere.configuration(c),open_atmosphere.configuration(candidate));self.assertEqual(c,original)
        bad=copy.deepcopy(candidate);bad['transport_scheme']='unknown'
        with self.assertRaises(ValueError):fluid_validate(bad)
    def test_string_budget_root_is_normalized_before_native_start(self):
        import tempfile
        from unittest.mock import patch
        from coupling import scalar_candidate_study as study
        with tempfile.TemporaryDirectory(dir=os.environ.get('ACCURACY_TEST_ROOT')) as tmp:
            base=Path(tmp).resolve();baseline=base/'baseline';baseline.mkdir();(baseline/'config.json').write_text(json.dumps({'fluid':{},'fluid_worker':'old'}))
            def allocation(root):
                self.assertIsInstance(root,Path);self.assertEqual(root,base);return 0
            with patch.object(study,'configuration',return_value={'fluid':{},'fluid_worker':'new'}),patch.object(study,'allocation',side_effect=allocation),patch.object(study,'ReplayRun',side_effect=RuntimeError('stop-before-native')) as native:
                with self.assertRaisesRegex(RuntimeError,'stop-before-native'):study.run(base/'run',(),{},baseline,str(base))
                self.assertEqual(native.call_count,1)

if __name__=='__main__':unittest.main()
