# Structural Visualization Reference

This document describes how the structural overlays map solver values to colors
and how the scaling is computed. It applies to both the structural runtime view
and the structural preset editor.

## Overlays

### Axial (T)
- Source: `edge->axial_stress`.
- Mapping: **diverging** palette (blue for negative, red for positive, light
  neutral near zero) with gamma correction.
- Yield: if `|axial_stress|` exceeds the material yield (`sigma_y`), the color
  **blends toward purple** as the exceed ratio increases. This preserves the
  heatmap while still flagging yield.

### Bending (B)
- Source: `edge->bending_moment_a` and `edge->bending_moment_b`.
- Mapping: **diverging** palette with gamma correction.
- Each beam end is colored independently (A and B), then interpolated along the
  beam to show moment gradients.

### Shear (V)
- Source: `0.5 * (shear_force_a + shear_force_b)`.
- Mapping: **heatmap** (magnitude only). Low values blend from a neutral
  baseline to the heat color so small shear remains visible without being
  misleading.

### Combined (Q)
- Source: `sqrt(axial_stress^2 + shear_avg^2)`.
- Mapping: **heatmap** (magnitude only) with gamma correction.
- Use this for quick “overall stress” visualization when sign is less important.

## Scaling

- Default scaling uses the **percentile of magnitudes** (P95) rather than the
  absolute max, which prevents a single outlier from flattening all colors.
- Toggle between percentile and max scaling.
- Optional **scale freeze** keeps the scale fixed after a solve.
- Gamma (< 1.0) boosts mid-range contrast so more beams stand out.
- Beam thickness can also scale with magnitude (optional).

## Controls (Runtime Structural Mode)

- `T` axial overlay
- `B` bending overlay
- `V` shear overlay
- `Q` combined overlay
- `Y` toggle percentile vs max scale
- `G` cycle gamma
- `K` freeze/unfreeze scale
- `X` toggle thickness scaling

## Controls (Structural Preset Editor)

Same as above, but with `Ctrl+` modifier for the visualization toggles:
- `Ctrl+Q`, `Ctrl+Y`, `Ctrl+G`, `Ctrl+K`, `Ctrl+X`

## Debug Tooltip

Hover near a beam to see the raw values:
- Axial stress
- Shear average
- Bending moment A/B

This tooltip is shown in both runtime structural mode and the preset editor.
