# CarrierLab Milestone 4 Entry Packet

Date: 2026-06-10

Status: prepared after M3R external acceptance. Recommended verdict:
`READY_FOR_USER_GO_NO_GO_TO_IMPLEMENT_MILESTONE_4_CHARACTERIZATION_AND_FIELDS`.

## Question

Can the carrier carry the paper's crust fields and per-step convergence tracking
state without inventing persistent ownership, persistent contact ids, or terrain
mutation?

Milestone 4 exists because M3R made process filters usable, but it still left
three paper dependencies deliberately unresolved:

- ocean/ocean polarity needs real oceanic age;
- continental/continental contacts need tracked convergence distance before
  later collision transfer can be justified;
- q1/q2 generated crust needs the paper's field outputs, not just a material
  placeholder.

M4 is the field and tracking substrate for those dependencies. It does not add
uplift, erosion, slab pull, rifting, terrane transfer, plate birth/death, editor
validation, or visual amplification.

## Entry Evidence

M3R is accepted for M4 planning. The accepted M3R baseline is commit `88421d3`,
with closeout report `docs/checkpoints/milestone-3-closeout-report.md`.

External review of M3R gave `GO` and authorized M4 planning. The review carried
four watch items into this packet:

- `PreviouslyBlockedQ1Q2OceanicNonOpeningCount` is currently coupled to the
  generation predicate and should become an independent audit, or M5C must be
  named as its first live test.
- M3R hole growth is legitimate deferral debt, but an 8-window paper-regime
  characterization will likely breach the M3 budget by construction.
- M3R no longer has a per-pair mixed-signal micro fixture; M4 needs one.
- M3R has no micro fixture with genuinely opening contacts; M4 needs one
  divergent-only contact fixture.

The current branch includes a later roadmap-only commit, `5011d15`. M3R review
evidence should remain pinned to `88421d3`; M4 implementation work starts after
this packet is explicitly approved.

## Source Anchors

- `docs/paper-carrier-extraction.md`: plate-local triangulations are crust
  authority, global TDS samples are resampling targets, the timestep is 2 Ma,
  the speed scale is `v0 = 100 mm/yr`, and cadence follows the thesis formula.
- `docs/paper-resampling-extraction.md`: `r` and `f` rotate with geodetic
  motion; q1/q2/qGamma gap fill sets new oceanic fields; the subduction matrix,
  convergence-tracking lists, and `d(p)` are invalidated by remesh and rebuilt
  afterward.
- `docs/phase-iii-paper-process-extraction.md`: per-step process order applies
  geodetic rotation, rotates vector crust fields, detects convergence, and uses
  the cadence formula `DeltaT = (1-alpha)M + alpha m`.
- `docs/phase-iii-paper-process-design.md`: per-vertex/per-triangle fields are
  `z`, `a_o`, `r`, and `f`; reset-at-remesh process state includes
  convergence lists, `d(p)`, and the plate-pair subduction matrix.
- `docs/checkpoints/milestone-3-closeout-report.md`: M3R pins source-grounded
  contact evidence, auto O/C reachability, q1/q2 rejection splits,
  pump-safety metrics, and filters-on replay hashes.

## Characterization Plan Before Field Work

Before adding M4 crust fields, run a paper-regime characterization using the
existing M3R behavior only. This run is not a pass/fail proof of M4. It sets
budgets and exposes how much debt M4 is expected to pay down.

Configuration:

- sample count: 50k hard characterization row;
- plate count: 40, matching the current scale fixtures;
- timestep: 2 Ma;
- maximum speed regime: include at least one run at `v0 = 100 mm/yr`;
- cadence: speed-driven-equivalent windows in the 16-64 step range;
- minimum horizon: 8 resample windows;
- process filters: enabled, using M3R behavior only;
- M4 fields: disabled or absent;
- M5 process mutation: disabled.

Required reported metrics:

- per-window raw hits, zero-raw candidates, post-filter writes, q1/q2 accepted,
  and q1/q2 rejected by opening/process/same-plate cause;
