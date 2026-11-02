"""Build isolated coupled diagnostic; no consumer adoption or shared mutation."""
import argparse,hashlib,json,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument("--tail-diffusion",action="store_true");p.add_argument("--tail-time",action="store_true");p.add_argument("--durable-shear",action="store_true");p.add_argument("--live-receiver",action="store_true");p.add_argument("--joint-thermal",action="store_true");p.add_argument("--warm-solid",action="store_true");p.add_argument("--shear-normal256",action="store_true");p.add_argument("--normal-mean",action="store_true");p.add_argument("--repaired-scalars",action="store_true");p.add_argument("--solid-trace",action="store_true");p.add_argument("--edge-candidate",action="store_true");p.add_argument("--localized-startup",action="store_true");p.add_argument('--local-fine-end',type=int,choices=[48,64],default=48);p.add_argument('--interface-trace',action='store_true');p.add_argument('--local-micro-factor',type=int,choices=[1,2],default=1);p.add_argument('--wall',action='store_true');p.add_argument('--patch',action='store_true');p.add_argument('--columns',action='store_true');p.add_argument('--packet-trace',action='store_true');p.add_argument('--core-version',choices=['0.3.2','0.3.3'],default='0.3.2');p.add_argument('--output',type=Path,required=True);p.add_argument('--thermal',type=Path,required=True);p.add_argument('--shared',type=Path,required=True);a=p.parse_args()
assert not a.tail_time or a.tail_diffusion
assert not a.tail_diffusion or (a.shear_normal256 and not a.durable_shear)
assert not a.durable_shear or (a.shear_normal256 and a.live_receiver)
assert not a.live_receiver or a.joint_thermal or a.shear_normal256
assert not a.joint_thermal or (a.warm_solid and a.columns and a.core_version=="0.3.3")
assert not a.warm_solid or (a.columns and not a.normal_mean and not a.localized_startup and not a.solid_trace and not a.shear_normal256)
assert not a.shear_normal256 or (a.repaired_scalars and a.columns and a.solid_trace and not a.normal_mean)
assert not a.normal_mean or (a.columns and not a.localized_startup and not a.solid_trace and not a.packet_trace)
assert not a.repaired_scalars or (a.localized_startup and a.local_micro_factor==2 and a.local_fine_end==48 and not a.interface_trace)
assert not a.solid_trace or (a.columns and not a.packet_trace)
assert a.local_micro_factor==1 or a.localized_startup
assert not a.packet_trace or a.columns
assert not a.interface_trace or a.localized_startup
assert a.local_fine_end==48 or a.localized_startup
a.patch=a.patch or a.columns
a.wall=a.wall or a.patch
r=Path(__file__).resolve().parents[1];t=a.thermal.resolve();s=a.shared.resolve();out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
assert (s/'core/core_thermal/VERSION').read_text().strip()==a.core_version
gas=[r/f'src/app/{x}.c' for x in ['cfd_open_atmosphere3d','cfd_open_atmosphere3d_box','cfd_open_atmosphere3d_box_preconditioner','cfd_open_atmosphere3d_lateral','cfd_open_atmosphere3d_wind','cfd_open_atmosphere3d_vent','cfd_open_atmosphere3d_vent_pressure','cfd_open_atmosphere3d_vent_balance','cfd_passive3d','cfd_cartesian3d','cfd_sparse_mg','cfd_memory']]+[r/'src/tools/physics_sim_heated_face.c']
core=[s/f'core/core_thermal/src/{x}.c' for x in ['core_thermal','core_thermal_rod','core_thermal_material','core_thermal_material_rod']]+[s/f'core/core_sim/src/{x}.c' for x in ['core_sim_time','core_sim_schedule','core_sim_exchange']]
host=[t/'src/experiment/participant.c',t/'src/coupling/inert.c',t/'src/coupling/receiver.c'];driver=r/'src/tools/physics_sim_thermal_transport_probe.c';ref=r/'src/tools/physics_sim_thermal_transport_reference.c'
if a.wall:
 gas=[x for x in gas if x.name!='cfd_open_atmosphere3d.c']+[r/'src/tools/physics_sim_wall_prescribed.c']
 driver=r/'src/tools/physics_sim_thermal_wall_probe.c';ref=r/'src/tools/physics_sim_thermal_wall_reference.c'
