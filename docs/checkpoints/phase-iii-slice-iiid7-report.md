# Phase III Slice IIID.7 Report - Collision Uplift Propagation

Status: PASS. This slice applies the thesis page-60 collision uplift formula after IIID.6 detach+suture topology mutation. It does not add remeshing, rifting, erosion, terrain displacement, ownership recovery, or projection repair.

Output root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIID7/20260503T184617Z`

## Source Check

- Formula source: `docs/Synthèse de terrain à léchelle planétaire/cc5c6807-071.png` (thesis page 60, Figure 38 text).
- Radius: `r = r_c * sqrt(v(q)/v_0 * A/A_0)`.
- Uplift: `dz(p) = Delta_c * A * (1 - (d(p,R)/r)^2)^2` inside the influence region.
- Fold direction: `(n x normalize(p - q)) x n`, with `q` as terrane centroid. CarrierLab applies this as a scalar field only; vertices remain on the unit sphere.

## Gate Summary

| Gate | Result | Evidence |
|---|---:|---|
| Slice 5.5 fixed-fixture bypass | pass | state `3b4a85366dab80db` / `3b4a85366dab80db`, ledger `bc3077100ba291b4` / `bc3077100ba291b4` |
| IIIB independent signature | pass | computed `bf8818a26ed7b1dc` / `bf8818a26ed7b1dc`, expected `bf8818a26ed7b1dc` |
| Collision uplift deterministic | pass | replay `db62ba3ab24153d1` / `db62ba3ab24153d1`, records 10 / 10 |
| Formula oracle | pass | delta residual 0.000000000000000 km, record residual 0.000000000000000 km |
| Pure oceanic negative | pass | collision candidates 0, uplift records 0 |
| Lab multi-hit policy dormant | pass | policy-resolved counts 0 / 0 / 0 / 0 |

## Uplift Replays

| Fixture | Replay | Step | Topology | Records | Unique vertices | Terrane area km2 | Radius km | Center expected | Center applied | Total delta | Oracle residual | Policy multi-hits | Plan hash | Uplift hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|
| forced_collision_all_continental | 0 | 2 | applied | 10 | 10 | 204025.788764 | 184.034724 | 2.652335253931 | 2.652335253931 | 20.095280074641 | 0.000000000000000 | 0 | `230f08be1990a0ab` | `03217642a67db5ff` |
| forced_collision_all_continental | 1 | 2 | applied | 10 | 10 | 204025.788764 | 184.034724 | 2.652335253931 | 2.652335253931 | 20.095280074641 | 0.000000000000000 | 0 | `230f08be1990a0ab` | `03217642a67db5ff` |
| pure_oceanic_negative | 0 | 1 | none | 0 | 0 | 0.000000 | 0.000000 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 0.000000000000000 | 0 | `0000000000000000` | `0000000000000000` |
| pure_oceanic_negative | 1 | 1 | none | 0 | 0 | 0.000000 | 0.000000 | 0.000000000000 | 0.000000000000 | 0.000000000000 | 0.000000000000000 | 0 | `0000000000000000` | `0000000000000000` |

## Representative Records

| Vertex | Distance km | Transfer | Previous z | Delta z | New z | Fold magnitude |
|---:|---:|---:|---:|---:|---:|---:|
| 445 | 66.352455494882 | 0.756915054078 | 0.000000000000 | 2.007592482163 | 2.007592482163 | 1.000000000000 |
| 468 | 32.673951019287 | 0.937951043447 | 0.000000000000 | 2.487760618997 | 2.487760618997 | 1.000000000000 |
| 469 | 27.860897428998 | 0.954687797456 | 0.000000000000 | 2.532152101689 | 2.532152101689 | 1.000000000000 |
| 470 | 47.091031921815 | 0.873336593179 | 0.000000000000 | 2.316381434636 | 2.316381434636 | 1.000000000000 |
| 828 | 110.682327508372 | 0.407417844970 | 0.000000000000 | 1.080608713293 | 1.080608713293 | 1.000000000000 |
| 829 | 97.165910139341 | 0.520189137768 | 0.000000000000 | 1.379715988814 | 1.379715988814 | 1.000000000000 |
| 1562 | 147.813791433414 | 0.125950508241 | 0.000000000000 | 0.334062973259 | 0.334062973259 | 1.000000000000 |
| 5143 | 0.000000000000 | 1.000000000000 | 0.000000000000 | 2.652335253931 | 2.652335253931 | 1.000000000000 |
| 5144 | 0.000000000000 | 1.000000000000 | 0.000000000000 | 2.652335253931 | 2.652335253931 | 1.000000000000 |
| 5145 | 0.000094935298 | 0.999999999999 | 0.000000000000 | 2.652335253929 | 2.652335253929 | 1.000000000000 |

## Scope Notes

- IIID.7 consumes the IIID.6 dry-run/application chain and then mutates destination-plate elevation/fold scalar fields only.
- The distance-to-terrane implementation is a discrete carrier approximation: minimum geodesic distance to transferred terrane vertices and triangle barycenters. Continuous distance to the terrane polygon remains a consolidation/IIIE review item if needed.
- The pure-oceanic fixture demonstrates that accepted convergence evidence alone does not fabricate collision uplift.
- Stage 1.5 remains foundation characterization; this slice does not claim standalone remesh paper faithfulness.

Decision: PASS. IIID.7 collision uplift is accepted for the exercised fixtures. Proceed to IIID.8.
