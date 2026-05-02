# Phase III Sub-phase IIIB Consolidation Checkpoint

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIBConsolidation/20260502T093623Z`

This checkpoint closes IIIB.1-IIIB.7 as a consolidated read-only convergence-tracking sub-phase. It adds no subduction mutation, no triangle consumption, no material transfer, and no new global ownership authority. The gates below exist to catch exactly the lingering review threads: Phase II Slice 5.5 regression, local mixed-signal convergence evidence, and a single rollup signature for future Phase III regression checks.

## Gate Summary

| Gate | Result | Evidence |
|---|---|---|
| Slice 5.5 baseline replay regression | pass | replay hashes stable: pass; state `3b4a85366dab80db`, material ledger `bc3077100ba291b4` |
| Same-pair mixed-signal local gate | pass | pair signed 0.075341390677, local probe 0.001226329058, hits 31416, non-convergent 3828, propagated 34 |
| IIIB rollup signature replay | pass | `df36a5bc9e8f175e` vs `df36a5bc9e8f175e` |

## Slice 5.5 Baseline Regression

| Replay | State hash | Material ledger hash | Contact hash | Label hash | Filter hash | Expected state | Expected ledger | Seconds |
|---:|---|---|---|---|---|---|---|---:|
| 0 | `3b4a85366dab80db` | `bc3077100ba291b4` | `b21ee68fa450142a` | `5279c470b8ff7c1c` | `68428bf8efa6b3c6` | `3b4a85366dab80db` | `bc3077100ba291b4` | 4.254 |
| 1 | `3b4a85366dab80db` | `bc3077100ba291b4` | `b21ee68fa450142a` | `5279c470b8ff7c1c` | `68428bf8efa6b3c6` | `3b4a85366dab80db` | `bc3077100ba291b4` | 4.238 |

## Same-Pair Mixed-Signal Fixture

Fixture: two plates, forced-divergence motion, 40 steps. The closed sphere produces both locally convergent and locally divergent/non-convergent evidence on the same plate pair. The matrix and propagation gates therefore prove local hit location controls IIIB state, not a blanket pair-wide classification.

| Replay | Pair sign | Probe local sign | Matrix pairs | Hits | Non-convergent | Decisions | Seed hits | Added | Closure hash | Rollup signature |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|
| 0 | 0.075341390677 | 0.001226329058 | 1 | 31416 | 3828 | 1 | 523 | 34 | `3cc5d376d632f52c` | `df36a5bc9e8f175e` |
| 1 | 0.075341390677 | 0.001226329058 | 1 | 31416 | 3828 | 1 | 523 | 34 | `3cc5d376d632f52c` | `df36a5bc9e8f175e` |

## IIIB Rollup Signature Components

| Component | Replay 0 hash | Replay 1 hash |
|---|---|---|
| IIIB.1 active list | `3cc5d376d632f52c` | `3cc5d376d632f52c` |
| IIIB.2 distance-to-front | `3cc5d376d632f52c` | `3cc5d376d632f52c` |
| IIIB.3 matrix | `3cc5d376d632f52c` | `3cc5d376d632f52c` |
| IIIB.4 polarity | `a8139942bc021af9` | `a8139942bc021af9` |
| IIIB.6 propagation | `3cc5d376d632f52c` | `3cc5d376d632f52c` |
| IIIB.7 closure | `3cc5d376d632f52c` | `3cc5d376d632f52c` |
| Consolidated IIIB signature | `df36a5bc9e8f175e` | `df36a5bc9e8f175e` |

## Notes

- This checkpoint intentionally corrects IIIB.3's earlier pair-level interpretation: matrix admission is now based on local signed convergence at the active triangle barycenter. Pair keys remain canonical metadata, not the convergence oracle.
- The same-pair fixture requires accepted local hits and rejected non-convergent hits on the same plate pair. The pair-level sign is reported for context only; it is not the matrix admission oracle.
- The consolidated signature is the Phase IIIB regression token future Phase III sub-phases should quote when they need to prove convergence tracking did not drift.

## Recommendation

IIIB consolidation passes. Pause for user review before Phase IIIC planning or implementation.