if a.patch:
 driver=r/'src/tools/physics_sim_thermal_patch_probe.c';ref=r/'src/tools/physics_sim_thermal_patch_reference.c'
if a.columns:
 driver=r/'src/tools/physics_sim_thermal_columns_probe.c';ref=r/'src/tools/physics_sim_thermal_columns_reference.c'
if a.warm_solid:
 driver=r/"src/tools/physics_sim_warm_solid_probe.c";ref=r/"src/tools/physics_sim_warm_solid_reference.c"
if a.warm_solid:gas += [r/"src/tools/physics_sim_thermal_step.c"]
if a.live_receiver:gas += [r/"src/tools/physics_sim_receiver_client.c"]
if a.joint_thermal:
 gas += [r/"src/tools/physics_sim_joint_thermal.c",r/"src/tools/physics_sim_joint_checkpoint.c"]
 driver=r/"src/tools/physics_sim_joint_thermal_cli.c"
if a.durable_shear:gas += [r/"src/tools/physics_sim_shear_checkpoint.c"]
if a.normal_mean:driver=r/'src/tools/physics_sim_normal_mean_probe.c'
if a.localized_startup:
 assert a.columns
 gas += [r/'src/tools/physics_sim_localized_columns.c']
headers=[r/'src/tools/physics_sim_shear_clock.h',r/'src/tools/physics_sim_scalar_fct.h',r/'src/tools/physics_sim_scalar_logquadratic.h',r/'src/tools/physics_sim_scalar_front_target.h',r/'src/tools/physics_sim_scalar_exponential.h',r/'src/tools/physics_sim_localized_columns.h',r/'src/tools/physics_sim_local_matrix.h',r/'src/app/cfd_scalar_edge_candidate.h']+list((r/'include/app').glob('*.h'))+[r/'src/tools/physics_sim_heated_face.h']+list((t/'include/thermal_sim').glob('*.h'))+list((t/'src/coupling').glob('*.h'))+list((s/'core/core_sim/include').glob('*.h'))+list((s/'core/core_thermal/include').glob('*.h'))
if a.warm_solid:headers += [r/"src/tools/physics_sim_normal_conservative.h",r/"src/tools/physics_sim_thermal_step.h"]
if a.joint_thermal:headers += [r/"src/tools/physics_sim_joint_thermal.h",r/"src/tools/physics_sim_receiver_fixture.h"]
if a.live_receiver:headers += [r/"src/tools/physics_sim_receiver_fixture.h",r/"src/tools/physics_sim_receiver_wire.h",r/"src/tools/physics_sim_receiver_client.h",r/"src/tools/physics_sim_receiver_worker.c"]
if a.repaired_scalars:headers += [r/'src/tools/physics_sim_nonuniform_advection.h',r/'src/tools/physics_sim_nonuniform_diffusion.h']
if a.shear_normal256 or a.live_receiver:headers += [r/"src/tools/physics_sim_shear_receiver_fixture.h"]
if a.localized_startup:headers += [r/"src/tools/physics_sim_localized_state.h"]
if a.tail_diffusion:headers += [r/"src/tools/physics_sim_tail_diffusion.h",r/"src/tools/physics_sim_tail_fit.h",r/"src/tools/physics_sim_scalar_logquadratic.h",r/"src/tools/physics_sim_scalar_front_target.h",r/"src/tools/physics_sim_scalar_exponential.h"]
if a.tail_time:headers += [r/'src/tools/physics_sim_tail_time.h']
if a.durable_shear:headers += [r/"src/tools/physics_sim_shear_checkpoint.h",r/"scripts/run_shear_durable.py",r/"tests/cfd_shear_checkpoint_test.c"]
if a.solid_trace:headers += [r/'src/tools/physics_sim_columns_solid_trace.h']
if a.interface_trace:headers += [r/'src/tools/physics_sim_local_interface_trace.h']
if a.packet_trace:headers += [r/'src/tools/physics_sim_patch6_trace.h',r/'src/tools/physics_sim_patch6_replay.c']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
if a.core_version=='0.3.3':assert sha(s/'core/core_thermal/src/core_thermal_material_rod.c')=='801d1467be00edecfbba0f4ed42bc332e7dc26b50519bcfd9864e814fbe2ab49'
inputs={str(p):sha(p) for p in sorted(set(gas+core+host+headers+[r/'src/app/cfd_open_atmosphere3d.c',driver,ref,Path(__file__)]))}
identity=hashlib.sha256(json.dumps(inputs,sort_keys=True).encode()).hexdigest()
fixture=dict(domain_m=[.25,.25,.5],source_z=[.125,.25],support_z=[11/32,12/32],area_m2=.0625,rho_cp=1206,solid_capacity=[50,50],solid_conductance=2,reference_k=300,ticks_per_second=1600)
if a.wall:
 fixture.update(domain_m=[.25,.25,.0625],source_z=[1/64,1/32],support_z='first gas cell only',observation_z=[0,1/128],wall_z=0,contact_h_w_m2_k=10,gas_half_cell_resistance=True,solid_half_cell_resistance=True,prescribed_shear='u_x=0.05*z/0.0625; u_y=u_z=0',scalar_dt_s=1/3200)
