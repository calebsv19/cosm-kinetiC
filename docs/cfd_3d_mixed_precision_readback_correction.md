# Matched-control cost readback correction

The sealed precision checkpoint contains a small transcription error in its
matched count6 base exact, chunk512 table. The completed control's owned peak
is **1151.765625 MiB** (1207713792 bytes), rather than 1153.125 MiB.
The mixed run's owned peak is 1055.015625 MiB, so the measured reduction is
**8.400147%**, rounded to **8.4%**, rather than 8.5%.
The control's independent full FE residual is **3.1431087076e-11**.

This correction changes no numerical source, field, gate, receipt or adoption
decision. The original sealed checkpoint and audit are retained unchanged.
The accurate receipt and audit already contained the values used here.

- [Sealed checkpoint](cfd_3d_mixed_precision_checkpoint.md)
- Audit: `build/c3d-mixed-precision/checkpoint-audit.json`
- Audit SHA-256: `65f48c10c0fc050c9b18fd97d510890c1be3fac907b29645c1476b7b40c014d0`
- Control receipt: `build/c3d-mixed-precision/control-runs/954601a496348bc0314aeed13a564d373623df61eb9cc46236727f46f2362aac/L4-body6-base-double-chunk512-receipt.json`
- Control receipt SHA-256: `b4682fa196c9496fbe2fa6aa67cd0b8c0d95489152e41c84cc2e29af7698b649`
