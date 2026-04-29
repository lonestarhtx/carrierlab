# Stage 0 Checkpoint Report

Status: ready for user review. Stage advancement is paused pending explicit go/no-go.

## Scope

Stage 0 is cold-start carrier only: deterministic Fibonacci samples, clean-room spherical Delaunay, duplicated plate-local triangulations, ray-from-origin projection, no motion, no mutation, no resampling.

Implementation notes:

- Triangulation now uses Unreal `UE::Geometry::FConvexHull3d`. For unit-sphere points, the 3D convex hull facets are the spherical Delaunay triangulation. This replaces the provisional hand-rolled Bowyer-Watson audit path.
- Projection uses ray-from-origin triangle intersection against plate-local duplicated triangles.
- Stage 0 ray acceleration uses an exact per-sample incident plate-local triangle candidate index. This is equivalent for cold-start because target samples are the source TDS vertices, so every valid ray hit must be in the sample's incident triangle fan.
- This Stage 0 accelerator is not a substitute for Stage 1 moving-geometry queries. Stage 1 should add or validate a general moving-mesh spatial index such as `TMeshAABBTree3`.
- Third-plate intrusion is classified separately from two-plate boundary-degenerate overlap.

## Commands

Build:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' CarrierLabEditor Win64 Development 'C:\Users\Michael\Documents\Unreal Projects\CarrierLab\CarrierLab.uproject'
```

Automation audit:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\Michael\Documents\Unreal Projects\CarrierLab\CarrierLab.uproject' -unattended -nop4 -nosplash -NullRHI -NoSound -stdout -FullStdOutLogOutput -ExecCmds="Automation RunTests CarrierLab.Stage0; Quit" -abslog="C:\Users\Michael\Documents\Unreal Projects\CarrierLab\Saved\Logs\CarrierLabStage0Tests.log"
```

Checkpoint run:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\Michael\Documents\Unreal Projects\CarrierLab\CarrierLab.uproject' -run=CarrierLabStage0 '-Resolutions=60000,100000,250000,500000' -Out='C:\Users\Michael\Documents\Unreal Projects\CarrierLab\Saved\CarrierLab\Stage0\target_20260428' -unattended -nop4 -nosplash -NullRHI -NoSound -stdout -FullStdOutLogOutput -abslog='C:\Users\Michael\Documents\Unreal Projects\CarrierLab\Saved\Logs\CarrierLabStage0Target.log'
```

## Gate Results

| Gate | Result |
|---|---|
| Every global sample resolved to a plate | PASS at 60k, 100k, 250k, 500k |
| Raw misses | PASS: 0 at every resolution |
| Non-degenerate misses | PASS: 0 at every resolution |
| Non-degenerate overlaps | PASS: 0 at every resolution |
| Third-plate non-degenerate intrusions | PASS: 0 at every resolution |
| Third-plate boundary intrusions classified separately | PASS |
| CAF authoritative equals projected | PASS, exact to reported precision |
| Euler characteristic | PASS: 2 at every resolution |
| Determinism replay hash | PASS at every resolution |
| Runtime and memory budget | PASS by wide margin |

## Metrics

Memory is `FPlatformMemory::GetStats().UsedPhysical` sampled at checkpoint time, not peak process RSS.

| Resolution | Tris | Raw hit | Raw miss | Raw multi | 2-plate boundary | 3-plate boundary | 3-plate nondeg | CAF | Kernel s | Total s | Memory GiB | Hash pair |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| 60k | 119,996 | 60,000 | 0 | 4,910 | 4,828 | 82 | 0 | 0.301050 | 0.033716 | 0.252433 | 1.42 | `d0665da2cf12b6ec` / `d0665da2cf12b6ec` |
| 100k | 199,996 | 100,000 | 0 | 5,845 | 5,761 | 84 | 0 | 0.301040 | 0.058617 | 0.435981 | 1.47 | `47db6a846265525f` / `47db6a846265525f` |
| 250k | 499,996 | 250,000 | 0 | 9,777 | 9,701 | 76 | 0 | 0.301056 | 0.152837 | 1.160604 | 1.54 | `d1fe99d4066e0b0f` / `d1fe99d4066e0b0f` |
| 500k | 999,996 | 500,000 | 0 | 13,780 | 13,692 | 88 | 0 | 0.301078 | 0.317640 | 2.514690 | 1.73 | `fe32cee2be23d69d` / `fe32cee2be23d69d` |

Acceleration evidence:

| Resolution | Ray candidates | Ray tests | Naive sample x tri tests |
|---:|---:|---:|---:|
| 60k | 359,988 | 359,988 | 7,199,760,000 |
| 100k | 599,988 | 599,988 | 19,999,600,000 |
| 250k | 1,499,988 | 1,499,988 | 124,999,000,000 |
| 500k | 2,999,988 | 2,999,988 | 499,998,000,000 |

## Aurous Comparison Rows

No parameter-matched Aurous Stage 0 cold-start baseline is available.

| Metric | Lab value | Aurous value | Aurous run id | Parameters match |
|---|---:|---|---|---|
| 60k raw miss rate | 0.000000 | no baseline available | no Stage 0 cold-start baseline | false |
| 60k raw multi-hit rate | 0.081833 | no baseline available | no Stage 0 cold-start baseline | false |
| 100k raw miss rate | 0.000000 | no baseline available | no Stage 0 cold-start baseline | false |
| 100k raw multi-hit rate | 0.058450 | no baseline available | no Stage 0 cold-start baseline | false |
| 250k raw miss rate | 0.000000 | no baseline available | no Stage 0 cold-start baseline | false |
| 250k raw multi-hit rate | 0.039108 | no baseline available | no Stage 0 cold-start baseline | false |
| 500k raw miss rate | 0.000000 | no baseline available | no Stage 0 cold-start baseline | false |
| 500k raw multi-hit rate | 0.027560 | no baseline available | no Stage 0 cold-start baseline | false |

Known Aurous failure-memo numbers, such as 33% miss and 24% multi-hit at 250k step 100, are not parameter-matched to Stage 0. They remain the Stage 1 comparison target.

## Exports

Metrics:

- `C:\Users\Michael\Documents\Unreal Projects\CarrierLab\Saved\CarrierLab\Stage0\target_20260428\metrics.jsonl`

Each resolution folder contains:

- `PlateId.png`
- `MissMask.png`
- `OverlapMask.png`
- `BoundaryMask.png`
- `ContinentalFraction.png`
- `ThirdPlateIntrusion.png`
- `ContactSheet.png`

Contact sheet order is: PlateId, MissMask, OverlapMask, BoundaryMask, ContinentalFraction, ThirdPlateIntrusion.

## Interpretation

Stage 0 initializes cleanly at all target resolutions. All samples receive a resolved plate, CAF is preserved, the projection has no misses, no non-degenerate overlaps, no NaN/Inf, and deterministic same-seed replay hashes match.

Raw multi-hit counts are nonzero, but all are classified as boundary phenomena: two-plate exact boundary hits or third-plate boundary junctions. The third-plate intrusion count is separate from the two-plate boundary-degenerate count and has zero non-degenerate cases at every target resolution.

## Recommendation

Go for Stage 1 only after explicit user approval. Stage 0 passes its cold-start gate.

Stage 1 must not reuse the Stage 0 incident-candidate shortcut as the sole moving-geometry query path. Once plates rotate, the implementation needs a general ray acceleration structure or an independently justified equivalent for moved plate-local meshes before comparing `step_kernel_ms` against Table 2.
