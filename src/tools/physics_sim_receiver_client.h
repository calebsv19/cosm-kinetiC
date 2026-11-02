#ifndef PHYSICS_SIM_RECEIVER_CLIENT_H
#define PHYSICS_SIM_RECEIVER_CLIENT_H
#include "physics_sim_thermal_step.h"
#include <sys/types.h>
typedef struct {int input,output;pid_t pid;uint64_t transactions,stages;bool open,healthy;unsigned columns;} PhysicsSimReceiverClient;
bool physics_sim_receiver_client_open(PhysicsSimReceiverClient *client,const char *executable,int mode);
bool physics_sim_receiver_client_open_profile(PhysicsSimReceiverClient *client,const char *executable,const char *profile);
void physics_sim_receiver_client_close(PhysicsSimReceiverClient *client);
bool physics_sim_receiver_client_stage(void *context,int column,const ThermalReceiver *accepted,
    const ThermalReceiverExchange *packet,CoreSimTimePoint available,ThermalReceiver *candidate,ThermalReceiverReceipt *receipt);
#endif
