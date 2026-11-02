#ifndef PHYSICS_SIM_JOINT_THERMAL_H
#define PHYSICS_SIM_JOINT_THERMAL_H
#include "physics_sim_thermal_step.h"
#define JOINT_PROFILE "wall-refined-v1-n512-cn51200-ex6400"
typedef struct {
    unsigned version,mode;
    uint64_t generation;
    double last_q;
    PhysicsSimThermalState thermal;
} PhysicsSimJointState;
typedef struct {CfdOpenAtmosphere3d gas;PhysicsSimLocalMatrix matrix;int mode;bool initialized;uint64_t receiver_binding[4];PhysicsSimReceiverStage receiver_stage;void *receiver_context;} PhysicsSimJointRuntime;
typedef struct {
    PhysicsSimJointState state;
    PhysicsSimThermalTrace trace;
    uint64_t base_hash,state_hash;
} PhysicsSimJointCandidate;
typedef enum {JOINT_OK=0,JOINT_REFUSED=1,JOINT_IO=2,JOINT_PUBLISHED_RELOAD=3} PhysicsSimJointStatus;
bool physics_sim_joint_seed(int mode,PhysicsSimJointState *out);
bool physics_sim_joint_init(PhysicsSimJointRuntime *runtime,int mode,PhysicsSimJointState *out);
void physics_sim_joint_destroy(PhysicsSimJointRuntime *runtime);
bool physics_sim_joint_valid(const PhysicsSimJointRuntime *runtime,const PhysicsSimJointState *state);
bool physics_sim_joint_prepare(PhysicsSimJointRuntime *runtime,const PhysicsSimJointState *accepted,
    int expected_tick,int available_tick,int fail,PhysicsSimJointCandidate *out);
bool physics_sim_joint_accept(PhysicsSimJointRuntime *runtime,PhysicsSimJointState *accepted,
    const PhysicsSimJointCandidate *candidate);
/* Local versioned field codec; checksum detects corruption, not authentication.
 * State/config are reconstructed, never memcpy'd across a persisted ABI. */
uint64_t physics_sim_joint_digest(const PhysicsSimJointState *state);
bool physics_sim_joint_load(const char *path,const PhysicsSimJointRuntime *runtime,PhysicsSimJointState *out);
/* Single-writer lock + compare-and-swap + fsync(temp),rename,fsync(parent).
 * expected=NULL requires absent destination. fail1/2 simulate before publish;
 * fail3 simulates interruption after durable publish and requires same-file read.
 * Refusal/prepublish failure leaves authoritative checkpoint and caller unchanged. */
PhysicsSimJointStatus physics_sim_joint_publish(const char *path,const PhysicsSimJointRuntime *runtime,
    const PhysicsSimJointState *expected,const PhysicsSimJointState *next,int fail);
#endif
