# Fixed velocity correction: accuracy gain, target cost rejection

Four support tests pass. The original small P4/DG-P3 solve publishes a strict
9.557e-12 full residual field with unchanged physical/Float input identities and
all numerical conservation/publication checks. It takes210 iterations/17.487s/
365.109MiB owned peak, versus200/13.550s previously. On the ten real coarse inputs,
one physical-Double residual correction reduces Float velocity inverse residuals
by over100 times, from up to.002626 to5.641e-6. This establishes sensitivity of the
approximate inverse, not useful whole-solver performance or a universal spectrum.

One exact33216tet L8 target reaches cleanup116.584s and full-residual phase150.620s
but is terminated at180.103s, sampled1554.859MiB, with no result or field. Last
logged300iter continuity1.220e-11 exceeds momentum2.070e-12. Original cap is retained;
no numerical full-target/physical success is inferred from the phase marker.
Fresh admission includes14,387,712 extra correction-vector bytes. No retries,
adoption, target qualification, or force-gate substitution. The sealed ten-pressure
field remains the numerical anchor; its performance and raw1.757percent force
failures remain open. Default/native/package/physical certification are unchanged.

Next is a bounded pressure-mode and mesh-conditioning diagnostic, with actual
approximate Schur actions, projected Ritz residuals and localization. Diagnostics
must distinguish a sampled preconditioner spectrum from the complete physical
Schur spectrum; no solution field or physical acceptance is inferred.
