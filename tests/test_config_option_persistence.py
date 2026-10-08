"""Load/save/load keeps all 52 configuration members in the current file contract."""
import json
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
NUMBERS='window_w window_h grid_w grid_h grid_d sim_mode space_mode tunnel_inflow_speed tunnel_inflow_density tunnel_viscosity_scale water_level min_dt max_dt physics_substeps physics_fixed_dt max_physics_steps_per_frame command_batch_limit density_diffusion velocity_damping density_decay fluid_buoyancy_force fluid_solver_iterations fluid_3d_solver_region_cell_budget fluid_3d_max_velocity_displacement_cells stroke_sample_rate stroke_spacing emitter_density_multiplier emitter_velocity_multiplier emitter_sink_multiplier enable_render_blur render_black_level text_zoom_step collider_max_loops collider_max_loop_vertices collider_max_parts collider_max_part_vertices collider_simplify_epsilon collider_raster_padding physics_broadphase_enabled physics_broadphase_cell_size collider_debug_logs headless_enabled headless_frame_count headless_custom_slot headless_quality_index headless_skip_present save_volume_frames save_render_frames'.split()
STRINGS='input_root atmospheric_warm_start_path retained_runtime_scene_path headless_output_dir'.split()
VALID={
 'window':{'width':913,'height':719},'grid':{'width':37,'height':41,'depth':11},
 'simulation':{'mode':4,'space_mode':1,'tunnel_inflow_speed':0.123456789,'tunnel_inflow_density':0.25,'tunnel_viscosity_scale':1.75,'water_level':0.375},
 'timing':{'min_dt':0.0012345678901234567,'max_dt':0.051234567890123456,'substeps':7,'fixed_dt':0.011234567890123456,'max_steps_per_frame':13},
 'commands':{'max_per_frame':117},
 'fluid':{'diffusion':1.23456789e-12,'viscosity':2.3456789e-13,'density_decay':0.123456789,'buoyancy':0.314159265,'solver_iterations':29,'solver_region_cell_budget':345678,'max_velocity_displacement_cells':0.876543219},
 'input':{'stroke_sample_rate':137.12345678901235,'stroke_spacing':0.123456789},
 'emitters':{'density_multiplier':1.23456789,'velocity_multiplier':2.34567891,'sink_multiplier':0.456789123},
 'render':{'blur_enabled':False,'black_level':117},'ui':{'text_zoom_step':2},
 'paths':{'input_root':'input/"quoted"\\source','atmospheric_warm_start_path':'warm/雪.pack','retained_runtime_scene_path':'scene/with\ncontrol.json'},
 'collider':{'max_loops':7,'max_loop_vertices':71,'max_parts':11,'max_part_vertices':39,'simplify_epsilon':1.23456789e-8,'raster_padding':0.314159265},
 'broadphase':{'enabled':False,'cell_size':2.71828182},'debug':{'collider_logs':True},
 'headless':{'enabled':True,'frame_count':117,'custom_slot_index':3,'quality_index':2,'skip_present':False,'output_dir':'out/"snapshot"'},
 'exports':{'save_volume_frames':True,'save_render_frames':True}}
class OptionPersistence(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert len(NUMBERS)+len(STRINGS)==52
        cls.temp=tempfile.TemporaryDirectory();cls.base=Path(cls.temp.name);source=cls.base/'probe.c'
        checks='\n'.join('if(first.'+key+'!=second.'+key+'){fprintf(stderr,"changed '+key+'\\n");return 4;}' for key in NUMBERS)
        checks+='\n'+'\n'.join('if(strcmp(first.'+key+',second.'+key+')){fprintf(stderr,"changed '+key+'\\n");return 4;}' for key in STRINGS)
        source.write_text('#include "config/config_loader.h"\n#include <stdio.h>\n#include <string.h>\nint main(int argc,char**argv){if(argc<2)return 9;AppConfig first={0},second={0};ConfigLoadOptions opts={.path=argv[1],.allow_missing=false};if(!config_loader_load(&first,&opts))return 2;if(!config_loader_save(&first,argv[1]))return 3;if(!config_loader_load(&second,&opts))return 2;'+checks+'\nreturn 0;}\n')
        sources=['src/config/config_loader.c','src/app/physics_sim_job_json.c','src/app/physics_sim_persistence.c','src/app/physics_sim_headless_output.c','src/app/app_config.c','src/app/data_paths.c']
        flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','json-c'],text=True));cls.binary=cls.base/'probe'
        cls.command=['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),'-I'+str(ROOT/'src'),str(source),*[str(ROOT/p) for p in sources],*flags,'-lm','-o',str(cls.binary)]
        subprocess.run(cls.command,capture_output=True,check=True)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def setUp(self):
        self.t=tempfile.TemporaryDirectory();self.addCleanup(self.t.cleanup);self.base=Path(self.t.name);self.path=self.base/'config.json'
    def invoke(self,value):
        self.path.write_text(json.dumps(value));return subprocess.run([str(self.binary),str(self.path)],capture_output=True,text=True,timeout=5)
    def test_all_52_nondefault_options_roundtrip_without_value_loss(self):
        result=self.invoke(VALID);self.assertEqual(result.returncode,0,result.stderr)
        saved=json.loads(self.path.read_text())
        for section in ('timing','input','emitters','exports'):
            for key in VALID[section]:self.assertIn(key,saved[section])
        for key in ('fixed_dt','max_steps_per_frame'):self.assertEqual(saved['timing'][key],VALID['timing'][key])
        self.assertEqual(saved['input']['stroke_sample_rate'],VALID['input']['stroke_sample_rate'])
    def test_small_nonzero_float_and_double_values_survive(self):
        value={'fluid':{'diffusion':1e-20,'viscosity':1e-15},'timing':{'min_dt':1e-12,'max_dt':1e-10,'fixed_dt':1e-11},'input':{'stroke_sample_rate':1e-10,'stroke_spacing':1e-15}}
        result=self.invoke(value);self.assertEqual(result.returncode,0,result.stderr)
        saved=json.loads(self.path.read_text());self.assertGreater(saved['fluid']['diffusion'],0);self.assertGreater(saved['input']['stroke_spacing'],0)
    def test_optional_defaults_and_legacy_aliases_survive_canonical_saving(self):
        result=self.invoke({'fluid':{'density_diffusion':1e-12,'velocity_damping':2e-13,'decay':0.125,'buoyancy_force':0.25,'iterations':27},'simulation':{'spaceMode':1},'collider':{'collider_logs':1}})
        self.assertEqual(result.returncode,0,result.stderr)
        saved=json.loads(self.path.read_text());self.assertIn('diffusion',saved['fluid']);self.assertNotIn('density_diffusion',saved['fluid']);self.assertTrue(saved['debug']['collider_logs'])
    def test_exports_both_false_and_mixed_choices_persist(self):
        for volume,render in ((False,False),(True,False),(False,True)):
            result=self.invoke({'exports':{'save_volume_frames':volume,'save_render_frames':render}})
            self.assertEqual(result.returncode,0,result.stderr);self.assertEqual(json.loads(self.path.read_text())['exports'],{'save_volume_frames':volume,'save_render_frames':render})
if __name__=='__main__':unittest.main()
