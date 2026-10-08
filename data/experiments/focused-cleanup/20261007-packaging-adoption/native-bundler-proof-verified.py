from pathlib import Path
import sys,subprocess,shutil,os,json,hashlib
source=Path('/Users/calebsv/Desktop/CodeWork/physics_sim');sys.path.insert(0,str(source/'scripts'))
import macos_bundle
from package_outputs import plan,declare
base=Path('/private/tmp/physics-packaging-adoption-20261007');repo=base/'native-source-verified';repo.mkdir(exist_ok=False)
for rel in ['scripts/macos_bundle.py','scripts/macos_bundle_engine.sh','tools/packaging/macos/bundle-dylibs.sh']:
 p=repo/rel;p.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source/rel,p)
deps=repo/'dependencies';(deps/'lib').mkdir(parents=True)
(deps/'fixture.c').write_text('int physics_fixture(void) { return 7; }\n')
(deps/'main.c').write_text('extern int physics_fixture(void); int main(void) { return physics_fixture() == 7 ? 0 : 1; }\n')
root=repo/'dist/native-attempt';app=root/'fixture.app';validate=lambda:plan(repo,root,[app],[]);declare(validate(),validate)
binary=app/'Contents/MacOS/physics-sim-bin';binary.parent.mkdir(parents=True);frameworks=app/'Contents/Frameworks';frameworks.mkdir()
commands=[['clang','-dynamiclib','-Wl,-no_adhoc_codesign','-Wl,-headerpad_max_install_names','-install_name','@rpath/libphysics_fixture.dylib',str(deps/'fixture.c'),'-o',str(deps/'lib/libphysics_fixture.dylib')],['clang','-Wl,-no_adhoc_codesign','-Wl,-headerpad_max_install_names',str(deps/'main.c'),'-L'+str(deps/'lib'),'-lphysics_fixture','-Wl,-rpath,'+str(deps/'lib'),'-o',str(binary)]]
results=[]
for command in commands:
 r=subprocess.run(command,capture_output=True,text=True,timeout=60);results.append({'command':command,'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr});assert r.returncode==0,r.stderr
before=subprocess.check_output(['/usr/bin/otool','-L',str(binary)],text=True)
os.environ['PACKAGE_DEP_SEARCH_ROOTS']=str(deps)
for key in list(os.environ):
 if key.startswith('PHYSICS_SIM_BUILD_OWNER') or key.startswith('PHYSICS_SIM_BUILD_HIERARCHY'):os.environ.pop(key,None)
attempt=macos_bundle.run(repo,binary,frameworks,wall_cap=60)
after=subprocess.check_output(['/usr/bin/otool','-L',str(binary)],text=True)
id_readback=subprocess.check_output(['/usr/bin/otool','-D',str(frameworks/'libphysics_fixture.dylib')],text=True)
assert '@executable_path/../Frameworks/libphysics_fixture.dylib' in after,after
assert '@loader_path/libphysics_fixture.dylib' in id_readback,id_readback
receipt={'commands':results,'before':before,'after':after,'dylib_identity':id_readback,'attempt':str(attempt),'copied_library':str(frameworks/'libphysics_fixture.dylib'),'result':'passed','scope':'Native Mach-O compile, dependency copy and install-name readback only; no signing or execution'}
(base/'native-bundler-proof.json').write_text(json.dumps(receipt,indent=2)+'\n');print(receipt['scope'],flush=True)
