# Phase IIIE.6.7 Apply-Path Invalid Records Diagnosis

**Verdict:** PASS / DIAGNOSTIC ONLY. IIIE.6.6 closes selection multi-hit holds; IIIE.6.7 classifies the later live-apply vertex-record-builder invalid-record gate and does not change remesh behavior.

## Exact Validation Check

The live apply path increments `InvalidRecordCount` in the vertex-record builder loop and then holds here:

```cpp
CurrentMetrics.PhaseIIIELastInvalidRecordCount = InvalidRecordCount;
if (InvalidRecordCount > 0)
{
    CurrentMetrics.LastRemeshMode = FString::Printf(
        TEXT("phase_iii_e6_live_hold_invalid_records_%d_gen_%d_applied_%d_rift_%d_no_boundary_%d_nonsep_%d"),
        InvalidRecordCount,
        GeneratedCandidateCount,
        AppliedGeneratedCount,
        RiftingPendingCount,
        NoBoundaryPairMissCount,
        NonSeparatingGapFillCount);
    CurrentMetrics.ResampleEventSeconds = FPlatformTime::Seconds() - StartSeconds;
    RebuildRenderMesh();
    return false;
}
```

Primary invalid reasons below correspond to the individual `++InvalidRecordCount; continue;` sites before that hold. Sample unit/field columns are diagnostic anomaly flags among those invalid records, not new behavior.

## Scenario Summary

| Scenario | Result | Evidence |
|---|---|---|
| manual_step_60_apply_record_builder | pass | step `60`, selection resolved/gap/unresolved `59079/40921/0`, invalid `19512`, primary sum `19512`, gen/applied/rift `21409/21409/0`, noBoundary/nonsep/other `0/19512/0`, invalid assigned `0`, unhandled `0`, process any/sub/obd/coll `0/0/0/0`, hashes `65b2829ee8ac8aba/f8a29257c8769c06` |
| manual_step_60_replay_apply_record_builder | pass | step `60`, selection resolved/gap/unresolved `59079/40921/0`, invalid `19512`, primary sum `19512`, gen/applied/rift `21409/21409/0`, noBoundary/nonsep/other `0/19512/0`, invalid assigned `0`, unhandled `0`, process any/sub/obd/coll `0/0/0/0`, hashes `65b2829ee8ac8aba/f8a29257c8769c06` |

Same-seed replay: **pass**.

## Reason Distribution

| Scenario | Invalid sample | Resolved bad plate | No boundary pair | Non-separating | Other generation failure | Generated bad plate | Unhandled class | Unit anomaly | Field anomaly |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| manual_step_60_apply_record_builder | 0 | 0 | 0 | 19512 | 0 | 0 | 0 | 0 | 0 |
| manual_step_60_replay_apply_record_builder | 0 | 0 | 0 | 19512 | 0 | 0 | 0 | 0 | 0 |

## Time-Cost Breakdown

| Scenario | Warmup s | Selection s | Record build s | Continuous boundary-pair query s | Resolved-copy s | Validation s | Total s |
|---|---:|---:|---:|---:|---:|---:|---:|
| manual_step_60_apply_record_builder | 6.462 | 0.517 | 315.933 | 315.881 | 0.004 | 0.016 | 323.473 |
| manual_step_60_replay_apply_record_builder | 6.353 | 0.525 | 242.745 | 242.703 | 0.004 | 0.012 | 250.184 |

The diagnostic stops before topology rebuild. If `Continuous boundary-pair query s` dominates, the minutes-scale editor apply cost is in record building/gap-field validation rather than downstream topology rebuild.

## Process-State Cross-Reference

| Scenario | Invalid with any process-filtered source | Subducting | Obduction-pending | Collision-pending | Rifting-pending with any process-filtered source |
|---|---:|---:|---:|---:|---:|
| manual_step_60_apply_record_builder | 0 | 0 | 0 | 0 | 0 |
| manual_step_60_replay_apply_record_builder | 0 | 0 | 0 | 0 | 0 |

## Spatial Distribution

Invalid records are binned into 12 longitude by 6 latitude bands. This is diagnostic localization only, not visual approval.

| Scenario | Top bins |
|---|---|
| manual_step_60_apply_record_builder | lon 60..90, lat -60..-30: 1511; lon -180..-150, lat -30..0: 1473; lon 0..30, lat -30..0: 1156; lon -150..-120, lat -30..0: 990; lon -90..-60, lat -60..-30: 949; lon -30..0, lat -30..0: 779 |
| manual_step_60_replay_apply_record_builder | lon 60..90, lat -60..-30: 1511; lon -180..-150, lat -30..0: 1473; lon 0..30, lat -30..0: 1156; lon -150..-120, lat -30..0: 990; lon -90..-60, lat -60..-30: 949; lon -30..0, lat -30..0: 779 |

## Artifacts

- JSONL metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE67ApplyPathInvalidRecords/metrics.jsonl`
- JSONL includes one `scenario` row per run and one `invalid_record` row per invalid sample with reason, process-filter counters, source selection class, and spatial bin.

## Stop Conditions Preserved

- Diagnose only: no invalid-record fix, no Stage 1.5 fallback, no remesh mutation promotion.
- Stop if primary invalid reasons do not sum exactly to the live `InvalidRecordCount`.
- Stop if selection unresolved multi-hit reappears; that would regress IIIE.6.6 rather than diagnose apply-path records.
- Stop if invalid records correlate heavily with process-filtered candidates; that redirects the next slice toward IIIB/IIIC/IIID marking rather than continuous boundary-pair generation.

## Recommendation

Use this report to choose the next implementation slice. If invalids are dominated by `divergent_gap_no_boundary_pair`, the next slice should inspect why the current-state continuous boundary-pair builder cannot find two plate frontiers for valid divergent-gap routes after motion. If invalids are dominated by `divergent_gap_nonseparating`, the next slice should inspect signed separation/ridge-direction eligibility. Do not relax the invalid-record hold until the dominant reason is fixed or explicitly approved as a named lab policy.
