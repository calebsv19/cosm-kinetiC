#include "app/sim_mode.h"

extern const SimModeHooks g_sim_mode_box;
extern const SimModeHooks g_sim_mode_wind;

const SimModeHooks *sim_mode_get_hooks(SimulationMode mode) {
    switch (mode) {
    case SIM_MODE_WIND_TUNNEL:
        return &g_sim_mode_wind;
    case SIM_MODE_BOX:
    default:
        return &g_sim_mode_box;
    }
}
