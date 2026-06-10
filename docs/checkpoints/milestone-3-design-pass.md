# CarrierLab Milestone 3 Design Pass

Date: 2026-06-09

Status: revised after Fable design review. This document is not implementation
approval.

## Purpose

Milestone 3 is the process-filter bridge between the accepted Milestone 2
carrier cycle and later tectonic material evolution.

M2 deliberately blocked nondegenerate convergent overlaps because no process
state existed yet. That was the right stop. M3 answers whether source-grounded
contact and triangle labels can be consumed by the resampling path without
reintroducing the old failure modes: global sample ownership, prior-owner
fallback, centroid/random/nearest overlap winners, repair, retention, or
unreported material rescue.

The narrow question:

Can process-state filters turn a raw cross-plate overlap into either:

- a single barycentric write from a valid remaining hit; or
- an explicit unresolved/deferred state;

without allowing unresolved convergent holes from one window to become q1/q2
oceanic gap fill in a later window?

## Source Anchors

- `docs/paper-resampling-extraction.md`: resampling casts center-to-sample rays,
  ignores subducting/colliding triangles before source selection, gap-fills
  zero-valid intersections as divergent zones, rebuilds plate-local
  triangulations, and resets process marks after remesh.
- `docs/phase-ii-subduction-design.md`: process state is a separate layer that
  owns contact evidence, polarity, triangle labels, filter decisions, event
  logs, and process hashes.
- `docs/phase-ii-pre-mortem.md`: contact labels becoming hidden ownership,
  magnitude-only convergence classification, and global plate-pair flags are
  ranked failure modes.
- `docs/checkpoints/milestone-2-closeout-report.md`: M2 provides the literal
  fixture configs and hash baselines for the filters-off regression path.
- `docs/checkpoints/milestone-3-entry-packet.md`: the mandatory tripwire is the
  hole-to-oceanic conversion pump.

## Non-Negotiable Invariants

- Plate-local triangulations remain moving material authority.
- The fixed global substrate remains a query and resampling target, never
  tectonic authority.
- Process labels are derived from current plate-local geometry and motion, not
  from previous global sample ownership.
- Subducting/colliding labels may filter candidate hits; they do not pick a
  winner directly.
- q1/q2 generation is permitted only for current divergent gaps that pass a
  stateless opening-rate predicate.
- Any raw-overlap sample that becomes zero-valid after filtering remains
  `filter-exhausted` or unresolved in M3. It does not fall through to q1/q2.
- Any post-filter multi-hit remains counted and gated as unresolved.
- Previous-blocked sample ids are diagnostic lineage only. They may detect a
  pump in fixtures and metrics, but no write/assignment path may read them.
- Process marks reset at rebuild boundaries. Their effects survive only through
  the material/topology transferred by the accepted resample output.

## Design Corrections From Review

Fable's pre-implementation review found two blockers in the initial draft. Both
are now binding design requirements.

1. A zero-raw-hit sample is not automatically q1/q2-eligible in M3. M2-created
   unresolved holes can become genuinely zero-raw in later windows. The
   filters-on path therefore requires a current divergence predicate before
   q1/q2 can generate oceanic material.
2. M3 cannot compare an `FCarrierV2Milestone3Config` hash directly to M2
   baselines because M2 hashes are seeded by `HashMilestone2Config`. The M3
   baseline gate must rerun literal M2 fixture configs through
   `FCarrierV2Milestone2::RunFixtureWithReplay` and compare against pinned
   expected constants from the hardened M2 closeout.

## Proposed Data Boundaries

M3 should reuse the current M2 carrier cycle as the execution spine and add a
small process layer around the resampling hook.

### Configuration

`FCarrierV2Milestone3Config` should make the switch explicit:

- `M2BaselineConfig`: embedded literal `FCarrierV2Milestone2Config` for
  filters-off regression checks.
- `bEnableProcessFilters`: default `false` for ordinary M2 regression rows.
- `bRequireM2PinnedBaseline`: compares literal M2 reruns against pinned
  compile-time constants.
- `bRequireHolePumpTripwire`: requires the two-window negative control.
- `bAllowFixtureSpecifiedPolarity`: enabled only for micro controls; scale
  fixtures gate this to `false`.
- `ContinentalFractionThreshold`: default `0.5` for local material class.
- `Q1Q2OpeningRateTolerance`: named tolerance for q1/q2 divergence eligibility.
- `ContactLabelDistanceRad`: bounded contact-to-triangle label distance.
- `ProcessPolicyId`: names contact, polarity, divergence, and filter policies
  together.

