# Phase IIIE.6.7 / IIIE.6.9 Apply-Path Invalid Records Diagnosis

**Verdict:** PASS / PAPER-LITERAL ZERO-HIT CHECK. IIIE.6.6 closes selection multi-hit holds; IIIE.6.9 demotes the CarrierLab-invented non-positive-separation veto to an opt-in historical baseline and records generated non-positive-separation samples diagnostically.

## Exact Validation Check

The live apply path still increments `InvalidRecordCount` for true invalid vertex records and holds here. Under default IIIE.6.9 behavior, non-positive signed separation is no longer one of those invalid reasons; the restored-veto scenario below intentionally re-enables the old IIIE.4 hold for baseline reproducibility.

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

Primary invalid reasons below correspond to the individual `++InvalidRecordCount; continue;` sites before that hold. Sample unit/field columns are diagnostic anomaly flags among those invalid records. `Generated with non-positive separation` is an observability counter, not an invalid-record reason under default behavior.

## Scenario Summary

| Scenario | Result | Evidence |
|---|---|---|
| manual_step_60_paper_literal_record_builder | pass | step `60`, restore-veto `0`, selection resolved/gap/unresolved `59079/40921/0`, invalid `0`, primary sum `0`, gen/nonpos/applied/rift `40921/19512/40921/0`, noBoundary/nonsep/other `0/0/0`, invalid assigned `0`, unhandled `0`, process any/sub/obd/coll `0/0/0/0`, nonpos min/median/max `2.5217e-06/0.0171619/0.0412416`, spatial `5d72d426aeb650d7`, hashes `65b2829ee8ac8aba/228a012b2f8a4975` |
| manual_step_60_paper_literal_replay_record_builder | pass | step `60`, restore-veto `0`, selection resolved/gap/unresolved `59079/40921/0`, invalid `0`, primary sum `0`, gen/nonpos/applied/rift `40921/19512/40921/0`, noBoundary/nonsep/other `0/0/0`, invalid assigned `0`, unhandled `0`, process any/sub/obd/coll `0/0/0/0`, nonpos min/median/max `2.5217e-06/0.0171619/0.0412416`, spatial `5d72d426aeb650d7`, hashes `65b2829ee8ac8aba/228a012b2f8a4975` |
| manual_step_60_restored_veto_baseline_record_builder | pass | step `60`, restore-veto `1`, selection resolved/gap/unresolved `59079/40921/0`, invalid `19512`, primary sum `19512`, gen/nonpos/applied/rift `21409/0/21409/0`, noBoundary/nonsep/other `0/19512/0`, invalid assigned `0`, unhandled `0`, process any/sub/obd/coll `0/0/0/0`, nonpos min/median/max `0/0/0`, spatial `0232a7fe9702af6f`, hashes `65b2829ee8ac8aba/b799fc19f95f446d` |

Same-seed replay: **pass**.

## Reason Distribution

| Scenario | Invalid sample | Resolved bad plate | No boundary pair | Non-separating invalid | Generated with non-positive separation | Other generation failure | Generated bad plate | Unhandled class | Unit anomaly | Field anomaly |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| manual_step_60_paper_literal_record_builder | 0 | 0 | 0 | 0 | 19512 | 0 | 0 | 0 | 0 | 0 |
| manual_step_60_paper_literal_replay_record_builder | 0 | 0 | 0 | 0 | 19512 | 0 | 0 | 0 | 0 | 0 |
| manual_step_60_restored_veto_baseline_record_builder | 0 | 0 | 0 | 19512 | 0 | 0 | 0 | 0 | 0 | 0 |

## Time-Cost Breakdown

| Scenario | Warmup s | Selection s | Record build s | Continuous boundary-pair query s | Resolved-copy s | Validation s | Total s |
|---|---:|---:|---:|---:|---:|---:|---:|
| manual_step_60_paper_literal_record_builder | 6.429 | 0.514 | 312.813 | 312.767 | 0.003 | 0.026 | 320.368 |
| manual_step_60_paper_literal_replay_record_builder | 6.413 | 0.510 | 310.999 | 310.954 | 0.003 | 0.026 | 318.508 |
| manual_step_60_restored_veto_baseline_record_builder | 6.398 | 0.515 | 312.989 | 312.936 | 0.003 | 0.016 | 320.480 |

The diagnostic stops before topology rebuild. If `Continuous boundary-pair query s` dominates, the minutes-scale editor apply cost is in record building/gap-field validation rather than downstream topology rebuild.

## Process-State Cross-Reference

| Scenario | Invalid with any process-filtered source | Subducting | Obduction-pending | Collision-pending | Rifting-pending with any process-filtered source |
|---|---:|---:|---:|---:|---:|
| manual_step_60_paper_literal_record_builder | 0 | 0 | 0 | 0 | 0 |
| manual_step_60_paper_literal_replay_record_builder | 0 | 0 | 0 | 0 | 0 |
| manual_step_60_restored_veto_baseline_record_builder | 0 | 0 | 0 | 0 | 0 |

## Spatial Distribution

Invalid records are binned into 12 longitude by 6 latitude bands. This is diagnostic localization only, not visual approval.

| Scenario | Top bins |
|---|---|
| manual_step_60_paper_literal_record_builder |  |
| manual_step_60_paper_literal_replay_record_builder |  |
| manual_step_60_restored_veto_baseline_record_builder | lon 60..90, lat -60..-30: 1511; lon -180..-150, lat -30..0: 1473; lon 0..30, lat -30..0: 1156; lon -150..-120, lat -30..0: 990; lon -90..-60, lat -60..-30: 949; lon -30..0, lat -30..0: 779 |

## Artifacts

- JSONL metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE67ApplyPathInvalidRecords/metrics.jsonl`
- JSONL includes one `scenario` row per run and one `invalid_record` row per invalid sample with reason, process-filter counters, source selection class, and spatial bin. Default paper-literal runs may have zero `invalid_record` rows; restored-veto runs preserve the older non-separating invalid sample rows.

## Stop Conditions Preserved

- No Stage 1.5 fallback and no prior-owner retention. The only behavior change is removal of the default non-positive-separation veto from zero-hit gap generation.
- Stop if primary invalid reasons do not sum exactly to the live `InvalidRecordCount`.
- Stop if selection unresolved multi-hit reappears; that would regress IIIE.6.6 rather than diagnose apply-path records.
- Stop if invalid records correlate heavily with process-filtered candidates; that redirects the next slice toward IIIB/IIIC/IIID marking rather than continuous boundary-pair generation.

## Recommendation

Default paper-literal runs should have zero invalid records from the former `divergent_gap_nonseparating` class while reporting how many samples generated with non-positive signed separation. If any default run still has invalid records, inspect its remaining reason distribution before treating live cadence as visually unblocked. The restored-veto scenario is a historical baseline only.
