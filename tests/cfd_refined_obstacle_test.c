#include "app/cfd_memory.h"
#include "app/cfd_refined_mixed.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static int sample(const CfdRefinedMesh *m, double x, double y) {
    for (int c = 0; c < m->cell_count; c++)
        if (fabs(m->cells[c].cx - x) < 1e-10 * m->dx && fabs(m->cells[c].cy - y) < 1e-10 * m->dy)
            return c;
    return -1;
}
int main(int argc, char **argv) {
    clock_t started = clock();
    CfdMemoryBudget memory = {.limit_bytes = SIZE_MAX};
    if (argc > 6) {
        int mib = atoi(argv[6]);
        assert(mib > 0 && mib <= 16384);
        memory.limit_bytes = (size_t)mib * 1024 * 1024;
    }
    CfdMemoryBudget *previous_budget = cfd_memory_scope(&memory);
    int n = argc > 1 ? atoi(argv[1]) : 8;
    assert(n >= 8 && n <= 128 && n % 8 == 0);
    int uniform = argc > 2 ? atoi(argv[2]) : 0;
    int local_levels = argc > 3 ? atoi(argv[3]) : 0;
    assert(local_levels == 0 || (local_levels >= 3 && local_levels <= 10));
    double length = argc > 5 ? atof(argv[5]) : 4;
    assert(isfinite(length) && length >= 4 && length <= 8 && floor(length) == length);
    int domain_nx = (int)(length * n / 2);
    CfdRefinedMesh m;
    if (local_levels) {
        CfdRefinementRegion regions[48];
        int count = 0;
        regions[count++] = (CfdRefinementRegion){1, .5, 3, 1.5, 1};
        /* Keep a wall strip fine enough for three normal samples. Grade the
         * corner neighborhoods geometrically rather than refining far field. */
        double wall_h = 2.0 / n / 8, band = 4 * wall_h;
        regions[count++] = (CfdRefinementRegion){1.5 - band, .75 - band, 2.5 + band, .75 + band, 3};
        regions[count++] =
            (CfdRefinementRegion){1.5 - band, 1.25 - band, 2.5 + band, 1.25 + band, 3};
        regions[count++] =
            (CfdRefinementRegion){1.5 - band, .75 - band, 1.5 + band, 1.25 + band, 3};
        regions[count++] =
            (CfdRefinementRegion){2.5 - band, .75 - band, 2.5 + band, 1.25 + band, 3};
        for (int level = 4; level <= local_levels; level++) {
            double radius = 8 * (2.0 / n) / (1 << level);
            for (int j = 0; j < 2; j++)
                for (int i = 0; i < 2; i++) {
                    double x = 1.5 + i, y = .75 + .5 * j;
                    regions[count++] = (CfdRefinementRegion){x - radius, y - radius, x + radius,
                                                             y + radius, level};
                }
        }
        assert(cfd_refined_mesh_init_regions(&m, domain_nx, n, length, 2, regions, count, 200000));
    } else
        assert(cfd_refined_mesh_init(&m, domain_nx, n, length, 2, uniform ? 0 : n / 2,
                                     uniform ? 0 : n / 4, uniform ? domain_nx : 3 * n / 2,
                                     uniform ? n : 3 * n / 4));
    printf("domain_length_m=%.12g base_dx_m=%.12g\n", length, m.dx);
    printf("local_levels=%d lattice_scale=%d\n", local_levels, m.lattice_scale);
    int before = m.cell_count;
    assert(!cfd_refined_mesh_remove_rectangle(&m, 1.501, .75, 2.5, 1.25));
    assert(m.cell_count == before);
    assert(cfd_refined_mesh_remove_rectangle(&m, 1.5, .75, 2.5, 1.25));
    double volume = 0, perimeter = 0, nx = 0, ny = 0, moment = 0;
    for (int c = 0; c < m.cell_count; c++)
        volume += m.cells[c].volume;
    for (int f = 0; f < m.face_count; f++)
        if (m.faces[f].boundary == CFD_REFINED_SOLID) {
            const CfdRefinedFace *p = &m.faces[f];
            double normal = p->lo >= 0 ? 1 : -1;
            assert((p->lo < 0) != (p->hi < 0));
            perimeter += p->area;
            if (p->axis == 0) {
                nx += normal * p->area;
                moment += p->cx * normal * p->area;
            } else
                ny += normal * p->area;
        }
    assert(fabs(volume - (2 * length - .5)) < 1e-12 && fabs(perimeter - 3) < 1e-12 &&
           fabs(nx) + fabs(ny) < 1e-12 && fabs(moment + .5) < 1e-12);
    clock_t mesh_done = clock();
    CfdRefinedMixed *s = cfd_refined_mixed_create(&m, .1);
    assert(s);
    double *cells = calloc((size_t)5 * m.cell_count, sizeof(double)),
           *faces = calloc((size_t)4 * m.face_count, sizeof(double));
    assert(cells && faces);
    double *u = cells, *v = u + m.cell_count, *p = v + m.cell_count, *ru = p + m.cell_count,
           *rv = ru + m.cell_count;
    double *fu = faces, *fv = fu + m.face_count, *bu = fv + m.face_count, *bv = bu + m.face_count;
    for (int f = 0; f < m.face_count; f++) {
        double y = m.faces[f].cy / 2;
        bu[f] = m.faces[f].boundary == CFD_REFINED_SOLID ? 0 : 6 * .002 * y * (1 - y);
    }
    CfdRefinedMixedReport report;
    CfdRefinedForce body, all;
    assert(!cfd_refined_mixed_boundary_force(s, CFD_REFINED_SOLID, 1, .5, &body));
    clock_t setup_done = clock();
    bool okay = cfd_refined_mixed_solve(s, 0, ru, rv, bu, bv, u, v, fu, fv, p, &report);
    clock_t solve_done = clock();
    if (!okay) {
        fprintf(stderr, "solve failed n=%d iterations=%d residual=%.9g\n", n, report.iterations,
                report.relative_residual);
        return 2;
    }
    assert(cfd_refined_mixed_boundary_force(s, CFD_REFINED_SOLID, 1, .5, &body));
    assert(cfd_refined_mixed_boundary_force(s, -1, 1, .5, &all));
    for (int f = 0; f < m.face_count; f++)
        if (m.faces[f].boundary == CFD_REFINED_SOLID)
            assert(fabs(fu[f]) + fabs(fv[f]) < 1e-14);
    CfdRefinedEnergy energy;
    assert(cfd_refined_mixed_energy(s, 1, .5, NULL, NULL, &energy));
    double physical_residual = fabs(energy.stress_boundary_power_w - energy.strain_dissipation_w) /
                               fmax(fabs(energy.stress_boundary_power_w), 1e-30);
    printf("energy_strain=%.12g energy_boundary=%.12g physical_steady_stokes_residual=%.9g "
           "stabilization_fraction=%.9g algebraic_balance=%.3g\n",
           energy.strain_dissipation_w, energy.stress_boundary_power_w, physical_residual,
           energy.stabilization_w / energy.discrete_diffusion_w,
           fabs(energy.weak_boundary_power_w - energy.discrete_diffusion_w));
    double pr = length == 4 ? .003633153361585 : NAN, vr = length == 4 ? .002347529940743 : NAN;
    clock_t observation_done = clock();
    double reconstructed_p = 0, reconstructed_v = 0;
    bool reconstruction_available = true;
    for (int f = 0; f < m.face_count; f++)
        if (m.faces[f].boundary == CFD_REFINED_SOLID) {
            const CfdRefinedFace *face = &m.faces[f];
            double normal = face->lo >= 0 ? 1 : -1;
            int cell = face->lo >= 0 ? face->lo : face->hi;
            double spacing = (face->axis == 0 ? m.dx : m.dy) * m.cells[cell].span / m.lattice_scale;
            int ids[3];
            for (int k = 0; k < 3; k++) {
                double x = face->cx - (face->axis == 0 ? normal * (k + .5) * spacing : 0);
                double y = face->cy - (face->axis == 1 ? normal * (k + .5) * spacing : 0);
                ids[k] = sample(&m, x, y);
                if (ids[k] < 0)
                    reconstruction_available = false;
            }
            if (ids[0] < 0 || ids[1] < 0 || ids[2] < 0)
                continue;
            if (face->axis == 0)
                reconstructed_p += normal *
                                   (1.875 * p[ids[0]] - 1.25 * p[ids[1]] + .375 * p[ids[2]]) *
                                   face->area * .5;
            else
                reconstructed_v += .1 * (225 * u[ids[0]] - 50 * u[ids[1]] + 9 * u[ids[2]]) /
                                   (60 * spacing) * face->area * .5;
        }
    if (!reconstruction_available)
        reconstructed_p = reconstructed_v = NAN;
    printf("surface_reconstruction_available=%s\n", reconstruction_available ? "true" : "false");
    printf("reconstructed_pressure=%.15g reconstructed_viscous=%.15g pressure_error=%.9g "
           "viscous_error=%.9g total_error=%.9g reaction_disagreement=%.9g\n",
           reconstructed_p, reconstructed_v, fabs(reconstructed_p / pr - 1),
           fabs(reconstructed_v / vr - 1),
           fabs((reconstructed_p + reconstructed_v) / (pr + vr) - 1),
           fabs((reconstructed_p + reconstructed_v) / body.total[0] - 1));

    printf("n=%d uniform=%d cells=%d pressure=%.15g viscous=%.15g total=%.15g pressure_error=%.9g "
           "viscous_error=%.9g total_error=%.9g lift=%.3g closed_balance=%.3g divergence=%.3g "
           "iterations=%d bytes=%zu\n",
           n, uniform, m.cell_count, body.pressure[0], body.viscous[0], body.total[0],
           fabs(body.pressure[0] / pr - 1), fabs(body.viscous[0] / vr - 1),
           fabs(body.total[0] / (pr + vr) - 1), body.total[1], hypot(all.total[0], all.total[1]),
           report.divergence, report.iterations, report.storage_bytes);
    assert(report.divergence < 1e-8 && hypot(all.total[0], all.total[1]) < 1e-9 &&
           fabs(body.total[1]) < 1e-9);
    bool force_qualified =
        fabs(body.pressure[0] / pr - 1) <= .02 && fabs(body.viscous[0] / vr - 1) <= .02;
    bool physical_qualified = length == 4 && force_qualified &&
                              fabs(body.total[0] / (pr + vr) - 1) <= .02 &&
                              physical_residual <= .02;
    printf("separate_force_two_percent_gate=%s\n", length != 4       ? "reference_not_applicable"
                                                   : force_qualified ? "passed"
                                                                     : "not_qualified");
    printf("fixed_stokes_obstacle_physical_gate=%s relative_linear_residual=%.12g\n",
           physical_qualified ? "passed" : "not_qualified", report.relative_residual);
    assert(report.relative_residual <= 1e-11);
    if (argc > 4 && atoi(argv[4]))
        assert(physical_qualified);
    printf("numerical_memory_limit_bytes=%zu numerical_memory_peak_bytes=%zu "
           "numerical_memory_live_bytes=%zu\n",
           memory.limit_bytes, memory.peak_bytes, memory.live_bytes);
    printf("mesh_cpu_s=%.9g setup_cpu_s=%.9g solve_cpu_s=%.9g observation_cpu_s=%.9g reconstruction_cpu_s=%.9g\n",
           (double)(mesh_done-started)/CLOCKS_PER_SEC,
           (double)(setup_done-mesh_done)/CLOCKS_PER_SEC,
           (double)(solve_done-setup_done)/CLOCKS_PER_SEC,
           (double)(observation_done-solve_done)/CLOCKS_PER_SEC,
           (double)(clock()-observation_done)/CLOCKS_PER_SEC);
    free(cells);
    free(faces);
    cfd_refined_mixed_destroy(s);
    cfd_refined_mesh_destroy(&m);
    assert(memory.live_bytes == 0);
    cfd_memory_scope(previous_budget);
    return 0;
}