The disabled M3 process mode may emit empty process hashes and timing rows, but
it must not build authoritative labels or alter M2 behavior.

### Contact Evidence

`FCarrierV2M3ContactRecord`

- contact id
- resample window and source step
- ordered plate ids
- local evidence point on the unit sphere
- participating plate-local triangle ids
- signed relative velocity or convergence margin
- class: convergent, divergent, transform/low-margin, ambiguous, third-plate
- reason/provenance string

Contact evidence is derived from plate-local boundary geometry, not from global
sample overlap hit sets. The implementation should iterate plate-local boundary
triangles or boundary edges, reuse the M2 boundary-edge extraction surface, and
query other plates through the same inverse-ray/static-tree family used by M1
and M2. Global sample overlaps may corroborate contact diagnostics, but they
must not be the authority source for labels.

Gate: the same plate-local setup queried against two global substrate
resolutions must produce the same plate-local contact/label hash. Per-sample
filter-decision hashes may differ because the output substrate differs.

Fixture construction: build one plate-local state, then transplant the same
`State.Plates` into two fixture runs with different global substrate sample
counts. Compare only the plate-local contact/label hash before topology rebuild.
Do not cold-start two independent 50k/100k plate partitions for this gate; those
legitimately have different boundaries and triangle ids. Do not compare
per-sample decisions in this fixture; those are expected to differ because the
query substrates differ.

M3 uses end-of-window contact detection for its first implementation. Per-step
distance-to-front accumulation, collision distance, and full convergence
tracking are out of scope until a later milestone explicitly approves them.

### Triangle Process Labels

`FCarrierV2M3TriangleProcessLabel`

- label id
- contact id
- plate id
- plate-local triangle id
- source material evidence at the contact
- label: none, subducting, colliding, overriding, ambiguous,
  third-plate-out-of-scope
- distance/margin from contact evidence
- reason/provenance string

Only `subducting` and `colliding` are filter-active in M3. `overriding` is
provenance for the remaining source, not a direct winner. Every filter-active
label must cite exactly one local contact id and one plate-local triangle id.

### Filter Decisions

`FCarrierV2M3FilterDecision`

- sample id
- raw hit count
- raw hit plate/local-triangle ids
- filtered hit ids and filter reasons
- valid remaining hit ids
- q1/q2 boundary pair and qGamma, if a zero-raw gap is evaluated
- q1/q2 opening-rate margin
- output class: single-source-write, true-divergent-q1q2,
  deferred-nondivergent-gap, unresolved-post-filter-multihit,
  filter-exhausted, blocked-no-process, third-plate-out-of-scope
- selected source hit, if and only if exactly one valid hit remains
- q1/q2 provenance, only when output class is `true-divergent-q1q2`

The filter decision is transient per resample window. It is hashed and reported,
then consumed by topology rebuild. It is not persistent authority.

## Q1/Q2 Divergence Predicate

The thesis gap-fill algorithm applies to divergence zones. M3 adds a
CarrierLab-specific guard because M2 can intentionally create unresolved holes
that the thesis' watertight process pipeline would not create.

In the filters-on path, zero-raw q1/q2 eligibility requires:

1. a q1/q2 pair from two different plates;
2. qGamma computed from that pair;
3. current relative motion at qGamma between plate(q1) and plate(q2);
4. a positive opening rate along the q1/q2 separation direction greater than
   `Q1Q2OpeningRateTolerance`.

If the predicate fails, the sample is `deferred-nondivergent-gap`, increments
`q1q2_divergence_rejected_count`, and does not generate oceanic material.

This predicate is stateless. It reads current plate motion and current boundary
geometry, not previous sample ownership. It is the production mechanism that
prevents the hole pump; previous-blocked lineage only measures whether the
sentinel would have pumped.

## Write-Back Flow

For each global substrate sample at a resample boundary:

1. Gather raw ray/triangle hits against current moved plate-local geometry.
2. If the row is a pinned M2 baseline row, rerun the literal M2 fixture config
   through `FCarrierV2Milestone2::RunFixtureWithReplay` and compare against
   embedded expected constants.
3. If the M3 process row has filters disabled, execute the exact M2 path for
   characterization; do not claim direct hash equality against M2 unless it is
   the literal M2 rerun above.
