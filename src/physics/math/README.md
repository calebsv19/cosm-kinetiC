# `src/physics/math/`

Houses shared math utilities. Right now only `math2d.c` lives here to anchor the inline helpers defined in `include/physics/math/math2d.h`. When we add gradient/divergence kernels, matrix helpers, or pressure solvers, they land in this folder so every subsystem can share them.