if a.patch:fixture.update(source_x=[1/32,1/16],pulse_end_s=.4,periodic_x=True,observations='transport x[1/16,3/32] z[1/64,1/32]; tail x[3/32,1/8] same z; wall x[1/16,3/32] z[0,1/128]')
if a.columns:fixture.update(solid_support='one x-strip per gas x cell; independent two-cell columns',solid_capacity_per_column='50/nx per depth cell',solid_conductance_per_column='2/nx',lateral_solid_conduction=False)
if a.normal_mean:fixture.update(reduction='periodic x mean; source fraction 1/8; native 4x4 exchange support',time_parameters='selected independently at run time')
if a.shear_normal256:fixture.update(profile="prescribed-shear-v1-nx128-nz256-ex5-6400",ticks_per_second=6400,scalar_fine_dt_s=1/51200,scalar_coarse_dt_s=1/12800,exchange_s=5/6400,source_off_s=.4,restriction_s=391/800,observation_s=1/40,normal_cells=256)
if a.warm_solid:
 fixture=dict(domain_m=[.25,.25,.0625],native_grid=[4,4,256],duration_s=5,observation_s=.01,scalar_dt_s=1/51200,exchange_nominal_s=5/6400,exchange_event_split=True,receiver_tps=6400,receiver_window_steps=1,reference_k=300,gas_initial_k=320,solid_initial_k=[300,300],heater_k_s=1,heater_off_s=1,gas_bound_k=21,rho_cp=1206,gas_k_w_m_k=.026,solid_capacity_j_k=[50,50],solid_conductance_w_k=2,contact_h_w_m2_k=10,prescribed_shear="u_x=.05*z/.0625",controls=["exchange-off","reversal"],physical_gate=.1,reference_gate=.005)
