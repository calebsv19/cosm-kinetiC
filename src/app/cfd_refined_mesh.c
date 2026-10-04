#include "app/cfd_memory.h"
#include "app/cfd_refined_mesh.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
void cfd_refined_mesh_destroy(CfdRefinedMesh *m) {
    if (!m)
        return;
    cfd_memory_free(m->cells);
    cfd_memory_free(m->faces);
    memset(m, 0, sizeof(*m));
}
bool cfd_refined_mesh_init(CfdRefinedMesh *m, int nx, int ny, double length, double height, int x0,
                           int y0, int x1, int y1) {
    if (!m)
        return false;
    memset(m, 0, sizeof(*m));
    if (nx < 2 || ny < 2 || nx > 1024 || ny > 1024 || !isfinite(length) || !isfinite(height) ||
        length <= 0 || height <= 0 || x0 < 0 || y0 < 0 || x1 > nx || y1 > ny || x0 >= x1 ||
        y0 >= y1)
        return false;
    m->lattice_scale = 2;
    m->nx = nx;
    m->ny = ny;
    m->dx = length / nx;
    m->dy = height / ny;
    const int fx = 2 * nx, fy = 2 * ny;
    int *owner = cfd_memory_malloc((size_t)fx * fy * sizeof(*owner));
    m->cells = cfd_memory_calloc((size_t)nx * ny + 3 * (x1 - x0) * (y1 - y0), sizeof(*m->cells));
    m->faces = cfd_memory_calloc((size_t)(fx + 1) * fy + (size_t)(fy + 1) * fx, sizeof(*m->faces));
    if (!owner || !m->cells || !m->faces) {
        cfd_memory_free(owner);
        cfd_refined_mesh_destroy(m);
        return false;
    }
    for (int j = 0; j < ny; ++j)
        for (int i = 0; i < nx; ++i) {
            const int span = i >= x0 && i < x1 && j >= y0 && j < y1 ? 1 : 2;
            for (int b = 0; b < 2; b += span)
                for (int a = 0; a < 2; a += span) {
                    int id = m->cell_count++;
                    CfdRefinedCell *c = &m->cells[id];
                    *c = (CfdRefinedCell){2 * i + a,
                                          2 * j + b,
                                          span,
                                          j * nx + i,
                                          (2 * i + a + .5 * span) * .5 * m->dx,
                                          (2 * j + b + .5 * span) * .5 * m->dy,
                                          .25 * span * span * m->dx * m->dy};
                    for (int v = 0; v < span; ++v)
                        for (int u = 0; u < span; ++u)
                            owner[(c->y + v) * fx + c->x + u] = id;
                }
        }
    /* Adjacent segments with identical cell pair are merged; fine/coarse
     * interfaces remain separate subfaces owned by the two actual cells. */
    for (int axis = 0; axis < 2; ++axis) {
        int normal = axis == 0 ? fx : fy, tangent = axis == 0 ? fy : fx;
        for (int k = 0; k <= normal; ++k) {
            for (int t = 0; t < tangent;) {
                int lo = k == 0 ? -1 : owner[axis == 0 ? t * fx + k - 1 : (k - 1) * fx + t];
                int hi = k == normal ? -1 : owner[axis == 0 ? t * fx + k : k * fx + t];
                if (lo == hi) {
                    ++t;
                    continue;
                }
                int end = t + 1;
                while (end < tangent) {
                    int l = k == 0 ? -1 : owner[axis == 0 ? end * fx + k - 1 : (k - 1) * fx + end];
                    int h = k == normal ? -1 : owner[axis == 0 ? end * fx + k : k * fx + end];
                    if (l != lo || h != hi)
                        break;
                    ++end;
                }
                CfdRefinedFace *f = &m->faces[m->face_count++];
                *f = (CfdRefinedFace){lo,
                                      hi,
                                      axis,
                                      (axis == 0 ? k : .5 * (t + end)) * .5 * m->dx,
                                      (axis == 0 ? .5 * (t + end) : k) * .5 * m->dy,
                                      (end - t) * .5 * (axis == 0 ? m->dy : m->dx),
                                      lo < 0 || hi < 0 ? CFD_REFINED_DOMAIN : CFD_REFINED_INTERIOR};
                t = end;
            }
        }
    }
    cfd_memory_free(owner);
    return true;
}
void cfd_refined_restrict(const CfdRefinedMesh *m, const double *leaf, double *coarse) {
    memset(coarse, 0, (size_t)m->nx * m->ny * sizeof(*coarse));
    for (int i = 0; i < m->cell_count; ++i)
        coarse[m->cells[i].parent] += m->cells[i].volume / (m->dx * m->dy) * leaf[i];
}
void cfd_refined_prolong(const CfdRefinedMesh *m, const double *coarse, const double *gx,
                         const double *gy, double *leaf) {
    for (int i = 0; i < m->cell_count; ++i) {
        const CfdRefinedCell *c = &m->cells[i];
        int p = c->parent;
        leaf[i] = coarse[p] + gx[p] * (c->cx - (p % m->nx + .5) * m->dx) +
                  gy[p] * (c->cy - (p / m->nx + .5) * m->dy);
    }
}
void cfd_refined_flux_balance(const CfdRefinedMesh *m, const double *velocity, double *balance) {
    memset(balance, 0, (size_t)m->cell_count * sizeof(*balance));
    for (int i = 0; i < m->face_count; ++i) {
        const CfdRefinedFace *f = &m->faces[i];
        double flux = velocity[i] * f->area;
        if (f->lo >= 0)
            balance[f->lo] += flux;
        if (f->hi >= 0)
            balance[f->hi] -= flux;
    }
}

