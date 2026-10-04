#include "app/cfd_memory.h"
#include "app/cfd_refined_mesh.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
/* Mesh construction is fixed before operator assembly. Integer side events
 * partition each geometric edge without allocating a dense finest-level grid. */
typedef struct {
    int axis, plane, position, side, cell, start;
} SideEvent;
typedef struct {
    CfdRefinedMesh *mesh;
    int capacity, limit;
    const CfdRefinementRegion *regions;
    int region_count;
} Builder;
static bool append_leaf(Builder *b, CfdRefinedCell c) {
    if (b->mesh->cell_count >= b->limit)
        return false;
    if (b->mesh->cell_count == b->capacity) {
        int next = b->capacity ? 2 * b->capacity : 128;
        if (next > b->limit)
            next = b->limit;
        CfdRefinedCell *p = cfd_memory_realloc(b->mesh->cells, (size_t)next * sizeof(*p));
        if (!p)
            return false;
        b->mesh->cells = p;
        b->capacity = next;
    }
    b->mesh->cells[b->mesh->cell_count++] = c;
    return true;
}
static CfdRefinedCell leaf(const CfdRefinedMesh *m, int x, int y, int span, int parent) {
    double hx = m->dx / m->lattice_scale, hy = m->dy / m->lattice_scale;
    return (CfdRefinedCell){x,
                            y,
                            span,
                            parent,
                            (x + .5 * span) * hx,
                            (y + .5 * span) * hy,
                            span * (double)span * hx * hy};
}
static bool subdivide(Builder *b, int x, int y, int span, int parent, int level) {
    CfdRefinedMesh *m = b->mesh;
    double hx = m->dx / m->lattice_scale, hy = m->dy / m->lattice_scale;
    bool refine = false;
    for (int i = 0; i < b->region_count; i++) {
        const CfdRefinementRegion *r = &b->regions[i];
        if (level < r->level && x * hx < r->x1 && (x + span) * hx > r->x0 && y * hy < r->y1 &&
            (y + span) * hy > r->y0)
            refine = true;
    }
    if (!refine)
        return append_leaf(b, leaf(m, x, y, span, parent));
    int half = span / 2;
    for (int j = 0; j < 2; j++)
        for (int i = 0; i < 2; i++)
            if (!subdivide(b, x + i * half, y + j * half, half, parent, level + 1))
                return false;
    return true;
}
static int event_compare(const void *a, const void *b) {
    const SideEvent *p = a, *q = b;
#define CMP(member)                                                                                \
    if (p->member != q->member)                                                                    \
    return (p->member > q->member) - (p->member < q->member)
    CMP(axis);
    CMP(plane);
    CMP(position);
    CMP(start);
    CMP(side);
    CMP(cell);
#undef CMP
    return 0;
}
static bool build_faces(CfdRefinedMesh *m) {
    int nc = m->cell_count;
    SideEvent *events = cfd_memory_malloc((size_t)8 * nc * sizeof(*events));
    CfdRefinedFace *faces = cfd_memory_malloc((size_t)4 * nc * sizeof(*faces));
    if (!events || !faces) {
        cfd_memory_free(events);
        cfd_memory_free(faces);
        return false;
    }
    int used = 0, count = 0;
    for (int c = 0; c < nc; c++) {
        const CfdRefinedCell *p = &m->cells[c];
        for (int axis = 0; axis < 2; axis++)
            for (int side = 0; side < 2; side++) {
                int plane = (axis == 0 ? p->x : p->y) + (side == 0 ? p->span : 0);
                int pos = axis == 0 ? p->y : p->x;
                events[used++] = (SideEvent){axis, plane, pos, side, c, 1};
                events[used++] = (SideEvent){axis, plane, pos + p->span, side, c, 0};
            }
    }
    qsort(events, used, sizeof(*events), event_compare);
    int k = 0;
    while (k < used) {
        int axis = events[k].axis, plane = events[k].plane, active[2] = {-1, -1};
        while (k < used && events[k].axis == axis && events[k].plane == plane) {
            int pos = events[k].position;
            do {
                SideEvent e = events[k++];
                if (e.start) {
                    if (active[e.side] >= 0)
                        goto fail;
                    active[e.side] = e.cell;
                } else {
                    if (active[e.side] != e.cell)
                        goto fail;
                    active[e.side] = -1;
                }
            } while (k < used && events[k].axis == axis && events[k].plane == plane &&
                     events[k].position == pos);
            if (k == used || events[k].axis != axis || events[k].plane != plane)
                break;
            int end = events[k].position;
            if (active[0] < 0 && active[1] < 0)
                continue;
            bool boundary = active[0] < 0 || active[1] < 0;
            if (boundary && plane != 0 && plane != (axis == 0 ? m->nx : m->ny) * m->lattice_scale)
                goto fail;
            if (count >= 4 * nc)
                goto fail;
            double x = (axis == 0 ? plane : .5 * (pos + end)) * m->dx / m->lattice_scale;
            double y = (axis == 0 ? .5 * (pos + end) : plane) * m->dy / m->lattice_scale;
            double area = (end - pos) * (axis == 0 ? m->dy : m->dx) / m->lattice_scale;
            faces[count++] = (CfdRefinedFace){active[0],
                                              active[1],
                                              axis,
                                              x,
                                              y,
                                              area,
                                              boundary ? CFD_REFINED_DOMAIN : CFD_REFINED_INTERIOR};
        }
        if (active[0] >= 0 || active[1] >= 0)
            goto fail;
    }
    cfd_memory_free(events);
    cfd_memory_free(m->faces);
    m->faces = faces;
    m->face_count = count;
    return true;
fail:
    cfd_memory_free(events);
    cfd_memory_free(faces);
    return false;
}
bool cfd_refined_mesh_init_regions(CfdRefinedMesh *m, int nx, int ny, double length, double height,
                                   const CfdRefinementRegion *regions, int region_count,
                                   int max_cells) {
    if (!m)
        return false;
    memset(m, 0, sizeof(*m));
    if (nx < 2 || ny < 2 || nx > 1024 || ny > 1024 || !isfinite(length) || !isfinite(height) ||
        length <= 0 || height <= 0 || region_count < 0 || region_count > 256 ||
        (region_count && !regions) || max_cells < nx * ny || max_cells > 10000000)
        return false;
    int level = 0;
    for (int i = 0; i < region_count; i++) {
        const CfdRefinementRegion *r = &regions[i];
        if (!isfinite(r->x0) || !isfinite(r->y0) || !isfinite(r->x1) || !isfinite(r->y1) ||
            r->x0 < 0 || r->y0 < 0 || r->x1 > length || r->y1 > height || r->x0 >= r->x1 ||
            r->y0 >= r->y1 || r->level < 0 || r->level > 10)
            return false;
        if (r->level > level)
            level = r->level;
    }
    m->nx = nx;
    m->ny = ny;
    m->dx = length / nx;
    m->dy = height / ny;
    m->lattice_scale = 1 << level;
    Builder b = {m, 0, max_cells, regions, region_count};
    for (int j = 0; j < ny; j++)
        for (int i = 0; i < nx; i++)
            if (!subdivide(&b, i * m->lattice_scale, j * m->lattice_scale, m->lattice_scale,
                           j * nx + i, 0))
                goto fail;
    for (;;) {
        if (!build_faces(m))
            goto fail;
        unsigned char *split = cfd_memory_calloc(m->cell_count, 1);
        if (!split)
            goto fail;
        int additions = 0, old_count = m->cell_count;
        for (int f = 0; f < m->face_count; f++) {
            int lo = m->faces[f].lo, hi = m->faces[f].hi;
            if (lo < 0 || hi < 0)
                continue;
            int large = m->cells[lo].span > m->cells[hi].span ? lo : hi,
                small = large == lo ? hi : lo;
            if (m->cells[large].span > 2 * m->cells[small].span && !split[large]) {
                split[large] = 1;
                additions += 3;
            }
        }
        if (!additions) {
            cfd_memory_free(split);
            return true;
        }
        if (additions > max_cells - old_count) {
            cfd_memory_free(split);
            goto fail;
        }
        for (int c = 0; c < old_count; c++)
            if (split[c]) {
                CfdRefinedCell p = m->cells[c];
                int half = p.span / 2;
                m->cells[c] = leaf(m, p.x, p.y, half, p.parent);
                for (int j = 0; j < 2; j++)
                    for (int i = 0; i < 2; i++)
                        if (i || j)
                            if (!append_leaf(
                                    &b, leaf(m, p.x + i * half, p.y + j * half, half, p.parent))) {
                                cfd_memory_free(split);
                                goto fail;
                            }
            }
        cfd_memory_free(split);
    }
fail:
    cfd_refined_mesh_destroy(m);
    return false;
}