- hole count, hole growth, unassigned triangles, and unassigned-triangle budget;
- deferred-debt classes: O/O ambiguous, C/C candidate, process-adjacent,
  filter-exhausted, post-filter multi-hit, and no-pair;
- M3R dangerous subset: previously blocked q1/q2 oceanic with non-opening
  current motion, recorded by an independent audit if available;
- contact class distribution: convergent, divergent, transform/low-margin, and
  third-plate;
- auto-polarity distribution: O/C, O/O ambiguous, C/C candidate;
- top plate-pair attribution for misses, gaps, overlaps, and deferred classes;
- runtime split: motion, contact/label/process lane, resample/filter,
  topology rebuild, diagnostics, total cycle, and peak memory;
- replay hashes for contact labels, filter decisions, resample output, topology,
  and metrics.

Expected outcome:

- the M3R hole-growth budget may breach before 8 windows;
- that breach is an expected characterization finding, not permission to loosen
  the M3R budget;
- the breach must be reported as M4 dependency evidence for age polarity,
  collision tracking, and later M5 mutation;
- if dangerous non-opening q1/q2 oceanic appears, stop and reopen M3R before
  M4 field work.

The characterization run should generate a short appendix or first section in
the M4 closeout. It does not need a separate closeout report unless it fails a
stop condition.

## Allowed Scope

Milestone 4 may add:

- per-vertex crust field storage on plate-local authority:
  - `z`: visible/current elevation field, inert except for transfer and q1/q2
    generation in M4;
  - `a_o`: oceanic age;
  - `r`: local ridge direction, tangent to the sphere;
  - `f`: local fold direction, tangent to the sphere;
- optional historical elevation storage only as an inert field if needed to
  keep later M5A interfaces honest;
- barycentric transfer of scalar and vector fields during single-source
  resampling;
- vector-field rotation with plate geodetic motion;
- tangent projection and normalization checks for `r` and `f`;
- q1/q2 oceanic field generation:
  - `a_o = 0`;
  - `r = normalize_tangent((p - qGamma) x p)`;
  - `z` from the existing q1/q2 elevation placeholder or a declared inert
    profile placeholder, without terrain claims;
  - `f` initialized to zero or a declared neutral tangent value;
- per-step convergence tracking within each resample window:
  - active boundary triangles;
  - active front records derived from current plate-local boundary geometry;
  - `d(p)` accumulation for active triangles;
  - a per-window plate-pair subduction matrix;
- O/C polarity from M3R contact-local material;
- O/O polarity from `a_o`, with equal-age or missing-age contacts deferred;
- front-continuity metrics across windows, computed from re-derived geometry;
- independent oracles for cadence, vector rotation, age advance/reset, O/O
  polarity, and `d(p)`;
- M3R pinned-baseline reruns and M3R content-stream hashes.

## Forbidden Scope

Milestone 4 must not add:

- uplift, trench elevation, erosion, dampening, sediment accretion, terrain
  amplification, or aesthetic terrain shaping;
- collision transfer, terrane extraction, suturing, slab break, or any topology
  mutation beyond the existing resampling rebuild;
- slab pull or any process feedback into plate motion;
- rifting, plate birth/death, plate-count feedback, or fragmentation;
- persistent contact, ridge, or front ids that survive rebuild as authority;
- any registry that functions like hidden sample ownership;
- prior-owner, retention, repair, backfill, hysteresis, anchoring, centroid,
  nearest, random, or lower-id fallback as a source selection policy;
- q1/q2 generation for non-opening holes;
- treating visual maps, actor output, or front continuity as authority for
  persistent identity.

## Data Boundaries

M4 should keep two lifetimes separate.

Long-lived plate-local crust fields:

- live on plate-local vertices;
- move with the plate;
- transfer by barycentric interpolation at resample;
- are included in authority hashes and replay hashes;
- may survive topology rebuild only by being written into the new plate-local
  vertices through accepted resampling records.