if a.joint_thermal:fixture.update(profile="wall-refined-v1-n512-cn51200-ex6400",native_grid=[4,4,512],exchange_nominal_s=1/6400,normal_theta=.5,composition="normal-half/wall/normal-half",checkpoint_schema=1,live_thermal=False)
fixture_id=hashlib.sha256(json.dumps(fixture,sort_keys=True).encode()).hexdigest()
flags=['-std=c11','-O2','-Wall','-Wextra','-Werror','-ffp-contract=off']
includes=[r/'include',r/'src/tools',t/'include',t/'src/coupling',s/'core/core_sim/include',s/'core/core_thermal/include']
command=['clang',*flags,*['-I'+str(x) for x in includes],'-DPHYSICS_SIM_THIN_TRANSPORT_QUALIFIER=1',f'-DCOUPLED_SOURCE_ID="{identity}"',f'-DCOUPLED_FIXTURE_ID="{fixture_id}"',f'-DCOUPLED_SUPPORT_ID="{fixture_id}"']
if a.tail_diffusion:command += ["-DPHYSICS_SIM_TAIL_DIFFUSION=1"]
if a.tail_time:command += ["-DPHYSICS_SIM_TAIL_TIME=1"]
if a.durable_shear:command += ["-DPHYSICS_SIM_SHEAR_DURABLE=1"]
if a.live_receiver:command += ["-DPHYSICS_SIM_LIVE_RECEIVER=1"]
if a.wall:command+=['-DPHYSICS_SIM_OPEN_FULL_GRID_QUALIFIER=1','-DPHYSICS_SIM_OPEN_EXPLICIT_SCALAR_DT=0.0003125']
if a.localized_startup:command+=['-DPHYSICS_SIM_LOCALIZED_COLUMNS=1']
if a.localized_startup and a.local_micro_factor!=1:command+=[f'-DPHYSICS_SIM_LOCAL_MICRO_FACTOR={a.local_micro_factor}']
if a.localized_startup:command+=[f'-DPHYSICS_SIM_LOCAL_FINE_END={a.local_fine_end}']
if a.interface_trace:command+=['-DPHYSICS_SIM_INTERFACE_TRACE=1']
if a.shear_normal256:command+=['-DPHYSICS_SIM_SHEAR_NORMAL256=1']
if a.repaired_scalars:command+=['-DPHYSICS_SIM_REPAIRED_SCALARS=1']
if a.solid_trace:command+=['-DPHYSICS_SIM_COLUMNS_SOLID_TRACE=1']
if a.edge_candidate:command+=['-DPHYSICS_SIM_OPEN_EDGE_CANDIDATE=1']
if a.packet_trace:command+=['-DPHYSICS_SIM_PATCH6_TRACE=1']
for name,extra in [('probe',[]),('probe-sanitize',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
 subprocess.run(command+extra+list(map(str,gas+core+host+[driver]))+['-lm','-o',str(out/name)],check=True)
if a.durable_shear:
 for name,extra in [('checkpoint-test',[]),('checkpoint-test-sanitize',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
  subprocess.run(command+extra+list(map(str,gas+core+host+[r/'tests/cfd_shear_checkpoint_test.c']))+['-lm','-o',str(out/name)],check=True)
if a.live_receiver:
 import sys
 for name,extra in [('receiver-worker',[]),('receiver-worker-sanitize',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
  strip=['-Wl,-dead_strip'] if sys.platform=='darwin' else ['-ffunction-sections','-Wl,--gc-sections']
  subprocess.run(command+extra+strip+list(map(str,core+host+[r/'src/tools/physics_sim_receiver_worker.c']))+['-lm','-o',str(out/name)],check=True)
subprocess.run(['clang',*flags,str(ref),'-lm','-o',str(out/'reference')],check=True)
if a.packet_trace:
 for name,extra in [('patch6-replay',[]),('patch6-replay-sanitize',['-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer'])]:
  subprocess.run(command+extra+list(map(str,gas+core+host+[r/'src/tools/physics_sim_patch6_replay.c']))+['-lm','-o',str(out/name)],check=True)
for name,driver in ([('columns-test','cfd_heated_columns_test.c'),('face-test','cfd_heated_face_test.c'),('support-test','cfd_heated_face_support_test.c'),('slab-test','cfd_heated_slab_test.c')] if a.columns else [] if a.wall else [('face-test','cfd_heated_face_test.c'),('support-test','cfd_heated_face_support_test.c')]):
 test_gas=[x for x in gas if x.name not in {"physics_sim_thermal_step.c","physics_sim_joint_thermal.c","physics_sim_joint_checkpoint.c","physics_sim_receiver_client.c","physics_sim_shear_checkpoint.c"}]
 test_command=command
 if a.columns and name!='columns-test':
  test_gas=[x for x in test_gas if x.name!='physics_sim_wall_prescribed.c']+[r/'src/app/cfd_open_atmosphere3d.c']
  test_command=[x for x in command if not x.startswith('-DPHYSICS_SIM_OPEN_')]
 subprocess.run(test_command+[str(x) for x in test_gas]+[str(r/'tests'/driver),'-lm','-o',str(out/name)],check=True)
assert all(sha(Path(k))==v for k,v in inputs.items()),'source drift during compilation'
names=['probe','probe-sanitize','reference','columns-test','face-test','support-test','slab-test'] if a.columns else ['probe','probe-sanitize','reference'] if a.wall else ['probe','probe-sanitize','reference','face-test','support-test']
if a.durable_shear:names += ['checkpoint-test','checkpoint-test-sanitize']
if a.live_receiver:names += ['receiver-worker','receiver-worker-sanitize']
if a.packet_trace:names+=['patch6-replay','patch6-replay-sanitize']
(out/'identity.json').write_text(json.dumps(dict(inputs=inputs,source=identity,fixture=fixture,fixture_sha256=fixture_id,core_version=a.core_version,flags=command,binaries={x:sha(out/x) for x in names}),indent=2)+'\n')
