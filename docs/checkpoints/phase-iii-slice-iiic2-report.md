# Phase III Slice IIIC.2 Checkpoint

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIC2/20260502T220216Z`

Status: opt-in visible/historical elevation split for IIIC subducting triangles. On first marking, the under-plate triangle snapshots visible `Elevation` into `HistoricalElevation`, then visible `Elevation` is set to paper Table 3.2 trench depth `z_t = -10 km`. This slice does not add uplift, slab pull, collision, rifting, erosion, terrain displacement, global ownership, or any new resampling mutation path.

## Gate Summary

| Gate | Result | Evidence |
|---|---|---|
| Bypass disabled | pass | Slice 5.5 fixed fixture state `3b4a85366dab80db` / `3b4a85366dab80db`, ledger `bc3077100ba291b4` / `bc3077100ba291b4` |
| Snapshot and trench depth | pass | snapshots 84 / 84, visible -10.000..-10.000 km, historical 2.500..2.500 km |
| Independent elevation hashes | pass | visible `7254f086b9a0e33a` / `7254f086b9a0e33a`, historical `c1c1cc9d2253fc21` / `c1c1cc9d2253fc21` |
| Opt-in disabled | pass | marks 84, snapshots 0, missing snapshots 84 |
| Remesh reset | pass | before marks 84/snapshots 84, persistent mark inputs 84, after marks 0/snapshots 0, reset serial 0 -> 1 |

## Bypass Gate

With both IIIC marks and the IIIC.2 elevation split disabled, the fixed Slice 5.5 replay must continue to hit the accepted baseline hashes. This is a fixed-fixture regression, not a global no-mutation proof.

| Replay | State hash | Expected state | Material ledger hash | Expected ledger | Persistent mark inputs | Seconds |
|---:|---|---|---|---|---:|---:|
| 0 | `3b4a85366dab80db` | `3b4a85366dab80db` | `bc3077100ba291b4` | `bc3077100ba291b4` | 0 | 3.217 |
| 1 | `3b4a85366dab80db` | `3b4a85366dab80db` | `bc3077100ba291b4` | `bc3077100ba291b4` | 0 | 3.185 |

## Elevation Split Replay

Fixture: two plates under forced convergence, plate 0 continental, plate 1 oceanic. Plate 1 visible elevation is seeded to 2.500 km before marking. The expected first snapshot is 2.500 km historical elevation and -10.000 km visible trench elevation.

| Replay | Step | Marks | Snapshots | Missing | Duplicate | Invalid | Snapshot vertices | Visible min/max km | Historical min/max km | Visible hash | Historical hash | Crust hash |
|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|---|---|
| 0 | 1 | 84 | 84 | 0 | 0 | 0 | 252 | -10.000 / -10.000 | 2.500 / 2.500 | `7254f086b9a0e33a` | `c1c1cc9d2253fc21` | `09685424999ed030` |
| 1 | 1 | 84 | 84 | 0 | 0 | 0 | 252 | -10.000 / -10.000 | 2.500 / 2.500 | `7254f086b9a0e33a` | `c1c1cc9d2253fc21` | `09685424999ed030` |

### Representative Elevation Records

| Mark | Plate | Other | Local triangle | Snapshot vertices | Historical min/max km | Visible min/max km | Applied trench km |
|---:|---:|---:|---:|---:|---|---|---:|
| 0 | 1 | 0 | 250 | 3 | 2.500 / 2.500 | -10.000 / -10.000 | -10.000 |
| 1 | 1 | 0 | 1706 | 3 | 2.500 / 2.500 | -10.000 / -10.000 | -10.000 |
| 2 | 1 | 0 | 1735 | 3 | 2.500 / 2.500 | -10.000 / -10.000 | -10.000 |
| 3 | 1 | 0 | 1750 | 3 | 2.500 / 2.500 | -10.000 / -10.000 | -10.000 |
| 4 | 1 | 0 | 1775 | 3 | 2.500 / 2.500 | -10.000 / -10.000 | -10.000 |
| 5 | 1 | 0 | 1805 | 3 | 2.500 / 2.500 | -10.000 / -10.000 | -10.000 |
| 6 | 1 | 0 | 1807 | 3 | 2.500 / 2.500 | -10.000 / -10.000 | -10.000 |
| 7 | 1 | 0 | 1808 | 3 | 2.500 / 2.500 | -10.000 / -10.000 | -10.000 |
| 8 | 1 | 0 | 1819 | 3 | 2.500 / 2.500 | -10.000 / -10.000 | -10.000 |
| 9 | 1 | 0 | 1857 | 3 | 2.500 / 2.500 | -10.000 / -10.000 | -10.000 |
| 10 | 1 | 0 | 1860 | 3 | 2.500 / 2.500 | -10.000 / -10.000 | -10.000 |
| 11 | 1 | 0 | 1862 | 3 | 2.500 / 2.500 | -10.000 / -10.000 | -10.000 |
| 12 | 1 | 0 | 1904 | 3 | 2.500 / 2.500 | -10.000 / -10.000 | -10.000 |
| 13 | 1 | 0 | 1913 | 3 | 2.500 / 2.500 | -10.000 / -10.000 | -10.000 |
| 14 | 1 | 0 | 1956 | 3 | 2.500 / 2.500 | -10.000 / -10.000 | -10.000 |
| 15 | 1 | 0 | 1981 | 3 | 2.500 / 2.500 | -10.000 / -10.000 | -10.000 |

## Negative And Reset Controls

| Control | Marks | Snapshots before | Snapshots after | Missing | Persistent mark inputs | Reset serial | Result |
|---|---:|---:|---:|---:|---:|---|---|
| elevation split disabled | 84 | 0 | n/a | 84 | 0 | 0 | pass |
| filtered resample reset | 84 | 84 | 0 | 0 | 84 | 0 -> 1 | pass |

## Scope Notes

- The historical snapshot is plate-local/per-window process state attached to IIIC subducting triangle marks. Duplicate mark evidence does not re-snapshot an existing triangle.
- `VisibleElevationHash` and `HistoricalElevationHash` are independent field hashes. The former sees the trench depth; the latter sees the pre-trench value that IIIC.3 uplift will read.
- The actor elevation heatmap now reads projected plate-local visible elevation for display, without making projected output authoritative.
- This checkpoint may claim only IIIC.2 visible/historical elevation behavior. It does not claim uplift, slab-pull, collision, rifting, erosion, Stage 1.5 carrier success, or Slice 5.5 asymmetry resolution.

## Recommendation

IIIC.2 passes. Pause for user review before IIIC.3 overriding-plate uplift work.