4. If filters are enabled, classify/filter only by active labels on the hit
   plate-local triangles.
5. If exactly one valid hit remains, barycentrically write material from that
   hit and record the contact/label provenance that made other hits invalid.
6. If zero raw hits existed before filtering, compute q1/q2/qGamma and evaluate
   the q1/q2 divergence predicate. Only a passing predicate may write q1/q2
   oceanic material. A failing predicate records `deferred-nondivergent-gap`.
7. If raw hits existed but zero valid hits remain after filtering, record
   `filter-exhausted` or unresolved, and do not run q1/q2.
8. If multiple valid hits remain after filtering, record unresolved multi-hit;
   do not apply centroid, nearest, random, prior owner, or lower-id fallback.
9. Rebuild plate-local topology from the accepted sample assignments.
10. Reset process marks and emit pre-reset/post-reset counters.

The important split is now two-part:

- zero raw hits are q1/q2 candidates only when the current boundary pair is
  diverging;
- zero valid hits after filtering are not q1/q2 candidates in M3.

The second rule is a deliberate CarrierLab lab policy, not a source-explicit
thesis rule. The thesis gap-fills zero-valid intersections after filtering, but
that is safe only in the full paper pipeline where subduction/collision/terrane
transfer has already processed continental material. M3 restores the thesis
rule only after later material-evolution prerequisites exist.

Remesh-guard classification: references to q1/q2 in this document describe the
existing divergent gap-fill path, the new divergence predicate, or a
negative-control tripwire. They do not authorize q1/q2 for filtered raw
overlaps. References to repair, retention, prior-owner, centroid, nearest, and
random policies are forbidden-token declarations, not proposed implementation
paths.

## Hole-Pump Sentinel

M3 must include a fixture that fails under M2 behavior and passes only when M3
separates unresolved convergent holes from true divergent gaps.

Fixture shape:

1. Window 1 creates a cross-plate convergent overlap at at least one sample.
2. With filters disabled, that sample is blocked and contributes to unassigned
   topology, matching M2 behavior.
3. Window 2 moves the plates again.
4. The fixture measures whether the window-1 blocked sample becomes zero-raw.
5. The gate fails if that measured sample is written through q1/q2 oceanic
   generation.
6. The gate passes only if either:
   - process labels filter one hit, leaving one source-grounded write; or
   - no valid source exists and the current q1/q2 boundary pair fails the
     divergence predicate, producing `deferred-nondivergent-gap`.

The previous-blocked sample set is diagnostic lineage only. It lives in fixture
or metrics structures, not in `FCarrierV2BuildState`, and no production
write-back branch may read it to decide material, plate assignment, q1/q2
eligibility, or preferred source.

## Contact And Polarity Rules

M3 should keep polarity conservative:

- Oceanic versus continental: if one contact-local side has
  `ContinentalFraction > ContinentalFractionThreshold` and the other does not,
  the oceanic side may be labeled subducting.
- Continental versus continental: collision-candidate or unresolved; no
  subducting source in M3 unless a later approved collision model exists.
- Oceanic versus oceanic: ambiguous unless an explicit local age proxy or
  fixture polarity is supplied.
- Third-plate intrusion: separate class; no two-plate subducting/overriding
  labels in M3.
- Low-margin/transform: report only, no filter-active label.
- Forced divergence: no filter-active labels.

Polarity must be local to contact evidence. A plate may have different process
classes at different boundary regions in the same run.

Magnitude-only or absolute-dot convergence classification is a stop condition.
Signed velocity must distinguish forced convergence from forced divergence and
must flip in polarity-swap controls.

## Proposed Fixture Ladder