Short-lived process tracking state:

- is derived from current plate-local geometry and motion;
- exists only inside the current resample window;
- is invalidated at rebuild;
- may produce labels, matrices, and metrics;
- may not become persistent ownership or a persistent front/contact id.

Any field or tracking record that crosses this boundary is a stop condition
unless the entry packet is revised and approved.

## Field Semantics

### Scalars

`z` is carried and transferred in M4, but not interpreted as accepted terrain.
M4 can initialize it, interpolate it, and assign q1/q2-generated values, but it
cannot claim morphology correctness.

`a_o` is the first live material-evolution field in M4. It advances with elapsed
time for oceanic crust, resets to zero for q1/q2-generated oceanic crust, and
is unset or ignored for continental crust. The O/O polarity rule may use it only
when both sides are oceanic and both ages are valid.

### Vectors

`r` and `f` are tangent vector fields. Plate motion rotates them by the same
geodetic transform as the vertex. After any rotation, interpolation, or q1/q2
generation, the vectors must be projected back into the tangent plane at the
owning vertex and normalized when nonzero.

Neutral vector policy:

- zero-length `f` is allowed until M5A/M5B has real fold updates;
- zero-length `r` is allowed for continental or unknown-ridge material;
- nonzero `r` on generated oceanic material must align with
  `(p - qGamma) x p` within the declared angular tolerance.

## Per-Step Tracking Semantics

M4 may track convergence across the steps inside a resample window, but it does
not yet apply subduction consequences.

For each step:

1. rotate plate-local vertices and vector fields;
2. build or update the per-step contact/front evidence from current
   plate-local boundaries;
3. classify local contact using M3R sign and material rules plus M4 O/O age
   polarity;
4. initialize or advance active front records for convergent pairs;
5. accumulate `d(p)` for active triangles from raw per-step motion and front
   geometry;
6. update the per-window subduction matrix for polarity/state visibility;
7. hash the process-tracking state;
8. do not mutate `z`, transfer terranes, apply uplift, or change plate motion.

After resampling:

- active front records reset;
- the subduction matrix resets;
- `d(p)` resets;
- plate-local crust fields survive only through accepted field transfer.

## Fable Watch Item Resolution

M4 must explicitly handle the four M3R watch items.

| watch item | M4 packet disposition | M4 exit requirement |
|---|---|---|
| Coupled non-opening pump counter | Add an independent audit over per-sample records, or mark M5C as the first live test if the implementation does not yet expose enough records. | The closeout must not present the old coupled counter as discriminating if it remains coupled. |
| Hole-growth budget breach under paper-regime run | Treat breach as expected characterization debt when it comes from O/O or C/C deferral. | Report breach class-by-class; stop only for dangerous non-opening q1/q2 oceanic or unexplained debt growth. |
| Missing per-pair mixed-signal micro fixture | Add `M4-FX-MixedSignalSamePair`. | One plate pair must produce both opening and convergent local records, with filters applied only to the convergent region. |
| Missing genuinely-opening micro contact fixture | Add `M4-FX-OpeningContactOnly`. | Contact records must classify divergent-only under motion normal to the seam, without relying only on q1/q2 evidence. |

## Proposed Fixture Ladder

