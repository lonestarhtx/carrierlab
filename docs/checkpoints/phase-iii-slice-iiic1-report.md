# Phase III Slice IIIC.1 Checkpoint

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIC1/20260502T211528Z`

Status: first Phase IIIC mutation slice. This slice adds opt-in plate-local/per-window subducting triangle marks derived from accepted IIIB polarity evidence. It does not add trench elevation, uplift, slab pull, collision, rifting, erosion, terrain displacement, global ownership, or projection-history authority.

## Gate Summary

| Gate | Result | Evidence |
|---|---|---|
| Bypass disabled | pass | Slice 5.5 fixed fixture state `3b4a85366dab80db` / `3b4a85366dab80db`, ledger `bc3077100ba291b4` / `bc3077100ba291b4` |
| Deterministic subducting marks | pass | mark count 84 / 84, hash `d32e7289d548e202` / `d32e7289d548e202` |
| No-admissible negative | pass | zero-motion matrix pairs 0, decisions 0, marks 0 |
| Remesh reset and filter consumption | pass | before marks 84, persistent mark inputs 84, filtered candidates 60, after marks 0, reset serial 0 -> 1 |

## Bypass Gate

With `bEnablePhaseIIICSubductingMarks=false`, the fixed Slice 5.5 replay must continue to hit the accepted baseline hashes. This is a fixed-fixture regression, not a global proof that every run is unchanged.

| Replay | State hash | Expected state | Material ledger hash | Expected ledger | Persistent mark inputs | Seconds |
|---:|---|---|---|---|---:|---:|
| 0 | `3b4a85366dab80db` | `3b4a85366dab80db` | `bc3077100ba291b4` | `bc3077100ba291b4` | 0 | 3.194 |
| 1 | `3b4a85366dab80db` | `3b4a85366dab80db` | `bc3077100ba291b4` | `bc3077100ba291b4` | 0 | 3.181 |

## Marking Replay

The enabled fixture is two plates under forced convergence with plate 0 continental and plate 1 oceanic. Marks are emitted only for the under-plate triangle from accepted local IIIB evidence.

| Replay | Step | Matrix pairs | Decisions | Marks | Invalid | Under mismatch | Non-subduction decisions | Mark hash | Seconds |
|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|
| 0 | 1 | 1 | 1 | 84 | 0 | 0 | 0 | `d32e7289d548e202` | 0.071 |
| 1 | 1 | 1 | 1 | 84 | 0 | 0 | 0 | `d32e7289d548e202` | 0.071 |

### Representative Marks

| Replay | Mark | Plate | Other | Local triangle | Evidence | Signed local velocity | Decision |
|---:|---:|---:|---:|---:|---:|---:|---|
| 0 | 0 | 1 | 0 | 250 | 86 | 0.068070912065 | oceanic_under_continental |
| 0 | 1 | 1 | 0 | 1706 | 87 | 0.066856551625 | oceanic_under_continental |
| 0 | 2 | 1 | 0 | 1735 | 88 | 0.068565989154 | oceanic_under_continental |
| 0 | 3 | 1 | 0 | 1750 | 89 | 0.070616024043 | oceanic_under_continental |
| 0 | 4 | 1 | 0 | 1775 | 90 | 0.069510215478 | oceanic_under_continental |
| 0 | 5 | 1 | 0 | 1805 | 91 | 0.074931574407 | oceanic_under_continental |
| 0 | 6 | 1 | 0 | 1807 | 92 | 0.072566014470 | oceanic_under_continental |
| 0 | 7 | 1 | 0 | 1808 | 93 | 0.071971300039 | oceanic_under_continental |
| 0 | 8 | 1 | 0 | 1819 | 94 | 0.074009601051 | oceanic_under_continental |
| 0 | 9 | 1 | 0 | 1857 | 95 | 0.075252952098 | oceanic_under_continental |
| 0 | 10 | 1 | 0 | 1860 | 96 | 0.075309485684 | oceanic_under_continental |
| 0 | 11 | 1 | 0 | 1862 | 97 | 0.075325486327 | oceanic_under_continental |
| 0 | 12 | 1 | 0 | 1904 | 98 | 0.072946821530 | oceanic_under_continental |
| 0 | 13 | 1 | 0 | 1913 | 99 | 0.066287888042 | oceanic_under_continental |
| 0 | 14 | 1 | 0 | 1956 | 100 | 0.058806709211 | oceanic_under_continental |
| 0 | 15 | 1 | 0 | 1981 | 101 | 0.063183964523 | oceanic_under_continental |

## Negative And Reset Controls

| Control | Marks before | Persistent mark inputs | Filtered candidates | Marks after | Reset serial | Result |
|---|---:|---:|---:|---:|---|---|
| zero-motion no-admissible | 0 | 0 | 0 | 0 | 0 | pass |
| filtered resample reset | 84 | 84 | 60 | 0 | 0 -> 1 | pass |

## Scope Notes

- The new marks are plate-local/per-window process state. They are reset by the existing plate-local rebuild at remesh.
- The normal Phase II label filter path remains the source when labels are supplied. Persistent IIIC marks are additional filter inputs only when `bEnablePhaseIIICSubductingMarks=true`.
- This checkpoint may claim deterministic IIIC.1 marking and filter consumption only. It does not claim Stage 1.5 carrier success, Slice 5.5 asymmetry resolution, elevation behavior, or slab-pull correctness.

## Recommendation

IIIC.1 passes. Pause for user review before IIIC.2 visible/historical elevation work.
