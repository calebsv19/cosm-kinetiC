#include "app/cfd_exterior_room.h"
#include "app/cfd_open_atmosphere3d.h"
bool cfd_exterior_room(CfdOpenAtmosphere3d *s, bool large_window, bool far_exterior) {
    if (!s || !s->storage || s->steps || s->solid_box || !s->ground ||
        !s->lateral_open || s->wind.enabled || s->weather.broad_sweeping_shear) return false;
    const double length = far_exterior ? 2.0 : 1.5;
    const int n = s->grid.n[0];
    if ((far_exterior ? (n != 32 && n != 64) : (n != 24 && n != 48)) ||
        s->grid.n[1] != n || s->grid.n[2] != n ||
        s->grid.length[0] != length || s->grid.length[1] != length || s->grid.length[2] != length) return false;
    const double offset = (length - 1.0) / 2.0;
    unsigned char *mask = cfd_memory_calloc((size_t)s->grid.count, 1);
    if (!mask) return false;
    for (int k = 0; k < n; ++k) for (int j = 0; j < n; ++j) for (int i = 0; i < n; ++i) {
        const double x = (i + .5) * s->grid.h[0];
        const double y = (j + .5) * s->grid.h[1] - offset;
        const double z = (k + .5) * s->grid.h[2];
        const bool room = x < 1 && y >= 0 && y < 1 && z < 1;
        const double lo = large_window ? .25 : .375, hi = large_window ? .75 : .625;
        const bool window = y >= lo && y < hi && z >= .375 && z < .875;
        const bool shell = x < .0625 || y < .0625 || y >= .9375 || z >= .9375 || (x >= .9375 && !window);
        mask[(k * n + j) * n + i] = (unsigned char)(room && shell);
    }
    s->rig_mask = mask; s->solid_box = true; return true;
}
