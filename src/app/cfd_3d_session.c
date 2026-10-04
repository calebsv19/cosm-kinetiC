#include "app/cfd_3d_session.h"
#include "app/cfd_obstacle3d_box.h"
#include <math.h>
#include <string.h>
static struct json_object *get(struct json_object *o, const char *k) {
    struct json_object *v = NULL;
    if (o)
        json_object_object_get_ex(o, k, &v);
    return v;
}
static bool number(struct json_object *v, double lo, double hi, double *out) {
    if (!v ||
        (json_object_get_type(v) != json_type_int && json_object_get_type(v) != json_type_double))
        return false;
    double x = json_object_get_double(v);
    if (!isfinite(x) || x < lo || x > hi)
        return false;
    *out = x;
    return true;
}
static const char *failure(const Cfd3dSession *s, const char *fallback) {
    switch (s->memory.last_failure) {
    case CFD_MEMORY_LIMIT:
        return "numerical_memory_budget_exceeded";
    case CFD_MEMORY_SIZE_OVERFLOW:
        return "numerical_allocation_size_overflow";
    case CFD_MEMORY_SYSTEM_FAILURE:
        return "numerical_allocation_failed";
    default:
        return fallback;
    }
}
void cfd_3d_session_destroy(Cfd3dSession *s) {
    if (!s)
        return;
    cfd_obstacle3d_destroy(&s->obstacle_duct);
    cfd_duct3d_destroy(&s->duct);
    cfd_open3d_destroy(&s->open_duct);
    cfd_periodic3d_destroy(&s->transient);
    cfd_wall3d_destroy(&s->wall);
    cfd_startup3d_destroy(&s->startup);
    s->ready = false;
}
bool cfd_3d_session_init(Cfd3dSession *s, struct json_object *request) {
    if (!s)
        return false;
    memset(s, 0, sizeof(*s));
    s->failed = true;
    s->error = "invalid_cartesian3d_configuration";
    struct json_object *ch = get(request, "channel"), *fluid = get(request, "fluid"),
                       *grid = get(request, "grid"), *dims = get(ch, "dimensions_m");
    if (json_object_get_type(grid) != json_type_array || json_object_array_length(grid) != 3 ||
        json_object_get_type(dims) != json_type_array || json_object_array_length(dims) != 3)
        return false;
    int n[3];
    double length[3];
    for (int a = 0; a < 3; a++) {
        double v;
        if (!number(json_object_array_get_idx(grid, a), 4, 256, &v) || floor(v) != v ||
            !number(json_object_array_get_idx(dims, a), .1, 100, &length[a]))
            return false;
        n[a] = (int)v;
    }
    double limit = 512, cells = 262144;
    if (!number(get(fluid, "density_kg_m3"), .001, 50000, &s->rho) ||
        !number(get(fluid, "dynamic_viscosity_pa_s"), 1e-9, 1000, &s->mu) ||
        !number(get(request, "dt"), .00001, .1, &s->dt))
        return false;
    if (get(request, "numerical_memory_limit_mib") &&
        (!number(get(request, "numerical_memory_limit_mib"), 1, 4096, &limit) ||
         floor(limit) != limit))
        return false;
    if (get(request, "solver_cell_budget") &&
        (!number(get(request, "solver_cell_budget"), 1, 262144, &cells) || floor(cells) != cells))
        return false;
    if (!cfd_cartesian3d_init(&s->grid, n, length) || s->grid.count > cells) {
        s->error = "cartesian3d_cell_budget_exceeded";
        return false;
    }
    const char *mode = json_object_get_string(get(ch, "solve_mode"));
    if (!mode)
        return false;
    s->box = !strcmp(mode, "steady_box_duct");
    s->obstacle = s->box || !strcmp(mode,"steady_obstacle_duct");
    s->open = !strcmp(mode, "steady_open_duct");
    s->steady = s->obstacle || s->open || !strcmp(mode, "steady_duct");
    if (!strcmp(mode, "wall_stokes_transient"))
        s->transient_kind = CFD_3D_WALL_STOKES;
    else if (!strcmp(mode, "wall_transport_transient"))
        s->transient_kind = CFD_3D_WALL_TRANSPORT;
    else if (!strcmp(mode, "pressure_startup"))
        s->transient_kind = CFD_3D_PRESSURE_STARTUP;
    else if (!strcmp(mode, "open_wall_stokes_transient"))
        s->transient_kind = CFD_3D_OPEN_WALL_STOKES;
    if (!s->steady && !s->transient_kind && strcmp(mode, "manufactured_transient"))
        return false;
    if (s->steady && json_object_get_int(get(request, "steps")) != 1)
        return false;
    double flow = 0;
    if ((s->steady || s->transient_kind == CFD_3D_PRESSURE_STARTUP) &&
        !number(get(ch, "volume_flow_m3_s"), 1e-9, 1e4, &flow))
        return false;
    double body_lower[3] = {0}, body_upper[3] = {0};
    struct json_object *lower = get(ch, "body_min_m"), *upper = get(ch, "body_max_m");
    if (s->box) {
        s->error = "invalid_stationary_box_bounds";
        if (get(ch, "center_x_m") || json_object_get_type(lower) != json_type_array ||
            json_object_get_type(upper) != json_type_array ||
            json_object_array_length(lower) != 3 || json_object_array_length(upper) != 3)
            return false;
        for (int a = 0; a < 3; a++)
            if (!number(json_object_array_get_idx(lower, a), 0, length[a], &body_lower[a]) ||
                !number(json_object_array_get_idx(upper, a), 0, length[a], &body_upper[a]) ||
                body_upper[a] <= body_lower[a])
                return false;
    } else if (lower || upper) {
        s->error = "body_bounds_require_steady_box_duct";
        return false;
    }
    double center=2;
    if(s->obstacle && !s->box && get(ch,"center_x_m") && !number(get(ch,"center_x_m"),.5,length[0]-.5,&center))return false;
    s->memory.limit_bytes = (size_t)limit * 1024 * 1024;
    CfdMemoryBudget *prev = cfd_memory_scope(&s->memory);
    bool ok = s->box ? cfd_obstacle3d_box_init(&s->obstacle_duct, n, length, s->rho,
                                               s->mu, flow, body_lower, body_upper)
              : s->obstacle ? cfd_obstacle3d_init(&s->obstacle_duct,n,length,s->rho,s->mu,flow,center)
              : s->transient_kind == CFD_3D_PRESSURE_STARTUP
                  ? cfd_startup3d_init(&s->startup, n, length, s->rho, s->mu, s->dt, flow)
              : s->transient_kind == CFD_3D_OPEN_WALL_STOKES
                  ? cfd_wall3d_init_open(&s->wall, n, length, s->rho, s->mu, s->dt)
              : s->transient_kind ? cfd_wall3d_init(&s->wall, n, length, s->rho, s->mu, s->dt,
                                                    s->transient_kind == CFD_3D_WALL_TRANSPORT)
              : s->open   ? cfd_open3d_init(&s->open_duct, n, length, s->rho, s->mu, flow, 0)
              : s->steady ? cfd_duct3d_init(&s->duct, n, length, s->rho, s->mu, flow)
                          : cfd_periodic3d_init(&s->transient, n, length, s->rho, s->mu, s->dt);
    cfd_memory_scope(prev);
    if (!ok) {
        s->error = failure(s, "cartesian3d_setup_failed_or_reynolds_limit");
        cfd_3d_session_destroy(s);
        return false;
    }
    s->ready = true;
    s->failed = false;
    s->error = NULL;
    return true;
}
bool cfd_3d_session_step(Cfd3dSession *s) {
    if (!s || !s->ready || s->failed || s->cancelled)
        return false;
    CfdMemoryBudget *prev = cfd_memory_scope(&s->memory);
    bool ok = s->obstacle ? cfd_obstacle3d_solve(&s->obstacle_duct)
              : s->transient_kind == CFD_3D_PRESSURE_STARTUP ? cfd_startup3d_step(&s->startup)
              : s->transient_kind                          ? cfd_wall3d_step(&s->wall)
              : s->open                                    ? cfd_open3d_solve(&s->open_duct)
              : s->steady                                  ? cfd_duct3d_solve(&s->duct)
                                                           : cfd_periodic3d_step(&s->transient);
    cfd_memory_scope(prev);
    if (!ok) {
        s->failed = true;
        const char *error = s->obstacle ? s->obstacle_duct.error
                            : s->transient_kind == CFD_3D_PRESSURE_STARTUP ? s->startup.error
                            : s->transient_kind                          ? s->wall.error
                                                                         : NULL;
        s->error = failure(s, error ? error : "cartesian3d_solve_failed_or_cfl_bound");
        if (!strcmp(s->error, "cancelled_at_krylov_checkpoint")) {
            s->cancelled = true;
            s->failed = false;
        }
        return false;
    }
    s->accepted_transport_cfl = s->wall.cfl;
    s->accepted_transport_power_w = s->wall.transport_power_w;
    s->accepted_transport_self_power_w = s->wall.transport_self_power_w;
    s->observed = true;
    s->time = s->transient_kind == CFD_3D_PRESSURE_STARTUP ? s->startup.time
              : s->transient_kind                          ? s->wall.time
              : s->steady                                  ? 0
                                                           : s->transient.time;
    if (s->transient_kind && s->transient_kind != CFD_3D_PRESSURE_STARTUP)
        cfd_3d_harmonic_observe(s);
    return true;
}

const char *cfd_3d_session_mode(const Cfd3dSession *s) {
    if(s->box)return "steady_box_duct";
    if(s->obstacle)return "steady_obstacle_duct";
    switch (s->transient_kind) {
    case CFD_3D_WALL_STOKES:
        return "wall_stokes_transient";
    case CFD_3D_WALL_TRANSPORT:
        return "wall_transport_transient";
    case CFD_3D_PRESSURE_STARTUP:
        return "pressure_startup";
    case CFD_3D_OPEN_WALL_STOKES:
        return "open_wall_stokes_transient";
    default:
        return s->open ? "steady_open_duct" : s->steady ? "steady_duct" : "manufactured_transient";
    }
}
void cfd_3d_session_checkpoint(Cfd3dSession *s, CfdMixed3dCheckpoint function, void *context) {
    if(s->obstacle){cfd_obstacle_mixed3d_checkpoint(s->obstacle_duct.mixed,function,context);return;}
    CfdMixed3d *mixed = s->transient_kind == CFD_3D_PRESSURE_STARTUP ? s->startup.mixed
                        : s->transient_kind                          ? s->wall.mixed
                                                                     : NULL;
    if (mixed)
        cfd_mixed3d_checkpoint(mixed, function, context);
}
