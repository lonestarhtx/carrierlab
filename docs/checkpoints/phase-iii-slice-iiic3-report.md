# Phase III Slice IIIC.3 Checkpoint

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIC3/20260502T222256Z`

Status: opt-in overriding-plate uplift. This slice applies thesis section 3.3.1.3 uplift to continental over-plate vertices near IIIC subducting marks, using IIIC.2 `HistoricalElevation` on the under-plate triangle. It does not add slab pull, collision, rifting, erosion, terrain displacement, global ownership, or any new resampling mutation path.

Formula: `Delta z = u0 * dt * exp(3d/rs) * exp(-9d^2/rs^2) * clamp(v/v0,0,1) * ztilde^2`, with `u0=0.6 mm/yr`, `dt=2 Ma`, `rs=1800 km`, `v0=100 mm/yr`, `zt=-10 km`, `zc=10 km`, and `ztilde=(HistoricalElevation-zt)/(zc-zt)`.

## Gate Summary

| Gate | Result | Evidence |
|---|---|---|
| Bypass disabled | pass | Slice 5.5 fixed fixture state `3b4a85366dab80db` / `3b4a85366dab80db`, ledger `bc3077100ba291b4` / `bc3077100ba291b4` |
| Forced convergence uplift | pass | records 11040 / 11040, unique vertices 899 / 899, total delta 1811.323411064979 / 1811.323411064979 km |
| Independent formula oracle | pass | oracle residual 6.821210263297e-13 / 6.821210263297e-13 km, max record residual 2.775557561563e-16 / 2.775557561563e-16 km |
| Same-seed replay | pass | uplift hash `74b54dd1f052ced4` / `74b54dd1f052ced4`, visible hash `a5bbbb2372fed5d1` / `a5bbbb2372fed5d1` |
| Uplift opt-in disabled | pass | marks 84, uplift records 0, total delta 0.000000000000 km |
| Negative controls | pass | zero 0, single 0, divergence-no-subduction 0 uplift records |

## Primary Forced-Convergence Replay

| Replay | Marks | Snapshots | Uplift records | Unique vertices | Total delta km | Oracle delta km | Max delta km | Min fold | Uplift hash | Visible hash | Crust hash | Seconds |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|---:|
| 0 | 84 | 84 | 11040 | 899 | 1811.323411064979 | 1811.323411064978 | 0.601886804423 | 1.000000000000 | `74b54dd1f052ced4` | `a5bbbb2372fed5d1` | `b89051dccbd392c8` | 0.080 |
| 1 | 84 | 84 | 11040 | 899 | 1811.323411064979 | 1811.323411064978 | 0.601886804423 | 1.000000000000 | `74b54dd1f052ced4` | `a5bbbb2372fed5d1` | `b89051dccbd392c8` | 0.081 |

## Representative Uplift Records

| Mark | Under | Over | Under tri | Over vertex | Distance km | Signed velocity | Historical km | Delta km | Distance f | Speed g | Relief h | Fold |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 0 | 1 | 0 | 250 | 28 | 1765.560 | 0.068070912065 | 2.500 | 0.001543073210 | 0.003292 | 1.000000 | 0.390625 | 1.000000 |
| 0 | 1 | 0 | 250 | 29 | 1783.895 | 0.068070912065 | 2.500 | 0.001327847863 | 0.002833 | 1.000000 | 0.390625 | 1.000000 |
| 0 | 1 | 0 | 250 | 780 | 785.385 | 0.068070912065 | 2.500 | 0.312819959082 | 0.667349 | 1.000000 | 0.390625 | 1.000000 |
| 0 | 1 | 0 | 250 | 781 | 833.290 | 0.068070912065 | 2.500 | 0.273164660356 | 0.582751 | 1.000000 | 0.390625 | 1.000000 |
| 0 | 1 | 0 | 250 | 782 | 623.184 | 0.068070912065 | 2.500 | 0.450310152588 | 0.960662 | 1.000000 | 0.390625 | 1.000000 |
| 0 | 1 | 0 | 250 | 846 | 1220.091 | 0.068070912065 | 2.500 | 0.057311077879 | 0.122264 | 1.000000 | 0.390625 | 1.000000 |
| 0 | 1 | 0 | 250 | 847 | 1140.873 | 0.068070912065 | 2.500 | 0.084435849617 | 0.180130 | 1.000000 | 0.390625 | 1.000000 |
| 0 | 1 | 0 | 250 | 848 | 932.046 | 0.068070912065 | 2.500 | 0.198422368988 | 0.423301 | 1.000000 | 0.390625 | 1.000000 |
| 0 | 1 | 0 | 250 | 1122 | 598.694 | 0.068070912065 | 2.500 | 0.469769538403 | 1.002175 | 1.000000 | 0.390625 | 1.000000 |
| 0 | 1 | 0 | 250 | 1123 | 894.068 | 0.068070912065 | 2.500 | 0.225821819175 | 0.481753 | 1.000000 | 0.390625 | 1.000000 |
| 0 | 1 | 0 | 250 | 1124 | 704.338 | 0.068070912065 | 2.500 | 0.382195946112 | 0.815351 | 1.000000 | 0.390625 | 1.000000 |
| 0 | 1 | 0 | 250 | 1128 | 393.961 | 0.068070912065 | 2.500 | 0.587305617633 | 1.252919 | 1.000000 | 0.390625 | 1.000000 |
| 0 | 1 | 0 | 250 | 1136 | 543.196 | 0.068070912065 | 2.500 | 0.510699027100 | 1.089491 | 1.000000 | 0.390625 | 1.000000 |
| 0 | 1 | 0 | 250 | 1137 | 1567.923 | 0.068070912065 | 2.500 | 0.006920126032 | 0.014763 | 1.000000 | 0.390625 | 1.000000 |
| 0 | 1 | 0 | 250 | 1138 | 1622.347 | 0.068070912065 | 2.500 | 0.004677863636 | 0.009979 | 1.000000 | 0.390625 | 1.000000 |
| 0 | 1 | 0 | 250 | 1139 | 232.194 | 0.068070912065 | 2.500 | 0.594248958715 | 1.267731 | 1.000000 | 0.390625 | 1.000000 |

## Negative Controls

| Fixture | Marks | Snapshots | Uplift records | Total delta km | Uplift hash | Visible hash | Result |
|---|---:|---:|---:|---:|---|---|---|
| uplift_disabled | 84 | 84 | 0 | 0.000000000000 | `` | `7254f086b9a0e33a` | pass |
| zero_motion | 0 | 0 | 0 | 0.000000000000 | `c941c3735bf40d22` | `d73408457d40f43a` | pass |
| single_plate | 0 | 0 | 0 | 0.000000000000 | `c941c3735bf40d22` | `a84052afa5482db5` | pass |
| forced_divergence_no_subduction | 0 | 0 | 0 | 0.000000000000 | `c941c3735bf40d22` | `a7fa6b3539ddb63a` | pass |

## Non-Gate Closed-Sphere Divergence Diagnostic

A two-plate forced-divergence mixed-material fixture can still produce backside local convergence after motion on a closed sphere. IIIC.3 keeps that evidence visible, but does not use it as the no-uplift negative gate because the IIIB mark path is correctly local, not pair-global.

| Fixture | Marks | Snapshots | Uplift records | Total delta km | Uplift hash | Visible hash |
|---|---:|---:|---:|---:|---|---|
| forced_divergence_mixed_backside_diagnostic | 80 | 80 | 10557 | 1674.166262624544 | `bea1c0459801608d` | `b2d2cc6ac82ac5a2` |

## Scope Notes

- Uplift mutates plate-local over-plate vertices only; global TDS samples remain projection/resampling targets, not persistent authority.
- The independent oracle recomputes the uplift formula from raw record distance, signed convergence velocity, and historical elevation fields.
- The speed transfer is clamped to `[0,1]` because thesis Table 3.2 defines `v0` as the maximum authorized plate speed and Figure 37 normalizes `g(v)` at `v0`.
- This checkpoint may claim only IIIC.3 overriding-plate uplift behavior. It does not claim slab pull, collision, rifting, erosion, Stage 1.5 carrier success, or Slice 5.5 asymmetry resolution.

## Recommendation

IIIC.3 passes. Pause for user review before IIIC.4 slab-pull work.
