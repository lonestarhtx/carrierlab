# Phase II Slice 1 Checkpoint: Contact Detection

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseII/Slice1/target_20260501`

This checkpoint adds geometry-derived contact detection only. It consumes raw ray-from-origin projection candidates from the current plate-local triangulations and emits deterministic contact records keyed by sample evidence, plate pair, local triangle ids, signed convergence velocity, contact class, and third-plate flag. It does not mutate material, plate topology, projection output, resampling behavior, or triangle process labels.

Signed convergence is the negative of the existing signed separation velocity. Positive values mean the two plates move toward each other at the evidence point; negative values mean separation. `subduction_candidate_count` is only the count of non-third-plate convergent contacts above the velocity margin. No triangle is filtered in Slice 1.

## Gate Summary

| Fixture | Samples | Plates | Steps | Contact hash match | Contact log byte match | No mutation | Sign gate | No-subduction gate | Convergence gate | Third-plate gate | Verdict |
|---|---:|---:|---:|---|---|---|---|---|---|---|---|
| default_60k | 60000 | 40 | 40 | pass | pass | pass | pass | pass | pass | pass | pass |
| zero_motion | 10000 | 40 | 40 | pass | pass | pass | pass | pass | pass | pass | pass |
| single_plate | 10000 | 1 | 40 | pass | pass | pass | pass | pass | pass | pass | pass |
| forced_convergence | 10000 | 2 | 40 | pass | pass | pass | pass | pass | pass | pass | pass |
| forced_divergence | 10000 | 2 | 0 | pass | pass | pass | pass | pass | pass | pass | pass |

## Contact Metrics

| Fixture | Raw evidence samples | Records | Convergent | Divergent | Low-margin | Third-plate | Subduction candidates | Boundary evidence | Contact seconds | Hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| default_60k | 15840 | 33275 | 5461 | 4007 | 0 | 23807 | 5461 | 0 | 0.138638 | `4de7d089bfb1bfbc` |
| zero_motion | 644 | 666 | 0 | 0 | 633 | 33 | 0 | 666 | 0.026625 | `00119a5d875c133b` |
| single_plate | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0.022697 | `9f210a4fabc97f8d` |
| forced_convergence | 4791 | 4791 | 1984 | 2807 | 0 | 0 | 1984 | 0 | 0.014578 | `e6a0cf34267f154d` |
| forced_divergence | 84 | 84 | 0 | 0 | 84 | 0 | 0 | 84 | 0.025953 | `46b44bc961ce0f44` |

## Directional Motion Probe

| Fixture | Signed convergence velocity replay A | Signed convergence velocity replay B | Interpretation |
|---|---:|---:|---|
| forced_convergence | 0.075341390677 | 0.075341390677 | converging |
| forced_divergence | -0.075341390677 | -0.075341390677 | separating |

## Notes

- `default_60k`: This 40-plate default fixture supplies the explicit third-plate intrusion control: third-plate evidence is emitted as third_plate records and is not folded into subduction_candidate_count.
- `forced_divergence`: Divergence gate is evaluated at step 0 to prove sign reversal without the closed-sphere backside convergence stress that belongs in later filter integration.
- Contact detection is deliberately not a process labeler. Slice 2 owns polarity and triangle labels; Slice 3 owns resampling filter integration.
- Third-plate contact records are excluded from `subduction_candidate_count` so they cannot silently become ordinary two-plate subduction.

## Recommendation

Go for user review of Phase II Slice 1. The detector is deterministic, sign-aware, non-mutating, and keeps third-plate evidence explicit. Do not advance to Slice 2 until the user records explicit go/no-go.
