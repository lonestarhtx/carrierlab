# Phase II Slice 0 Checkpoint: Baseline Performance Validation

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseII/Slice0/target_20260501`

This checkpoint validates the optimized actor path only: timing instrumentation, persistent render mesh/color updates, cached plate topology, combined projection BVH, and deterministic parallel per-sample ray queries. No subduction contacts, labels, filters, material mutation, or new resampling behavior is introduced.

Benchmark configuration: 40 plates, seed 42, continental plate fraction 0.30, centroid multi-hit policy, velocity 66.6666666667 mm/y, no resampling, `3` measured rigid steps per replay plus initialization. Each resolution ran two same-seed replays.

Important caveat: exact pre-optimization actor hashes/timings were not captured before Commit A added timing instrumentation. The `before` column therefore remains `not instrumented` except for the informal user-observed 250k actor cost of roughly 3 seconds/step. This run establishes the optimized baseline and same-seed replay gate for future Phase II work.

## Timing Summary

| Resolution | Before actor total | Optimized kernel s | Optimized actor total s | BVH s | Query s | Drift s | Boundary s | Hash s | Render s | Paper Table 2 target | Gate |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 60000 | not instrumented | 0.032184 | 0.039899 | 0.019342 | 0.009141 | 0.002869 | 0.000446 | 0.001335 | 0.004919 | 0.19 | pass |
| 100000 | not instrumented | 0.053333 | 0.065561 | 0.032868 | 0.014609 | 0.004643 | 0.000706 | 0.002135 | 0.008294 | 0.28 | pass |
| 250000 | ~3.000 informal | 0.138801 | 0.168566 | 0.084397 | 0.038224 | 0.013142 | 0.001812 | 0.005251 | 0.020434 | 1.24 | pass |
| 500000 | not instrumented | 0.290706 | 0.351891 | 0.173272 | 0.083475 | 0.028326 | 0.003345 | 0.010404 | 0.043571 | 1.90 | pass |

## Determinism

| Resolution | Projection hash A | Projection hash B | State hash A | State hash B | All step hashes match | Gate |
|---:|---|---|---|---|---|---|
| 60000 | `91beacf7230a3e77` | `91beacf7230a3e77` | `40500beb9765c597` | `40500beb9765c597` | yes | pass |
| 100000 | `51be90ed71be0ad9` | `51be90ed71be0ad9` | `b330103d274c56fe` | `b330103d274c56fe` | yes | pass |
| 250000 | `466540d53b3a6845` | `466540d53b3a6845` | `9d4467ba86718bcb` | `9d4467ba86718bcb` | yes | pass |
| 500000 | `ddaa8c04ed0751a1` | `ddaa8c04ed0751a1` | `f8c95117a3f710dc` | `f8c95117a3f710dc` | yes | pass |

## Final-Step Metrics

| Resolution | Miss % | Multi-hit % | Auth CAF | Projected CAF | Drift p95 km | Memory GB |
|---:|---:|---:|---:|---:|---:|---:|
| 60000 | 8.788333 | 8.690000 | 0.301050 | 0.270565 | 0.000094935298 | 1.479 |
| 100000 | 8.896000 | 8.749000 | 0.301040 | 0.270393 | 0.000094935298 | 1.545 |
| 250000 | 8.843200 | 8.705600 | 0.301056 | 0.270753 | 0.000094935298 | 1.763 |
| 500000 | 8.851800 | 8.707800 | 0.301078 | 0.270883 | 0.000094935298 | 2.155 |

## Interpretation

The actor path now has measurement points for the expensive surfaces that matter before Phase II contact detection: combined projection BVH build/refit, ray query, drift metrics, boundary mask, hash, and render/color update. This is still a no-mutation Slice 0 surface; global samples remain projection output, not process authority.

The before/after requirement is only partially closed because pre-optimization actor hashes were not recorded before the optimization series. The current optimized path is replay-deterministic within this run, which makes it a valid baseline for later Phase II slices. Treat the missing exact pre-optimization hash comparison as a documentation gap, not as permission to loosen future optimization gates.

## Recommendation

Conditional go for Phase II Slice 1 after user review: the optimized actor path is deterministic across same-seed replay. Any resolution marked `investigate` in the timing table should be profiled before adding contact detection at that resolution.
