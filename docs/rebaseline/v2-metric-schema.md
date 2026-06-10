# V2 Metric Schema

Date: 2026-06-08

Status: pre-code metric contract.

Purpose: define the numeric evidence that decides V2 pass/fail before maps,
editor UI, or visual plausibility are considered.

## Metric Principles

- Numbers first, maps second, verdict last.
- Projection output is measured separately from plate-local authority.
- Every mutation must have provenance.
- Every resolver/policy must leave a count.
- Every deterministic run must have same-seed replay hashes.
- A metric passes only within the scope of the fixture/stage that produced it.

## Shared Run Metadata

Every V2 report row should include:

| Field | Type | Required | Notes |
| --- | --- | --- | --- |
| `schema_version` | string | yes | start with `v2.precode.1` |
| `run_id` | string | yes | deterministic or UUID-like |
| `stage` | string | yes | `V2-0`, `V2-1`, etc. |
| `fixture_id` | string | yes | from fixture catalog |
| `seed` | int | yes | source for deterministic construction |
| `sample_count` | int | yes | global substrate samples |
| `plate_count` | int | yes | initial plates |
| `land_fraction` | double | yes | if material initialized |
| `dt_ma` | double | yes for motion stages | timestep in Ma |
| `step_index` | int | yes | zero for cold start |
| `event_index` | int | yes if remesh/process event | zero if none |
| `source_status` | string | yes | `source_explicit`, `derived`, `lab_policy`, etc. |
| `config_hash` | hex string | yes | covers knobs and policy flags |

## Authority Metrics

| Field | Type | Stage | Pass intent |
| --- | --- | --- | --- |
| `substrate_hash` | hex | V2-0+ | stable for same seed/sample count |
| `plate_topology_hash` | hex | V2-0+ | stable for same seed/config |
| `plate_geometry_hash` | hex | V2-0+ | changes only by allowed motion/remesh |
| `material_authority_hash` | hex | V2-0+ | stable unless named process mutates material |
| `process_state_hash` | hex | V2-2+ | stable for same replay and stage |
| `projection_output_hash` | hex | V2-0+ | stable for same geometry/query |
| `editor_observation_hash` | hex | V2-5 | must not alter authority hashes |

Required replay fields:

| Field | Type | Pass condition |
| --- | --- | --- |
| `replay_match` | bool | true |
| `replay_projection_hash_a` | hex | equals B |
| `replay_projection_hash_b` | hex | equals A |
| `replay_authority_hash_a` | hex | equals B |
| `replay_authority_hash_b` | hex | equals A |

## Stage V2-0 Cold-Start Metrics

| Field | Type | Gate |
| --- | --- | --- |
| `global_sample_count` | int | equals configured sample count |
| `global_triangle_count` | int | expected spherical TDS count for fixture |
| `local_plate_vertex_count_sum` | int | >= global sample count; exact expectation fixture-dependent |
| `local_plate_triangle_count_sum` | int | equals partitioned duplicated triangle count |
| `topology_duplicate_error_count` | int | 0 unless fixture intentionally duplicates |
| `topology_hole_error_count` | int | 0 unless fixture intentionally removes |
| `raw_hit_count_total` | int | diagnostic |
| `nondegenerate_miss_count` | int | 0 for pass fixtures |
| `nondegenerate_overlap_count` | int | 0 for pass fixtures |
| `boundary_degenerate_count` | int | bounded and reported |
| `boundary_policy_selected_count` | int | diagnostic/lab-policy only |
| `projection_reads_global_owner_count` | int | 0 |
| `material_authority_projected_delta` | double | 0 or bounded by fixture |

V2-0 hard fail if:

- `projection_reads_global_owner_count > 0`
- `nondegenerate_miss_count > 0` in non-failure fixture
- `nondegenerate_overlap_count > 0` in non-failure fixture
- topology error appears outside deliberate negative control

## Stage V2-1 Rigid Motion Metrics

| Field | Type | Gate |
| --- | --- | --- |
| `analytic_motion_max_error_km` | double | <= tolerance |
| `analytic_motion_mean_error_km` | double | <= tolerance |
| `unit_length_max_error` | double | <= tolerance |
| `rotated_vector_max_error` | double | <= tolerance when vectors exist |
| `material_attachment_error_count` | int | 0 |
| `motion_step_count` | int | diagnostic |
| `raw_motion_miss_count` | int | classified, not repaired |
| `raw_motion_overlap_count` | int | classified, not repaired |
| `divergent_candidate_count` | int | directional expectation in forced divergence |
| `convergent_candidate_count` | int | directional expectation in forced convergence |
| `motion_repair_count` | int | 0 |
| `remesh_during_motion_count` | int | 0 |

V2-1 hard fail if:

- motion oracle uses mutated state as expected data
- material attachment changes without named process
- any repair/fill/remesh occurs

## Stage V2-2 Contact/Process Dry-Run Metrics

| Field | Type | Gate |
| --- | --- | --- |
| `contact_candidate_count` | int | fixture-dependent |
| `accepted_convergence_evidence_count` | int | fixture-dependent |
| `rejected_contact_count` | int | diagnostic |
| `third_plate_intrusion_count` | int | visible, not collapsed |
| `polarity_candidate_count` | int | fixture-dependent |
| `process_mutation_count` | int | 0 in dry run |
| `centroid_primary_resolution_count` | int | 0 |
| `random_primary_resolution_count` | int | 0 |
| `nearest_primary_resolution_count` | int | 0 |

V2-2 hard fail if:

