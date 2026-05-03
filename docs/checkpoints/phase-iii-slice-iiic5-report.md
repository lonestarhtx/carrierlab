# Phase III Slice IIIC.5 Checkpoint

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIC5/20260503T070430Z`

Status: IIIC elevation-ledger extension. This slice adds named audit lines for IIIC visible-elevation deltas, separate from the Phase II material ledger. It does not add new tectonic behavior, collision, rifting, erosion, terrain displacement, projection-derived ownership, or any new resampling mutation path.

Ledger equation: `actual plate-local visible elevation delta = trench_visible_elevation_delta + overriding_uplift_delta + residual`. The ledger sums plate-local carrier vertices, not projected global samples.

## Gate Summary

| Gate | Result | Evidence |
|---|---|---|
| Slice 5.5 bypass | pass | state `3b4a85366dab80db` / `3b4a85366dab80db`, material ledger `bc3077100ba291b4` / `bc3077100ba291b4` |
| IIIB closure continuity (non-gating smoke) | pass | expected independent token `bf8818a26ed7b1dc` is listed for continuity only; this standalone slice does not claim an IIIB signature gate |
| Full elevation ledger | pass | records 11189 / 11189, trench 149 / 149, uplift 11040 / 11040, residual 1.854516540334e-12 / 1.854516540334e-12 km |
| Trench-only ledger line | pass | trench records 149, uplift records 0, residual 0.000000000000e+00 km |
| Disabled elevation mutations | pass | marks 84, records 0, actual delta 0.000000000000 km |
| Slab pull excluded from elevation ledger | pass | marks 84, slab contributions 84, ledger records 0 |
| Negative controls | pass | zero 0, single 0, divergence-no-subduction 0 records |

## Primary Full IIIC Elevation Ledger

| Replay | Marks | Records | Unique vertices | Actual delta km | Ledger delta km | Trench delta km | Uplift delta km | Residual km | Uplift audit residual km | Ledger hash | Visible hash | Crust hash | Seconds |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|---:|
| 0 | 84 | 11189 | 1048 | -51.176588935012 | -51.176588935014 | -1862.500000000000 | 1811.323411064979 | 1.854516540334e-12 | 4.547473508865e-13 | `a27dee465fa17ffb` | `a5bbbb2372fed5d1` | `f4e6ecb3d7bebff4` | 0.084 |
| 1 | 84 | 11189 | 1048 | -51.176588935012 | -51.176588935014 | -1862.500000000000 | 1811.323411064979 | 1.854516540334e-12 | 4.547473508865e-13 | `a27dee465fa17ffb` | `a5bbbb2372fed5d1` | `f4e6ecb3d7bebff4` | 0.085 |

### Representative Elevation Ledger Records

| Record | Class | Mark | Plate | Other | Triangle | Vertex | Global sample | Previous km | New km | Delta km | Signed velocity |
|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 0 | trench_visible_elevation_delta | 0 | 1 | 0 | 250 | 555 | 6769 | 2.500000 | -10.000000 | -12.500000000000 | 0.068070912065 |
| 1 | trench_visible_elevation_delta | 0 | 1 | 0 | 250 | 556 | 6714 | 2.500000 | -10.000000 | -12.500000000000 | 0.068070912065 |
| 2 | trench_visible_elevation_delta | 0 | 1 | 0 | 250 | 557 | 6858 | 2.500000 | -10.000000 | -12.500000000000 | 0.068070912065 |
| 3 | trench_visible_elevation_delta | 1 | 1 | 0 | 1706 | 2155 | 6803 | 2.500000 | -10.000000 | -12.500000000000 | 0.066856551625 |
| 4 | trench_visible_elevation_delta | 1 | 1 | 0 | 1706 | 2156 | 6947 | 2.500000 | -10.000000 | -12.500000000000 | 0.066856551625 |
| 5 | trench_visible_elevation_delta | 2 | 1 | 0 | 1735 | 2153 | 6625 | 2.500000 | -10.000000 | -12.500000000000 | 0.068565989154 |
| 6 | trench_visible_elevation_delta | 3 | 1 | 0 | 1750 | 2178 | 6536 | 2.500000 | -10.000000 | -12.500000000000 | 0.070616024043 |
| 7 | trench_visible_elevation_delta | 3 | 1 | 0 | 1750 | 2180 | 6303 | 2.500000 | -10.000000 | -12.500000000000 | 0.070616024043 |
| 8 | trench_visible_elevation_delta | 3 | 1 | 0 | 1750 | 2190 | 6392 | 2.500000 | -10.000000 | -12.500000000000 | 0.070616024043 |
| 9 | trench_visible_elevation_delta | 4 | 1 | 0 | 1775 | 2200 | 6481 | 2.500000 | -10.000000 | -12.500000000000 | 0.069510215478 |
| 10 | trench_visible_elevation_delta | 5 | 1 | 0 | 1805 | 2220 | 5426 | 2.500000 | -10.000000 | -12.500000000000 | 0.074931574407 |
| 11 | trench_visible_elevation_delta | 5 | 1 | 0 | 1805 | 2206 | 5570 | 2.500000 | -10.000000 | -12.500000000000 | 0.074931574407 |
| 12 | trench_visible_elevation_delta | 5 | 1 | 0 | 1805 | 2221 | 5337 | 2.500000 | -10.000000 | -12.500000000000 | 0.074931574407 |
| 13 | trench_visible_elevation_delta | 6 | 1 | 0 | 1807 | 2222 | 6070 | 2.500000 | -10.000000 | -12.500000000000 | 0.072566014470 |
| 14 | trench_visible_elevation_delta | 6 | 1 | 0 | 1807 | 2201 | 6214 | 2.500000 | -10.000000 | -12.500000000000 | 0.072566014470 |
| 15 | trench_visible_elevation_delta | 6 | 1 | 0 | 1807 | 2219 | 5981 | 2.500000 | -10.000000 | -12.500000000000 | 0.072566014470 |
| 16 | trench_visible_elevation_delta | 8 | 1 | 0 | 1819 | 2228 | 5748 | 2.500000 | -10.000000 | -12.500000000000 | 0.074009601051 |
| 17 | trench_visible_elevation_delta | 8 | 1 | 0 | 1819 | 2175 | 5892 | 2.500000 | -10.000000 | -12.500000000000 | 0.074009601051 |
| 18 | trench_visible_elevation_delta | 8 | 1 | 0 | 1819 | 2177 | 5659 | 2.500000 | -10.000000 | -12.500000000000 | 0.074009601051 |
| 19 | trench_visible_elevation_delta | 9 | 1 | 0 | 1857 | 2233 | 5248 | 2.500000 | -10.000000 | -12.500000000000 | 0.075252952098 |

## Controls

| Fixture | Marks | Trench records | Uplift records | Ledger records | Actual delta km | Ledger delta km | Residual km | Closure matches | Result |
|---|---:|---:|---:|---:|---:|---:|---:|---|---|
| trench_only | 84 | 149 | 0 | 149 | -1862.500000000000 | -1862.500000000000 | 0.000000000000e+00 | yes | pass |
| elevation_mutations_disabled | 84 | 0 | 0 | 0 | 0.000000000000 | 0.000000000000 | 0.000000000000e+00 | yes | pass |
| slab_pull_only | 84 | 0 | 0 | 0 | 0.000000000000 | 0.000000000000 | 0.000000000000e+00 | yes | pass |
| zero_motion | 0 | 0 | 0 | 0 | 0.000000000000 | 0.000000000000 | 0.000000000000e+00 | yes | pass |
| single_plate | 0 | 0 | 0 | 0 | 0.000000000000 | 0.000000000000 | 0.000000000000e+00 | yes | pass |
| forced_divergence_no_subduction | 0 | 0 | 0 | 0 | 0.000000000000 | 0.000000000000 | 0.000000000000e+00 | yes | pass |

## Scope Notes

- The Phase II material ledger categories and `MaterialLedgerHash` remain unchanged; IIIC.5 adds a separate elevation-ledger audit for process-state deltas.
- Slab pull is intentionally excluded from the elevation ledger because it mutates motion authority, not elevation or material.
- The ledger sums plate-local vertices, preserving carrier authority. It does not read projected global samples as authority.
- This checkpoint may claim only IIIC.5 elevation accounting. It does not claim Stage 1.5 carrier success, Slice 5.5 asymmetry resolution, collision, rifting, erosion, slab-pull correctness beyond its exclusion from the elevation ledger, or terrain morphology.

## Recommendation

IIIC.5 passes. Pause for user review before IIIC consolidation.
