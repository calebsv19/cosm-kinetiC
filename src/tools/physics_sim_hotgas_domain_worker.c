#include "app/cfd_hotgas_domain3d.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void values(const double *v, int count) {
    printf("[");
    for (int i = 0; i < count; ++i)
        printf("%s%.17g", i ? "," : "", v[i]);
    printf("]");
}
static void add(CfdHotGasDomainBudget *a, const CfdHotGasDomainBudget *b) {
    for (int f = 0; f < 6; ++f) {
        for (int i = 0; i < 5; ++i)
            a->outward[f][i] += b->outward[f][i];
        a->potential_outward_j[f] += b->potential_outward_j[f];
        a->conducted_outward_j[f] += b->conducted_outward_j[f];
    }
    for (int i = 0; i < 3; ++i)
        a->gravity_impulse_kg_m_s[i] += b->gravity_impulse_kg_m_s[i];
    a->gravity_work_j += b->gravity_work_j;
}
int main(int argc, char **argv) {
    bool caloric = argc==13 && strcmp(argv[12],"caloric")==0;
    if (argc!=11 && argc!=12 && !caloric) return 2;
    if (argc>=12 && strcmp(argv[11],"hllc") && strcmp(argv[11],"hllc_muscl") &&
        strcmp(argv[11],"lhllc_muscl") && !(caloric && strcmp(argv[11],"rusanov")==0)) return 2;
    double v[10];
    for (int i = 0; i < 10; ++i) {
        char *end = NULL;
        errno = 0;
        v[i] = strtod(argv[i + 1], &end);
        if (errno || end == argv[i + 1] || *end || !isfinite(v[i]))
            return 2;
    }
    for (int i = 0; i < 3; ++i)
        if (v[i] != floor(v[i]) || v[i] < 4 || v[i] > 256)
            return 2;
    if (v[9] != floor(v[9]) || v[9] < 0 || v[9] > 100000 || v[8] <= 0)
        return 2;
    int n[3] = {(int)v[0], (int)v[1], (int)v[2]}, code = 3;
    double length[3] = {v[3], v[4], v[5]};
    CfdHotGas3d s;
    if (!cfd_hotgas3d_init(&s, n, length, v[6], v[7]))
        return 3;
    if (caloric) {
        double moles[4], e, cp;
        if (scanf("%lf %lf %lf %lf",moles,moles+1,moles+2,moles+3)!=4 ||
            !cfd_hotgas_caloric_init(&s.caloric,moles) ||
            !cfd_hotgas_caloric_properties(&s.caloric,298.15,&e,&cp) ||
            fabs(s.gas_constant_j_kg_k-s.caloric.gas_constant)>1e-12*s.caloric.gas_constant ||
            fabs(s.gamma-cp/(cp-s.caloric.gas_constant))>1e-12) {
            cfd_hotgas3d_destroy(&s); return 3;
        }
    }
    CfdHotGasDomain domain = {0};
    domain.contact_flux = argc >= 12 && strcmp(argv[11], "lhllc_muscl") == 0 ? 3 : argc >= 12 ? (strcmp(argv[11], "hllc_muscl") == 0 ? 2 : 1) : 0;
    if (caloric && strcmp(argv[11],"rusanov")==0) domain.contact_flux=0;
    double *source = cfd_memory_calloc((size_t)5 * s.grid.count, sizeof(double));
    double *samples = cfd_memory_calloc((size_t)8 * 3 * n[0] * n[1], sizeof(double));
    if (!source || !samples)
        goto done;
    if (scanf("%lf %lf %lf %lf", domain.gravity_m_s2, domain.gravity_m_s2 + 1,
              domain.gravity_m_s2 + 2, &domain.conductivity_w_m_k) != 4)
        goto done;
    for (int f = 0; f < 6; ++f) {
        double kind;
        CfdHotGasFace *b = domain.face + f;
        if (scanf("%lf %lf %lf %lf %lf %lf %lf", &kind, &b->reservoir_pressure_origin_pa,
                  &b->reservoir_temperature_k, b->reservoir_velocity_m_s,
                  b->reservoir_velocity_m_s + 1, b->reservoir_velocity_m_s + 2,
                  &b->wall_temperature_k) != 7 ||
            !isfinite(kind) || kind != floor(kind) || kind < 0 || kind > 2)
            goto done;
        b->kind = (CfdHotGasFaceKind)kind;
    }
    if (!cfd_hotgas_domain3d_validate(&s, &domain))
        goto done;
    for (int i = 0; i < 5 * s.grid.count; ++i)
        if (scanf("%lf", s.state + i) != 1)
            goto done;
    for (int i = 0; i < 5 * s.grid.count; ++i)
        if (scanf("%lf", source + i) != 1)
            goto done;
    double count, height[8];
    if (scanf("%lf", &count) != 1 || !isfinite(count) || count != floor(count) || count < 0 ||
        count > 8)
        goto done;
    for (int i = 0; i < (int)count; ++i)
        if (scanf("%lf", height + i) != 1)
            goto done;
    char tail;
    if (scanf(" %c", &tail) == 1)
        goto done;
    for (int q = 0; q < s.grid.count; ++q) {
        double t, p, c;
        if (!cfd_hotgas3d_primitive(&s, s.state + 5 * q, &t, &p, &c))
            goto done;
    }
    CfdHotGasDomainBudget total = {0};
    for (int step = 0; step < (int)v[9]; ++step) {
        CfdHotGasDomainBudget budget;
        if (!cfd_hotgas_domain3d_step(&s, &domain, source, v[8], &budget))
            goto done;
        add(&total, &budget);
    }
    int plane = n[0] * n[1];
    for (int i = 0; i < (int)count; ++i)
        if (!cfd_hotgas_domain3d_sample(&s, &domain, height[i], samples + 3 * i * plane,
                                        samples + (3 * i + 1) * plane,
                                        samples + (3 * i + 2) * plane))
            goto done;
    printf("{\"conserved\":");
    values(s.state, 5 * s.grid.count);
    printf(",\"budget\":{\"outward\":[");
    for (int f = 0; f < 6; ++f) {
        if (f)
            printf(",");
        values(total.outward[f], 5);
    }
    printf("],\"potential_outward_j\":");
    values(total.potential_outward_j, 6);
    printf(",\"conducted_outward_j\":");
    values(total.conducted_outward_j, 6);
    printf(",\"gravity_impulse_kg_m_s\":");
    values(total.gravity_impulse_kg_m_s, 3);
    printf(",\"gravity_work_j\":%.17g},\"samples\":[", total.gravity_work_j);
    for (int i = 0; i < (int)count; ++i) {
        printf("%s{\"height_m\":%.17g,\"temperature_k\":", i ? "," : "", height[i]);
        values(samples + 3 * i * plane, plane);
        printf(",\"conducted_downward_w_m2\":");
        values(samples + (3 * i + 1) * plane, plane);
        printf(",\"advected_upward_w_m2\":");
        values(samples + (3 * i + 2) * plane, plane);
        printf("}");
    }
    printf("]}\n");
    code = 0;
done:
    cfd_memory_free(source);
    cfd_memory_free(samples);
    cfd_hotgas3d_destroy(&s);
    return code;
}
