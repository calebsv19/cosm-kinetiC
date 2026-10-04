# Body-relative local refinement matching control

The original absolute-coordinate adaptive controls pass physical geometry, actual
symmetries and global quality but fail the required paired translated inner-cell
identity. No field/factor follows those controls. Before further geometry results,
declare the same radius0.025/0.04m controls with adaptive decisions performed on
body-relative coordinates rounded to the same12-digit convention already used in
mirroring. Restore physical coordinates before mirroring and original plane checks.
This controls translation-sensitive refinement edge choices; do not assume the
paired identity is fixed until actual cells are compared.

Preserve original refinement/Alfeld algebra and boundaries; passes0 must rebuild
original macro coordinates unchanged. Same2048MiB/600s geometry allowance and
120000tet prospective solve cap, no increase in global worst shape/conditioning.
Actual paired inner cells must match for any paired numerical trial. No numerical
field, pressure-mode, equation, residual, native/default or package changes.
