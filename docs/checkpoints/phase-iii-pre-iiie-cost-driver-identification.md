# Phase III Pre-IIIE Cost Driver Identification

Status: queued. This is the next-after-containment investigation slice. No optimization has landed here.

## Initial Paper-Comparison Table

| Surface | Our measured cost | Paper Table 2 comparison | Ratio | Notes |
|---|---:|---:|---:|---|
| IIID.8 integrated total | 35.972813s/step | 0.190s/step total tectonic processes | 189.33x | replay 0 only; replay 1 skipped after failed gate |
| collision_detection_iiid1 | 0.484231s/step | 0.190s/step total tectonic processes | 2.55x | DetectPhaseIIID1ConnectedTerranes inclusive API timing |
| collision_grouping_iiid2 | 0.494453s/step | 0.190s/step total tectonic processes | 2.60x | DetectPhaseIIID2CollisionGroups inclusive API timing |
| event_selection_iiid3 | 1.658986s/step | 0.190s/step total tectonic processes | 8.73x | DetectPhaseIIID3DestinationMass inclusive API timing |
| slab_break_plan_iiid4 | 2.162657s/step | 0.190s/step total tectonic processes | 11.38x | PlanPhaseIIID4SlabBreak inclusive API timing |
| suture_plan_iiid5 | 2.154950s/step | 0.190s/step total tectonic processes | 11.34x | PlanPhaseIIID5Suture inclusive API timing |
| uplift_plan_iiid7 | 4.779112s/step | 0.190s/step total tectonic processes | 25.15x | PlanPhaseIIID7CollisionUplift inclusive API timing |
| uplift_apply_iiid7 | 9.036830s/step | 0.190s/step total tectonic processes | 47.56x | ApplyPhaseIIID7CollisionUplift inclusive API timing |
| topology_mutation_iiid6 | 4.305855s/step | 0.190s/step total tectonic processes | 22.66x | ApplyPhaseIIID6DetachAndSuture inclusive API timing on a fresh matching fixture |
| total_measured_collision_surface | 25.077074s/step | 0.190s/step total tectonic processes | 131.98x | sum of inclusive diagnostic rows; not an exclusive callgraph profile |
| IIIE oceanic crust generation | not measured | 0.580s/event | pending | activate once IIIE lands |
| IIIF plate rifting | not measured | 0.230s/event | pending | activate once IIIF lands |

## Investigation Scope

- Run existing fixtures only; do not add simulation behavior.
- Separate one-time setup from per-step process cost.
- Identify the dominant source before optimizing.
- Specific suspects: IIIB.3 matrix ray construction, IIIB.6 neighbor propagation, full audit struct construction, per-step all-plate BVH rebuilds, and setup costs amortized across short 32-step windows.

## Stop Condition

Do not normalize the current 189x integrated ratio as acceptable overhead. If the dominant cost driver cannot be isolated with existing instrumentation, write an investigation checkpoint before further integrated-performance claims.