| fixture | purpose | hard gate |
|---|---|---|
| `M4-FX-001-M3R-PinnedBaseline` | rerun M3R baseline configs and compare content-stream hashes | expected/current hashes match |
| `M4-FX-002-FieldsInertNoop` | adding fields disabled or neutral does not change M3R decisions | M3R content hashes unchanged |
| `M4-FX-003-ScalarBarycentricTransfer` | `z` and `a_o` transfer through single-source resampling | independent barycentric oracle matches |
| `M4-FX-004-VectorRotationOracle` | `r` and `f` rotate with plate motion | independent Rodrigues/tangent oracle matches |
| `M4-FX-005-VectorInterpolationAndTangent` | interpolated vectors remain tangent and normalized when nonzero | tangent residual and norm gates pass |
| `M4-FX-006-Q1Q2OceanicFields` | generated q1/q2 crust writes age zero and ridge direction | `a_o=0`, `r` matches `(p-qGamma)x p` |
| `M4-FX-007-OceanOceanAgePolarity` | older oceanic side subducts, matching the paper's O/O polarity rule | older-side label count is positive and younger-side label count is zero |
| `M4-FX-008-OceanOceanEqualAgeDefers` | equal or missing ages do not invent O/O polarity | no filter-active O/O labels |
| `M4-FX-009-OpeningContactOnly` | genuinely opening contact class is covered at micro scale | divergent contacts > 0 and convergent contacts == 0 |
| `M4-FX-010-MixedSignalSamePair` | same plate pair can carry convergent and divergent local contacts | both classes present for one pair; filtering remains local |
| `M4-FX-011-DistanceFrontAccumulation` | `d(p)` accumulates over steps before resample | independent raw-input oracle matches |
| `M4-FX-012-TrackingResetAtResample` | process tracking resets while fields survive transfer | tracking hashes reset; field hashes continue through resample |
| `M4-FX-013-FrontContinuityNoIds` | fronts re-derived across windows remain geometrically continuous without persistent ids | continuity ratio above threshold and no persistent-id store |
| `M4-FX-014-PaperRegimeCharacterization` | existing M3R behavior under 50k, 8-window, paper-speed/cadence stress | characterization report produced; dangerous subset zero |

Scale rows:

- 50k paper-regime characterization: required before field work and repeated
  after fields land.
- 50k multi-window fields-on gate: required.
- 100k fields-on characterization: required if runtime is acceptable; otherwise
  the closeout must state why not.
- 250k one-window fields-on gate: required.
- 250k multi-window fields-on characterization: optional but recommended.
- 500k: optional characterization only unless the user explicitly promotes it.

## Required Metrics

M4 closeout must report:

- source commit and commandlet command;
- whether the pre-field characterization run was performed, with output path;
- cadence inputs: `vm`, `v0`, `alpha`, `DeltaT`, timestep, steps/window, and
  windows;
- field storage enabled/disabled state;
- field transfer counts by source class: single-source, q1/q2, deferred,
  filter-exhausted, unresolved;
- scalar conservation/variation for `z` and `a_o` where meaningful;
- age advance total and q1/q2 age-reset count;
- invalid age count and O/O equal-age deferral count;
- vector tangent residuals, vector normalization residuals, and angular
  residuals against vector oracles;
- q1/q2 ridge-direction angular residuals;
- contact class distribution and same-pair mixed-signal count;
- O/C, O/O-age, O/O-equal, C/C, and third-plate polarity counts;
- active front count, front birth count, front retirement count, and continuity
  ratio;
- `d(p)` min/mean/max and oracle residual;
- subduction matrix density and reset count;
- hole/deferred debt by class and by window;
- dangerous non-opening prior-blocked q1/q2 oceanic count;
- M3R pinned baseline hash comparison;
- replay A/B hashes for authority fields, contact/tracking state, resample
  output, topology, and metrics;
- forbidden fallback/resolver counters plus source-inspection note;
- runtime split: motion, vector rotation, tracking, contact/process, resample,
  topology, hashing/diagnostics, full cycle, total, and peak memory.

## Hash Gates

M4 needs separate hash streams:

- M3R pinned content-stream hashes;
- field-authority hash covering plate-local field values;
- vector-field hash after motion;
- process-tracking hash covering active fronts, `d(p)`, and subduction matrix;
- resample-output hash covering field transfer records;
- topology hash;
- metrics hash.

Replay A/B is required but not sufficient. M4 must pin M3R predecessor hashes as
constants and rerun the predecessor configs before claiming the M4 path did not
break M3R.

## Performance Accounting

M4 keeps the M3R two-lane accounting and adds a field/tracking lane:

- per-step motion + vector rotation + convergence tracking;
- per-window resample/filter + field transfer + topology rebuild;
- process/contact lane;
- diagnostics/export/report generation.