| fixture | purpose | hard gate |
|---|---|---|
| `M3-FX-001-PinnedM2Baseline` | literal M2 configs rerun against pinned constants | expected/current M2 hashes match |
| `M3-FX-002-FiltersOnInert` | filters enabled under zero motion create no process effect | no contacts, labels, filters, or output drift |
| `M3-FX-003-HolePumpSentinel` | measured blocked overlap cannot become q1/q2 oceanic | `previously_blocked_became_q1q2_oceanic_count == 0` and divergence rejection/source write explains it |
| `M3-FX-004-FilteredSingleSource` | labeled subducting hit is ignored and remaining hit writes material | one filtered hit, one valid source write |
| `M3-FX-005-FilterExhausted` | all raw hits filtered does not fall into q1/q2 | zero q1/q2 writes for filtered raw-overlap samples |
| `M3-FX-006-PostFilterMultihit` | multiple remaining valid hits stay unresolved | no resolver counters, unresolved count visible |
| `M3-FX-007-TrueDivergentGap` | zero-raw gap with positive opening still uses q1/q2 | q1/q2 pair, positive opening margin, provenance recorded |
| `M3-FX-008-DivergenceNoLabels` | divergent contact class emits no filter-active labels | label count zero, q1/q2 path still legal for true gaps |
| `M3-FX-009-PolaritySwap` | fixture polarity changes which side is filtered | filtered plate swaps with polarity |
| `M3-FX-010-OceanOceanAmbiguous` | ocean/ocean without age does not invent polarity | ambiguous count > 0, filter-active labels zero |
| `M3-FX-011-ContinentalCollisionCandidate` | continental/continental is not subduction | collision-candidate count > 0, subducting labels zero |
| `M3-FX-012-ThirdPlateIntrusion` | triple evidence stays out of two-plate labels | third-plate count > 0, no subducting labels |
| `M3-FX-013-SamePairMixedSignal` | local contact ids beat global pair flags | divergent region remains unfiltered |
| `M3-FX-014-ResolutionInvariantLabels` | process labels are plate-local, not substrate-owned | same pre-rebuild label hash after transplanting identical `State.Plates` into 50k and 100k substrate runs |

Scale rows:

- 50k pinned M2 baseline: required.
- 50k filters-on multi-window with resolvable O/C motion: required after the
  sentinel.
- 250k filters-on resolvable-polarity characterization/gate row: required for
  M3 closeout.
- C/C and O/O-ambiguous multi-window rows: characterization only until M4
  material evolution can resolve them.
- 500k: optional characterization only unless the user explicitly upgrades it
  to a gate.

The no-hole-growth hard gate applies only to resolvable-polarity fixtures.
Ambiguous O/O and C/C fronts may defer by design in M3; their deferred growth is
reported as an M4 dependency, not pressure to invent a resolver.

## Required Metrics

M3 closeout must report:

- filters enabled/disabled;
- M2 baseline fixture id and expected/current hashes;
- raw hit, raw overlap, true zero-hit, and filtered-zero counts;
- contact candidate counts by class;
- contact polarity counts by class;
- signed velocity min/mean/max and sign-control pass/fail;
- triangle labels by label class and plate;
- plate-local contact/label hashes and resolution-invariance hashes;
- filtered subducting hit count and filtered colliding hit count;
- filtered-overlap-to-single-source-write count;
- filter-exhausted count;
- post-filter unresolved multi-hit count;
- third-plate-out-of-scope count;
- q1/q2 evaluated count, accepted count, and `q1q2_divergence_rejected_count`;
- previously blocked became q1/q2 oceanic count, gated to zero;
- deferred overlap mass before and after filters;
- unassigned triangle count and declared budget;
- global and per-plate material delta;
- total-variation delta, labeled as gated or characterization;
- process pre-reset mark count and post-reset mark count;
- forbidden fallback counters plus source-audit note;
- `bAllowFixtureSpecifiedPolarity` state for every row;
- timing split for contact, labeling, filtering, resampling, topology rebuild,
  hashing, diagnostics, and total cycle.

## Hash Gates

M3 needs three classes of hashes:

- pinned M2 regression hashes computed by rerunning literal M2 configs through
  `FCarrierV2Milestone2::RunFixtureWithReplay`;
- filters-on process hashes covering contacts, labels, filter decisions,
  resample output, rebuilt topology, and metrics;
- plate-local contact/label hashes separated from per-sample filter-decision
  hashes, so label authority can be tested against substrate resolution.

Replay A/B remains necessary but insufficient. The pinned M2 comparison is the
cross-commit regression gate.

Minimum pinned M2 baseline set:

| fixture | post-cycle authority | resample output | rebuilt topology |
|---|---|---|---|
| `M2-FX-001` | `7511dcbb7c924c31` | `08b0a405ab894a99` | `5ef8bc66d609830c` |
| `M2-FX-002` | `8830c6e200c00b5e` | `db35fcc15c616fcc` | `fd270209c1c27ec8` |
| `M2-FX-003` | `40ba6d7c39a12365` | `c402341ca3500605` | `c2d428da806e3611` |
| `M2-FX-004` | `cca652702954f563` | `31f5083a94017b64` | `88fccb82ea808753` |
| `M2-FX-005` | `18722ad63508e3e1` | `8e0a2604cbc15eb1` | `58cb7fab0bb550ff` |
| `M2-FX-006` | `0a6967c48d78791a` | `c2975db6a07f3163` | `e075208a19f4b9de` |
| `SCALE-50K-M2` | `1e53655423b1e157` | `11b0292d7a90e121` | `02a2aa10fb13f2d6` |