bool cfd_refined_mesh_remove_rectangle(CfdRefinedMesh *m, double x0, double y0, double x1,
                                       double y1) {
    if (!m || !m->cells || !m->faces || !isfinite(x0) || !isfinite(y0) || !isfinite(x1) ||
        !isfinite(y1) || x0 <= 0 || y0 <= 0 || x1 >= m->nx * m->dx || y1 >= m->ny * m->dy ||
        x0 >= x1 || y0 >= y1)
        return false;
    for (int f = 0; f < m->face_count; f++)
        if (m->faces[f].boundary == CFD_REFINED_SOLID)
            return false;
    int *map = cfd_memory_malloc((size_t)m->cell_count * sizeof(int));
    if (!map)
        return false;
    int kept = 0, removed = 0;
    double area = 0, eps = 1e-10 * fmin(m->dx, m->dy);
    for (int i = 0; i < m->cell_count; i++) {
        const CfdRefinedCell *c = &m->cells[i];
        double hx = .5 * c->span * m->dx / m->lattice_scale,
               hy = .5 * c->span * m->dy / m->lattice_scale;
        bool intersects = c->cx + hx > x0 + eps && c->cx - hx < x1 - eps && c->cy + hy > y0 + eps &&
                          c->cy - hy < y1 - eps;
        if (intersects) {
            if (c->span >= m->lattice_scale || c->cx - hx < x0 - eps || c->cx + hx > x1 + eps ||
                c->cy - hy < y0 - eps || c->cy + hy > y1 + eps) {
                cfd_memory_free(map);
                return false;
            }
            map[i] = -1;
            removed++;
            area += c->volume;
        } else
            map[i] = kept++;
    }
    if (!removed || fabs(area - (x1 - x0) * (y1 - y0)) > 1e-10 * (x1 - x0) * (y1 - y0)) {
        cfd_memory_free(map);
        return false;
    }
    CfdRefinedCell *cells = cfd_memory_malloc((size_t)kept * sizeof(*cells));
    CfdRefinedFace *faces = cfd_memory_malloc((size_t)m->face_count * sizeof(*faces));
    if (!cells || !faces) {
        cfd_memory_free(cells);
        cfd_memory_free(faces);
        cfd_memory_free(map);
        return false;
    }
    for (int i = 0; i < m->cell_count; i++)
        if (map[i] >= 0)
            cells[map[i]] = m->cells[i];
    int count = 0;
    for (int i = 0; i < m->face_count; i++) {
        CfdRefinedFace f = m->faces[i];
        int old_lo = f.lo, old_hi = f.hi;
        f.lo = old_lo < 0 ? -1 : map[old_lo];
        f.hi = old_hi < 0 ? -1 : map[old_hi];
        if (f.lo < 0 && f.hi < 0)
            continue;
        if ((old_lo >= 0 && f.lo < 0) || (old_hi >= 0 && f.hi < 0))
            f.boundary = CFD_REFINED_SOLID;
        faces[count++] = f;
    }
    cfd_memory_free(map);
    cfd_memory_free(m->cells);
    cfd_memory_free(m->faces);
    m->cells = cells;
    m->faces = faces;
    m->cell_count = kept;
    m->face_count = count;
    return true;
}