Initial gates:

- 50k paper-regime characterization must complete and report all lanes; no hard
  time gate until the characterization sets budgets.
- 250k one-window fields-on full cycle should remain below the M2/M3 paper
  resample-row comparison lane of `3580 ms`, unless the entry packet is revised
  before implementation.
- process/tracking lane should be reported against the M3R process lane and the
  260 ms no-pathology subduction-row anchor, but M4 must not claim extra
  headroom from a favorable definition change.

If field transfer or tracking exceeds these anchors, stop and write an
investigation note before adding more M4 behavior.

## Lab Policies To Record

| policy | authority class | behavior | restoration or retirement |
|---|---|---|---|
| `M4-POL-PAPER-REGIME-CHARACTERIZATION` | diagnostic | 50k, 8-window, paper-speed/cadence stress run sets budgets before fields land | becomes a baseline in M4 closeout |
| `M4-POL-FRONT-CONTINUITY-NO-IDS` | source_implicit | front continuity is measured by re-derived geometry, not persistent identity | no retirement; this is the intended guardrail |
| `M4-POL-ZERO-FOLD-NEUTRAL` | source_silent | zero `f` is allowed until M5A/M5B adds fold updates | retire or narrow when fold mutation lands |
| `M4-POL-RIDGE-DIRECTION-FOR-GENERATED-OCEANIC` | source_explicit | q1/q2 generated oceanic sets `r = (p - qGamma) x p` | permanent |
| `M4-POL-OCEAN-OCEAN-AGE-POLARITY` | source_explicit | O/O polarity uses local oceanic age: older oceanic crust subducts; equal/missing age defers | permanent unless thesis reread finds sharper rule |
| `M4-POL-HOLE-GROWTH-BREACH-PROTOCOL` | diagnostic | paper-regime breach is classified as expected debt only for O/O/C/C deferral, not dangerous q1/q2 | retire once M5 drains debt |
| `M4-POL-NONOPENING-PUMP-INDEPENDENT-AUDIT` | diagnostic | dangerous subset is audited from output records, not only from coupled branch counters | may become M5C live gate |

## Stop Conditions

Pause and write an investigation note if:

- M3R pinned baselines drift without a reviewed reason;
- field-disabled or neutral-field runs change M3R decisions;
- any field update reads prior global owner or persistent sample ownership;
- q1/q2 generates oceanic when current boundary motion is non-opening;
- a non-opening previously blocked sample becomes oceanic;
- O/O polarity fires without valid ages on both sides;
- equal-age O/O contacts choose a subducting side;
- vector fields become non-tangent or non-finite;
- q1/q2 ridge direction disagrees with the independent cross-product oracle;
- `d(p)` cannot be reproduced from raw per-step motion/front inputs;
- active front ids persist across rebuild as authority;
- process-tracking state survives resample except through field transfer;
- hole-growth budget breach is hidden by changing the denominator;
- performance lane definitions change after results;
- 50k paper-regime characterization exposes unexplained debt growth and the
  implementation proceeds anyway.

## Recommended Implementation Order

1. Add the M4 commandlet/config/report shell and rerun M3R pinned baselines.
2. Add the paper-regime characterization row using existing M3R behavior only.
3. Add inert field storage and neutral-field no-op hashes.
4. Add scalar field barycentric transfer fixtures.
5. Add vector rotation/interpolation/tangent oracle fixtures.
6. Add q1/q2 oceanic field generation for age and ridge direction.
7. Add O/O age-polarity fixtures, including equal-age deferral.
8. Add genuinely-opening and mixed-signal contact micro fixtures.
9. Add per-step tracking records, `d(p)`, and subduction matrix as read-only
   process state.
10. Add reset and front-continuity gates with no persistent identity.
11. Add scale rows, closeout report, claim audit, and stop for user go/no-go
    before M5.

## Verdict

Milestone 4 is ready for user review as a plan. It is not approved for
implementation until the user explicitly says go.
