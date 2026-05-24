# `src/physics/soft/`

Soft-body reference lane. Provides deformable node/spring allocation,
spring-force integration, iterative spring-length projection, and bounded
triangle area constraints so PhysicsSim has a testable soft-body seam for the
compiler-units rollout before a fuller production solver exists.
