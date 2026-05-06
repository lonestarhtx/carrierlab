# Phase III Slice IIID.7 Report - Collision Uplift Propagation

Status: PASS. This slice applies the thesis page-60 collision uplift formula after IIID.6 detach+suture topology mutation. It does not add remeshing, rifting, erosion, terrain displacement, ownership recovery, or projection repair.

Output root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIID7/20260506T200550Z`

## Source Check

- Formula source: `docs/Synthèse de terrain à léchelle planétaire/cc5c6807-071.png` (thesis page 60, Figure 38 text).
- Radius: `r = r_c * sqrt(v(q)/v_0) * A/A_0`.
- Uplift: `dz(p) = Delta_c * A * (1 - (d(p,R)/r)^2)^2` inside the influence region.
- Fold direction: `(n x normalize(p - q)) x n`, with `q` as terrane centroid. CarrierLab applies this as a scalar field only; vertices remain on the unit sphere.

## Gate Summary

| Gate | Result | Evidence |
|---|---:|---|
| Slice 5.5 fixed-fixture bypass | pass | state `3b4a85366dab80db` / `3b4a85366dab80db`, ledger `bc3077100ba291b4` / `bc3077100ba291b4` |
| IIIB independent signature | pass | computed `bf8818a26ed7b1dc` / `bf8818a26ed7b1dc`, expected `bf8818a26ed7b1dc` |
| Collision uplift deterministic | pass | replay `c939246033892836` / `c939246033892836`, records 3 / 3 |
| Influence-radius oracle | pass | radius 5.205288064802 / 5.205288064802 km, residual 0.000000000000e+00 / 0.000000000000e+00 km |
| Formula oracle | pass | delta residual 0.000000000000000 km, record residual 0.000000000000000 km |
| D7 input pipeline equivalence | pass | cached/uncached destination `6ed4df42165e5b39` / `6ed4df42165e5b39`, slab `6b1382c0d8553ce7` / `6b1382c0d8553ce7`, suture `eedd023eef11e867` / `eedd023eef11e867` |
| Pure oceanic negative | pass | collision candidates 0, uplift records 0 |
| Lab multi-hit policy dormant | pass | policy-resolved counts 0 / 0 / 0 / 0 |

## Uplift Replays

| Fixture | Replay | Step | Topology | Records | Unique vertices | Terrane area km2 | Radius km | Radius residual | Center expected | Center applied | Total delta | Oracle residual | Policy multi-hits | Plan hash | Uplift hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|
| forced_collision_all_continental | 0 | 2 | applied | 3 | 3 | 204025.788764 | 5.205288 | 0.000000000000e+00 | 2.652335253931 | 2.652335253931 | 7.957005760028 | 0.000000000000000 | 0 | `e0db9290b1935b8c` | `dc12a0f7785f9da6` |
| forced_collision_all_continental | 1 | 2 | applied | 3 | 3 | 204025.788764 | 5.205288 | 0.000000000000e+00 | 2.652335253931 | 2.652335253931 | 7.957005760028 | 0.000000000000000 | 0 | `e0db9290b1935b8c` | `dc12a0f7785f9da6` |
| pure_oceanic_negative | 0 | 1 | none | 0 | 0 | 0.000000 | 0.000000 | 0.000000000000e+00 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 0.000000000000000 | 0 | `0000000000000000` | `0000000000000000` |
| pure_oceanic_negative | 1 | 1 | none | 0 | 0 | 0.000000 | 0.000000 | 0.000000000000e+00 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 0.000000000000000 | 0 | `0000000000000000` | `0000000000000000` |

## D7 Input Pipeline Equivalence

| Fixture | Replay | Pass | Cached seconds | Uncached seconds | Destination hash | Slab-break hash | Suture hash |
|---|---:|---:|---:|---:|---|---|---|
| forced_collision_all_continental | 0 | PASS | 0.879865 | 1.319633 | `6ed4df42165e5b39` / `6ed4df42165e5b39` | `6b1382c0d8553ce7` / `6b1382c0d8553ce7` | `eedd023eef11e867` / `eedd023eef11e867` |
| forced_collision_all_continental | 1 | PASS | 0.879046 | 1.315772 | `6ed4df42165e5b39` / `6ed4df42165e5b39` | `6b1382c0d8553ce7` / `6b1382c0d8553ce7` | `eedd023eef11e867` / `eedd023eef11e867` |
| pure_oceanic_negative | 0 | SKIP | 0.000000 | 0.000000 | `` / `` | `` / `` | `` / `` |
| pure_oceanic_negative | 1 | SKIP | 0.000000 | 0.000000 | `` / `` | `` / `` | `` / `` |

## Representative Records

| Vertex | Distance km | Transfer | Previous z | Delta z | New z | Fold magnitude |
|---:|---:|---:|---:|---:|---:|---:|
| 5143 | 0.000000000000 | 1.000000000000 | 0.000000000000 | 2.652335253931 | 2.652335253931 | 1.000000000000 |
| 5144 | 0.000000000000 | 1.000000000000 | 0.000000000000 | 2.652335253931 | 2.652335253931 | 1.000000000000 |
| 5145 | 0.000094935298 | 0.999999999335 | 0.000000000000 | 2.652335252166 | 2.652335252166 | 1.000000000000 |

## Scope Notes

- IIID.7 consumes the IIID.6 dry-run/application chain and then mutates destination-plate elevation/fold scalar fields only.
- The distance-to-terrane implementation is a discrete carrier approximation: minimum geodesic distance to transferred terrane vertices and triangle barycenters. Continuous distance to the terrane polygon remains a consolidation/IIIE review item if needed.
- The pure-oceanic fixture demonstrates that accepted convergence evidence alone does not fabricate collision uplift.
- Stage 1.5 remains foundation characterization; this slice does not claim standalone remesh paper faithfulness.

Decision: PASS. IIID.7 collision uplift is accepted for the exercised fixtures. Proceed to IIID.8.