The 100k, 250k, and 500k M2 hashes may be added as characterization constants,
but the minimum gate above is enough to demonstrate the M2 path remains
reachable.

## Performance Accounting

Do not reuse the old 1.24 s total-row cap as spare headroom.

Pre-registered 250k caps:

- full resample cycle with filters enabled, diagnostics excluded:
  `<= 3580 ms`, matching the M2 paper-resample-row comparison;
- contact + label + filter process lane:
  `<= 260 ms` as the nearest Table 2 subduction-row no-pathology budget;
- diagnostics/export/report generation: reported separately and not counted in
  either simulation cap.

If the process lane misses the 260 ms cap while the full cycle stays under
3580 ms, the row may be reported as characterization only, but M3 cannot claim
paper-budget headroom without an investigation note.

500k remains characterization unless explicitly promoted later.

## Lab Policies To Record

| policy | authority class | statement |
|---|---|---|
| `M3-POL-PINNED-M2-BASELINE` | diagnostic regression | literal M2 fixture configs rerun against compile-time expected hashes |
| `M3-POL-Q1Q2-DIVERGENCE-PREDICATE` | source_implicit | q1/q2 requires current positive opening at qGamma because the thesis describes divergence zones |
| `M3-POL-ZERO-VALID-DEFERRED` | source_conflict | M3 does not q1/q2 filter-exhausted raw overlaps, deliberately deferring the thesis zero-valid rule until collision/terrane material prerequisites exist |
| `M3-POL-PREVIOUS-BLOCKED-LINEAGE` | diagnostic_only | previously blocked sample ids may detect the pump but may not affect behavior |
| `M3-POL-LOCAL-CONTACT-AUTHORITY` | source_implicit | labels are keyed by local contact and plate-local triangle evidence, not global plate-pair flags |
| `M3-POL-THIRD-PLATE-OUT-OF-SCOPE` | source_silent | triple evidence is counted separately and cannot emit two-plate subducting labels in M3 |
| `M3-POL-POST-FILTER-UNRESOLVED` | source_silent | multiple valid hits after filtering remain unresolved rather than resolved by a generic tie-break |

## Stop Conditions

Pause and write an investigation note if any of these appear:

- q1/q2 writes material for a sample with raw overlap evidence in the same
  window;
- q1/q2 writes material when the current boundary pair fails the divergence
  predicate;
- a measured previously blocked convergent sample becomes oceanic without a
  named process event;
- contact or label state reads previous global owner as authority;
- contact labels differ for the same plate-local setup under different global
  substrate resolutions;
- contact evidence is derived primarily from global-sample overlap hit sets;
- filtering is keyed only by plate pair and ignores local contact ids;
- third-plate evidence emits two-plate subducting/overriding labels;
- polarity swap does not swap the filtered plate;
- forced divergence emits filter-active labels;
- convergence classification uses magnitude-only or absolute-dot velocity;
- filtered area grows like overlap area rather than contact-boundary length;
- pinned M2 hashes drift without an intentional, reviewed reason;
- forbidden resolver/fallback counters are nonzero;
- performance rows merge per-step process cost with per-window resampling cost.

## Recommended Implementation Order

1. Add the M3 harness plus the pinned literal-M2 baseline gate.
2. Add contact/label/filter record schemas and empty filters-on hashes.
3. Add boundary-triangle contact detection with signed velocity controls, but no
   write-back changes.
4. Add the q1/q2 divergence predicate and its metrics behind filters-on mode.
5. Add the hole-pump sentinel as a failing negative control.
6. Add conservative polarity and filter-active triangle labels for micro
   fixtures.
7. Wire labels into the resampling hook and demonstrate filtered single-source,
   filter-exhausted, and post-filter unresolved behavior.
8. Add resolution-invariance, same-pair mixed-signal, and scale rows.
9. Write the M3 closeout report and stop for user go/no-go.

No code should advance to M4 material evolution until this bridge either passes
or produces a clear failure checkpoint.
