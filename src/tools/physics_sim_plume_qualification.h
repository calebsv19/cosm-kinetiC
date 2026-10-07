/* App-owned, local qualification transport. Numerical stepping is unchanged. */
#ifndef PHYSICS_SIM_PLUME_QUALIFICATION_H
#define PHYSICS_SIM_PLUME_QUALIFICATION_H
#include <stdint.h>

static bool plume_write_sample(const CfdOpenAtmosphere3d *s, const char *root) {
    char path[4096];
    int length = snprintf(path, sizeof(path), "%s/sample-%04d.bin", root, s->steps);
    if (length < 0 || (size_t)length >= sizeof(path)) return false;
    FILE *file = fopen(path, "wbx");
    if (!file) return false;
    uint32_t integers[4] = {(uint32_t)s->grid.n[0], (uint32_t)s->grid.n[1],
                            (uint32_t)s->grid.n[2], (uint32_t)s->steps};
    double scalars[8] = {s->time, s->dt, s->input_j, s->input_kg,
                        s->initial_j, s->initial_kg, s->divergence, s->residual};
    uint64_t work = s->scalar_work_cells;
    bool ok = fwrite("PSCPQ001", 1, 8, file) == 8 &&
        fwrite(integers, sizeof(uint32_t), 4, file) == 4 &&
        fwrite(scalars, sizeof(double), 8, file) == 8 &&
        fwrite(&work, sizeof(work), 1, file) == 1;
    const double *arrays[5] = {s->velocity, s->pressure, s->energy, s->smoke, s->flux};
    int counts[5] = {s->velocity_count, s->grid.count, s->grid.count,
                     s->grid.count, 8*s->plane};
    for (int a = 0; ok && a < 5; ++a)
        ok = fwrite(arrays[a], sizeof(double), (size_t)counts[a], file) == (size_t)counts[a];
    if (fclose(file)) ok = false;
    if (!ok) remove(path);
    return ok;
}

static bool plume_sparse_qualification(CfdOpenAtmosphere3d *s, double *input,
                                       const char *schedule, const char *root, double peaks[4], bool movie) {
    uint32_t endian = 1;
    struct stat statbuf;
    if (*(unsigned char *)&endian != 1 || sizeof(double) != 8 || s->steps != 0 ||
        stat(schedule, &statbuf) || statbuf.st_size <= 0 || statbuf.st_size > 64*1024*1024)
        return false;
    FILE *file = fopen(schedule, "rb");
    if (!file) return false;
    char magic[8]; uint32_t header[5], samples[160]; double dt;
    bool ok = fread(magic, 1, 8, file) == 8 && !memcmp(magic, "PSQUAL01", 8) &&
        fread(header, sizeof(uint32_t), 5, file) == 5 &&
        fread(&dt, sizeof(dt), 1, file) == 1;
    if (!ok || header[0] != (uint32_t)s->grid.n[0] ||
        header[1] != (uint32_t)s->grid.n[1] || header[2] != (uint32_t)s->grid.n[2] ||
        header[3] == 0 || header[3] > (movie ? 6400u : 3200u) ||
        header[4] == 0 || header[4] > (movie ? 160u : 3u) ||
        dt != s->dt || dt*header[3] > (movie ? 32.0 : 8.0) ||
        fread(samples, sizeof(uint32_t), header[4], file) != header[4]) {
        fclose(file); return false;
    }
    for (uint32_t a = 0; a < header[4]; ++a)
        if (!samples[a] || samples[a] > header[3] || (a && samples[a] <= samples[a-1])) {
            fclose(file); return false;
        }
    if (samples[header[4]-1] != header[3]) { fclose(file); return false; }
    int n = s->grid.count; uint32_t sample = 0;
    peaks[0] = s->reference_k; peaks[1] = peaks[2] = peaks[3] = 0;
    for (uint32_t step = 0; ok && step < header[3]; ++step) {
        memset(input, 0, (size_t)2*n*sizeof(double));
        uint32_t count, previous = 0;
        ok = fread(&count, sizeof(count), 1, file) == 1 && count <= (uint32_t)n;
        for (uint32_t row = 0; ok && row < count; ++row) {
            uint32_t cell; double amounts[2];
            ok = fread(&cell, sizeof(cell), 1, file) == 1 &&
                fread(amounts, sizeof(double), 2, file) == 2 &&
                cell < (uint32_t)n && (!row || cell > previous) &&
                isfinite(amounts[0]) && amounts[0] >= 0 && amounts[0] <= 1e12 &&
                isfinite(amounts[1]) && amounts[1] >= 0 && amounts[1] <= 1e12;
            if (ok) { input[cell] = amounts[0]; input[n+cell] = amounts[1]; previous = cell; }
        }
        if (ok) ok = cfd_open_atmosphere3d_step(s, input, input+n);
        if (!ok) {
            fprintf(stderr, "sparse qualification rejected before step %u; accepted_steps=%d time_s=%.17g\n",
                    step+1, s->steps, s->time);
            break;
        }
        double capacity = s->rho*s->cp*s->grid.volume;
        for (int q = 0; q < n; ++q)
            peaks[0] = fmax(peaks[0], s->reference_k+s->energy[q]/capacity);
        for (int q = 0; q < s->velocity_count; ++q)
            peaks[1] = fmax(peaks[1], fabs(s->velocity[q]));
        peaks[2] = fmax(peaks[2], s->divergence);
        peaks[3] = fmax(peaks[3], s->residual);
        if ((uint32_t)s->steps == samples[sample]) {
            ok = plume_write_sample(s, root);
            fprintf(stderr, "native qualification sample steps=%d time_s=%.17g\n", s->steps, s->time);
            if (++sample == header[4]) break;
        }
    }
    if (ok) ok = fgetc(file) == EOF && !ferror(file) && cfd_open_atmosphere3d_valid(s);
    fclose(file);
    return ok;
}
#endif
