"""Native strict config structure, typed optional fields and escaped save parity."""
import json
import math
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
class ConfigJson(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory();base=Path(cls.temp.name);source=base/'probe.c'
        source.write_text(r"""
#include "config/config_loader.h"
#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv){if(argc<3)return 9;AppConfig cfg=app_config_default();
if(strcmp(argv[2],"read")){
    if(argc>3){snprintf(cfg.input_root,sizeof(cfg.input_root),"%s",argv[3]);snprintf(cfg.headless_output_dir,sizeof(cfg.headless_output_dir),"%s",argv[3]);snprintf(cfg.atmospheric_warm_start_path,sizeof(cfg.atmospheric_warm_start_path),"%s",argv[3]);snprintf(cfg.retained_runtime_scene_path,sizeof(cfg.retained_runtime_scene_path),"%s",argv[3]);}
    if(!strcmp(argv[2],"nan"))cfg.min_dt=NAN;
    if(!strcmp(argv[2],"infinity"))cfg.min_dt=INFINITY;
    if(!strcmp(argv[2],"unterminated"))memset(cfg.input_root,'x',sizeof(cfg.input_root));
    if(!strcmp(argv[2],"utf8")){cfg.input_root[0]=(char)255;cfg.input_root[1]=0;}
    if(!config_loader_save(&cfg,argv[1]))return 2;
}
ConfigLoadOptions opts={.path=argv[1],.allow_missing=false};if(!config_loader_load(&cfg,&opts))return 3;
json_object *out=json_object_new_object();json_object_object_add(out,"width",json_object_new_int(cfg.grid_w));
json_object_object_add(out,"blur",json_object_new_boolean(cfg.enable_render_blur));json_object_object_add(out,"enabled",json_object_new_boolean(cfg.headless_enabled));
json_object_object_add(out,"density",json_object_new_double(cfg.density_diffusion));json_object_object_add(out,"input",json_object_new_string(cfg.input_root));json_object_object_add(out,"output",json_object_new_string(cfg.headless_output_dir));
json_object_object_add(out,"warm",json_object_new_string(cfg.atmospheric_warm_start_path));json_object_object_add(out,"scene",json_object_new_string(cfg.retained_runtime_scene_path));
puts(json_object_to_json_string_ext(out,JSON_C_TO_STRING_PLAIN));json_object_put(out);return 0;}
""")
        flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','json-c'],text=True));cls.binary=base/'probe'
        sources=['src/config/config_loader.c','src/app/physics_sim_job_json.c','src/app/physics_sim_persistence.c','src/app/physics_sim_headless_output.c','src/app/app_config.c','src/app/data_paths.c']
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ROOT/'include'),'-I'+str(ROOT/'src'),str(source),*[str(ROOT/p) for p in sources],*flags,'-lm','-o',str(cls.binary)],capture_output=True,check=True)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    def setUp(self):
        self.tempdir=tempfile.TemporaryDirectory();self.addCleanup(self.tempdir.cleanup);self.base=Path(self.tempdir.name);self.path=self.base/'config.json'
    def invoke(self,mode='read',text=None):return subprocess.run([str(self.binary),str(self.path),mode,*([text] if text is not None else [])],capture_output=True,text=True,timeout=5)
    def write(self,value):self.path.write_text(json.dumps(value))
    def test_optional_defaults_unknown_fields_and_legacy_aliases(self):
        self.write({'grid':{'width':37},'fluid':{'density_diffusion':0.125},'future':{'field':[True,None,'x']}})
        result=self.invoke();self.assertEqual(result.returncode,0,result.stderr)
        out=json.loads(result.stdout);self.assertEqual(out['width'],37);self.assertEqual(out['density'],0.125)
    def test_conflicting_aliases_hold_matching_aliases_are_supported(self):
        self.write({'fluid':{'diffusion':0.125,'density_diffusion':0.25}})
        self.assertEqual(self.invoke().returncode,3)
        self.write({'fluid':{'diffusion':0.125,'density_diffusion':0.125}})
        result=self.invoke();self.assertEqual(result.returncode,0);self.assertEqual(json.loads(result.stdout)['density'],0.125)
    def test_saved_and_legacy_numeric_booleans_are_applied(self):
        for value in (True,1,2):
            self.write({'render':{'blur_enabled':value},'headless':{'enabled':value}})
            result=self.invoke();self.assertEqual(result.returncode,0,result.stderr)
            self.assertTrue(json.loads(result.stdout)['blur']);self.assertTrue(json.loads(result.stdout)['enabled'])
        self.write({'render':{'blur_enabled':False},'headless':{'enabled':False}})
        result=self.invoke();self.assertEqual(result.returncode,0);self.assertFalse(json.loads(result.stdout)['enabled'])
    def test_malformed_trailing_root_type_and_duplicate_keys_hold(self):
        for text in ('{','{} junk','[]','null','{"grid":{"width":1,"width":2}}',r'{"grid":{},"gr\u0069d":{}}','{"grid":{"width":NaN}}','{"grid":{"width":1e999}}',r'{"x":"\u0000"}'):
            with self.subTest(text=text):
                self.path.write_text(text);self.assertEqual(self.invoke().returncode,3);self.assertEqual(self.path.read_text(),text)
    def test_invalid_utf8_and_depth_bound_hold(self):
        self.path.write_bytes(b'{"x":"\xff"}');self.assertEqual(self.invoke().returncode,3)
        text='{"x":'+'['*65+'0'+']'*65+'}';self.path.write_text(text);self.assertEqual(self.invoke().returncode,3)
        self.write({'x':[0]*100001});self.assertEqual(self.invoke().returncode,3)
    def test_wrong_known_section_and_field_types_hold(self):
        for value in ({'grid':[]},{'grid':None},{'grid':{'width':'37'}},{'grid':{'width':True}},{'paths':{'input_root':23}},{'fluid':{'diffusion':False}},{'headless':{'enabled':'true'}}):
            self.write(value);self.assertEqual(self.invoke().returncode,3,value)
    def test_integer_fraction_and_float_representation_overflows_hold(self):
        for value in ({'grid':{'width':2147483648}},{'grid':{'width':-2147483649}},{'grid':{'width':1.5}},{'fluid':{'diffusion':1e100}}):
            self.write(value);self.assertEqual(self.invoke().returncode,3,value)
        self.write({'grid':{'width':37.0}});self.assertEqual(self.invoke().returncode,0)
    def test_overlong_decoded_path_is_not_silently_truncated(self):
        self.write({'paths':{'input_root':'x'*4096}});self.assertEqual(self.invoke().returncode,3)
    def test_quoted_backslash_control_and_unicode_paths_roundtrip(self):
        text='root/"quoted"\\folder\nbrace}é雪'
        result=self.invoke('save',text);self.assertEqual(result.returncode,0,result.stderr)
        out=json.loads(result.stdout);saved=json.loads(self.path.read_text())
        for key in ('input','output','warm','scene'):self.assertEqual(out[key],text)
        self.assertEqual(saved['paths']['input_root'],text);self.assertEqual(saved['headless']['output_dir'],text)
    def test_nonfinite_and_invalid_utf8_saves_keep_predecessor_and_stage(self):
        for mode in ('nan','infinity','utf8'):
            self.path.write_text('original');self.assertEqual(self.invoke(mode).returncode,2)
            self.assertEqual(self.path.read_text(),'original')
        self.assertEqual(len(list(self.base.glob('.headless-sidecar-*.pending'))),3)
    def test_unterminated_memory_string_holds_before_attempt_allocation(self):
        self.path.write_text('original');self.assertEqual(self.invoke('unterminated').returncode,2)
        self.assertEqual(self.path.read_text(),'original');self.assertEqual(list(self.base.glob('.headless-sidecar-*.pending')),[])
    def test_string_with_section_name_does_not_override_real_section(self):
        self.write({'description':'"grid": {"width": 99}','grid':{'width':37}})
        result=self.invoke();self.assertEqual(result.returncode,0);self.assertEqual(json.loads(result.stdout)['width'],37)
if __name__=='__main__':unittest.main()