- contact evidence is derived from projection owner labels only
- a resolver consumes overlap before process state exists

## Stage V2-3 Process Mutation Fixture Metrics

| Field | Type | Gate |
| --- | --- | --- |
| `subducting_triangle_mark_count` | int | fixture-dependent |
| `collision_candidate_count` | int | fixture-dependent |
| `collision_event_count` | int | fixture-dependent |
| `terrane_detach_triangle_count` | int | fixture-dependent |
| `terrane_suture_triangle_count` | int | fixture-dependent |
| `slab_pull_enabled` | bool | explicit |
| `slab_pull_axis_delta_deg` | double | reported when enabled |
| `material_destroyed_without_process_count` | int | 0 |
| `material_created_without_process_count` | int | 0 |
| `topology_mutation_without_event_count` | int | 0 |

V2-3 hard fail if:

- continental persistence is implemented as remesh repair
- process mutation lacks event provenance

## Stage V2-4 Remesh Metrics

| Field | Type | Gate |
| --- | --- | --- |
| `remesh_trigger_reason` | string | cadence/manual fixture only |
| `pretreated_plate_count` | int | diagnostic |
| `fully_subducted_plate_destroyed_count` | int | fixture-dependent |
| `filtered_subducting_hit_count` | int | fixture-dependent |
| `filtered_colliding_hit_count` | int | fixture-dependent |
| `valid_hit_after_filter_count` | int | diagnostic |
| `zero_valid_hit_count` | int | equals divergent generation candidates |
| `post_filter_unresolved_multihit_count` | int | 0 for paper-faithful pass |
| `prior_owner_fallback_count` | int | 0 |
| `q1q2_pair_missing_count` | int | 0 |
| `q1q2_same_plate_count` | int | 0 |
| `q1q2_discrete_approx_count` | int | 0 in primary paper path |
| `qgamma_error_max` | double | <= tolerance |
| `generated_oceanic_count` | int | equals accepted gap fill |
| `generated_oceanic_age_nonzero_count` | int | 0 |
| `material_created_with_process_count` | int | equals generated material area/count |
| `material_destroyed_without_process_count` | int | 0 |
| `mixed_vertex_triangle_count` | int | diagnostic |
| `mixed_vertex_unapproved_policy_count` | int | 0 |
| `rebuilt_plate_count` | int | diagnostic |
| `process_state_reset_count` | int | expected reset count |

V2-4 hard fail if:

- `prior_owner_fallback_count > 0`
- `post_filter_unresolved_multihit_count > 0` in a paper-faithful pass fixture
- `q1q2_pair_missing_count > 0`
- `q1q2_discrete_approx_count > 0` in a primary paper-faithful run
- `mixed_vertex_unapproved_policy_count > 0`

## Stage V2-5 Editor Tool Metrics

| Field | Type | Gate |
| --- | --- | --- |
| `editor_command_invocation_count` | int | diagnostic |
| `unauthorized_editor_mutation_count` | int | 0 |
| `pre_editor_authority_hash` | hex | equals post when no approved command |
| `post_editor_authority_hash` | hex | equals pre when no approved command |
| `map_without_metric_row_count` | int | 0 |
| `visual_pass_without_numeric_gate_count` | int | 0 |

V2-5 hard fail if:

- editor visualization changes authority state
- map presentation claims correctness without a metric row

## Material Ledger

Every stage that initializes, transfers, creates, or destroys material must
emit:

| Field | Type | Meaning |
| --- | --- | --- |
| `authority_continental_area_before` | double | plate-local authority |
| `authority_continental_area_after` | double | plate-local authority |
| `projected_continental_area_before` | double | projection output |
| `projected_continental_area_after` | double | projection output |
| `material_delta_total` | double | after - before by authority |
| `material_delta_explained_by_process` | double | named process contribution |
| `material_delta_unexplained` | double | must be zero/bounded |
| `generated_oceanic_area` | double | divergent generation |
| `subducted_area` | double | subduction process |
| `collided_sutured_area` | double | collision/suture process |

Hard rule:

`material_delta_unexplained` must be zero within numeric tolerance. If it is
not, the stage stops.

## Policy Ledger Metrics

Every lab policy or source-silent decision used by a run must emit:

| Field | Type |
| --- | --- |
| `policy_name` | string |
| `authority_class` | string |
| `source_citation` | string or `source_silent` |
| `policy_applied_count` | int |
| `policy_applied_area` | double |
| `primary_path` | bool |
| `user_approved` | bool |
| `replacement_target` | string |

Hard rule:

No source-silent lab policy may run on the primary paper-faithful path unless it
has explicit user approval and a checkpoint names it as lab policy.

## Verdict Vocabulary

Allowed verdicts:

- `PASS_FIXTURE`: fixture passed only.
- `PASS_STAGE`: all required fixtures/metrics for the stage passed.
- `PASS_DIAGNOSTIC`: diagnostic did what it was supposed to do; not a stage
  proof.
- `BLOCKED_SOURCE_GAP`: paper/thesis source silence prevents primary path.
- `FAIL_IMPLEMENTATION`: implementation diverges from the design/source.
- `FAIL_DESIGN`: artifact/state model contradiction.
- `NO_GO_STAGE`: do not advance.
- `NO_GO_PROJECT_PATH`: current path should be redesigned or stopped.

Disallowed verdict language:

- "works" without stage/fixture scope
- "paper-faithful" for standalone remesh without process-state filtering
- "validated foundation" when remesh/process dependencies remain unresolved
- any visual-only pass
