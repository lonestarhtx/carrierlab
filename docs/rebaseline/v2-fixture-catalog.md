# V2 Fixture Catalog

Date: 2026-06-08

Status: pre-code falsification fixture plan.

Purpose: define tiny deterministic worlds that should break a bad V2 design
before a full planet or editor tool exists.

Fixtures are not beauty scenes. They are small mechanical traps.

## Fixture Principles

- Prefer tiny sample counts first: enough topology to exercise the rule, not
  enough complexity to hide it.
- Each fixture must have an independent oracle.
- Every fixture must say which lifecycle transition it exercises.
- A fixture pass means only that fixture passed; it is not a whole-stage pass.
- Maps are optional. Metrics are mandatory.

## Fixture Index

| ID | Name | First stage | Lifecycle focus | Expected verdict |
| --- | --- | --- | --- | --- |
| FX-000 | Single plate identity | V2-0 | substrate/local topology/projection | pass |
| FX-001 | Two plate clean partition | V2-0 | duplicated topology and t=0 projection | pass |
| FX-002 | Boundary degeneracy pin | V2-0 | exact edge/vertex hit classification | pass with counted degeneracy |
| FX-003 | Deliberate topology hole | V2-0 | true miss detection | fail |
| FX-004 | Deliberate duplicated overlap | V2-0 | true overlap detection | fail |
| FX-005 | Zero motion replay | V2-1 | determinism and no drift | pass |
| FX-006 | Single plate rotation | V2-1 | analytic geodetic motion | pass |
| FX-007 | Forced divergence | V2-1/V2-2 | gap classification under motion | classified, not repaired |
| FX-008 | Forced convergence | V2-1/V2-2 | overlap/contact classification | classified, not repaired |
| FX-009 | Third-plate intrusion | V2-2 | contact classification beyond pair labels | diagnostic pass if visible |
| FX-010 | Ocean-ocean age polarity | V2-3 | subduction polarity source fields | pass |
| FX-011 | Continental collision candidate | V2-3 | collision event readiness | pass dry run |
| FX-012 | Filtered overlap remesh | V2-4 | subducting/colliding filter before selection | pass |
| FX-013 | Post-filter unresolved multi-hit | V2-4 | source-silent tie-break exposure | expected no-go |
| FX-014 | Divergent q1/q2 gap | V2-4 | zero-hit oceanic generation | pass if provenance complete |
| FX-015 | No q1/q2 boundary pair | V2-4 | forbidden prior-owner fallback | fail |
| FX-016 | Mixed-vertex rebuild triangle | V2-4 | lab-policy need for triangle assignment | blocked until policy approved |
| FX-017 | Material conservation control | V2-4 | no hidden material loss/creation | pass |
| FX-018 | Editor observation lock | V2-5 | UI cannot mutate core state | pass |

## Fixture Details

### FX-000 Single Plate Identity

Purpose: check the simplest carrier path.

Setup:

- one plate covers the whole substrate
- zero motion
- uniform material field

Independent oracle:

- every global sample ray should hit exactly one plate-local triangle, except
  boundary-degenerate duplicate hits within the same surface location if the
  query samples are placed exactly on triangle boundaries.

Fail if:

- any non-degenerate miss appears
- any cross-plate overlap appears
- projection reads a stored global owner instead of local geometry

### FX-001 Two Plate Clean Partition

Purpose: test duplicated/re-indexed local topology across a boundary.

Setup:

- two plates split by a known great-circle or deterministic domain boundary
- zero motion
- distinct material classes or plate-local ids

Independent oracle:

- sample directions away from the boundary have exactly one owner
- boundary directions are counted separately

Fail if:

- the projection cannot distinguish true overlap from boundary degeneracy
- material authority lives only on global samples

### FX-002 Boundary Degeneracy Pin

Purpose: make the boundary policy unavoidable.

Setup:

- choose sample directions that lie on an edge and a vertex of plate-local
  triangles

Independent oracle:

- geometry construction knows the sample is on a zero-area boundary condition

Expected result:

- `boundary_degenerate_count > 0`
- non-degenerate miss/overlap remains zero
- selected owner, if any, is marked as policy-selected, not source-cited

Fail if:

- boundary hits are counted as true overlaps
- boundary resolver claims paper faithfulness without source status

### FX-003 Deliberate Topology Hole

Purpose: check that misses are not repaired away.

Setup:

- remove one plate-local triangle after duplication in a fixture-only path

Independent oracle:

- known missing region by triangle id and area

Expected result:

- true miss count and area become nonzero
- stage verdict fails

Fail if:

- miss is filled by prior owner, ocean, nearest plate, or map state

### FX-004 Deliberate Duplicated Overlap

Purpose: check that true overlaps are visible.

Setup:

- duplicate one triangle into another plate in a fixture-only path

Independent oracle:

- known overlap triangle ids

Expected result:

- non-degenerate overlap count becomes nonzero
- stage verdict fails

Fail if:

- centroid/nearest policy hides the overlap in the primary metrics

### FX-005 Zero Motion Replay

Purpose: check deterministic non-motion.

Setup:

- multiple steps, zero angular velocity for every plate

Independent oracle:

- identity transform
- stable hashes

Expected result:

- no drift
- no material change
- projection hash stable

Fail if:

- material or projected output changes without process mutation

### FX-006 Single Plate Rotation

Purpose: verify geodetic motion independent of projection.

Setup:

- one plate, nonzero axis and angular speed

Independent oracle:

- analytic rotation from initial positions and elapsed time

Expected result:

- local vertex positions match oracle
- material remains attached to moved vertices
- projection coverage remains exact because one plate covers the sphere

Fail if:

- drift oracle uses the mutated positions as expected values

### FX-007 Forced Divergence

Purpose: show that motion can create gaps without pretending they are bugs.

Setup:

- two neighboring plates move away from each other

Independent oracle:

- signed relative boundary velocity indicates separation

Expected result:

- zero-hit regions classified as divergent candidates
- no gap fill occurs before remesh stage

Fail if:

- V2-1 fills the gap
- fixed global sample ownership keeps old material in the gap

### FX-008 Forced Convergence

Purpose: show overlap/contact before process mutation.

Setup:

- two neighboring plates move toward each other

Independent oracle:

- signed relative boundary velocity indicates convergence

Expected result:

- raw multi-hit/convergent overlap visible
- no remesh-time winner selected in V2-1/V2-2

Fail if:

- centroid/random/nearest winner becomes primary behavior

### FX-009 Third-Plate Intrusion

Purpose: keep triple interactions visible.

Setup:

- three plates arranged so a sample/contact neighborhood can see all three

Independent oracle:

- fixture knows the plate triple involved

Expected result:

- third-plate evidence is reported as a distinct class

Fail if:

- classification collapses the event into a two-plate pair without evidence

### FX-010 Ocean-Ocean Age Polarity

Purpose: verify process state needs oceanic age before remesh.

Setup:

- two oceanic plates in forced convergence with different ages

Independent oracle:

- older oceanic crust subducts

Expected result:

- subducting triangle mark lands on the older oceanic side

Fail if:

- remesh chooses a survivor without subduction mark

### FX-011 Continental Collision Candidate

Purpose: test collision readiness without topology mutation first.

Setup:

- continental-on-continental forced convergence

Independent oracle:

- contact qualifies as collision candidate, not oceanic subduction

Expected result:

- candidate event record exists
- no centroid remesh preservation policy is used

Fail if:

- continental material is handled as generic overlap resolution

### FX-012 Filtered Overlap Remesh

Purpose: exercise the paper's multi-hit answer.

Setup:

- forced overlap where one triangle is already marked subducting/colliding

Independent oracle:

- filtered candidate set has one valid survivor

Expected result:

- remesh samples ignore marked triangle before selection
- surviving valid hit transfers material

Fail if:

- filtering occurs after a winner is selected

### FX-013 Post-Filter Unresolved Multi-Hit

Purpose: make source silence explicit.

Setup:

- two valid non-subducting/non-colliding candidates remain after filtering

Independent oracle:

- fixture intentionally creates a source-silent condition

Expected result:

- paper-faithful path returns no-go/unresolved
- optional comparison policy can run only if explicitly requested

Fail if:

- primary path silently picks a nearest/centroid/random winner

### FX-014 Divergent q1/q2 Gap

Purpose: verify named oceanic creation.

Setup:

- zero-hit divergent gap bounded by two plates

Independent oracle:

- known q1/q2 nearest boundary points or a small analytic boundary case

Expected result:

- q1 and q2 belong to different plates
- qGamma is spherical midpoint
- generated material provenance is `divergent_generated`
- material ledger records creation

Fail if:

- old material is retained by prior owner

### FX-015 No q1/q2 Boundary Pair

Purpose: check for hidden fallback.

Setup:

- zero-hit sample cannot find two distinct boundary plates

Independent oracle:

- fixture intentionally removes/blocks one boundary pair

Expected result:

- hard failure

Fail if:

- sample inherits prior owner or nearest global owner

### FX-016 Mixed-Vertex Rebuild Triangle

Purpose: isolate the remesh partition source gap.

Setup:

- global TDS triangle whose three remesh vertex assignments disagree

Independent oracle:

- known vertex assignment pattern

Expected result:

- blocked until named lab policy is approved

Fail if:

- majority/area/lower-id policy is hidden in primary behavior

### FX-017 Material Conservation Control

Purpose: guard against quiet material erosion.

Setup:

- no named material creation/destruction process enabled

Independent oracle:

- sum material/area by class from plate-local authority before and after

Expected result:

- deltas are zero or within numeric tolerance

Fail if:

- projected material replaces authority
- material changes without provenance

### FX-018 Editor Observation Lock

Purpose: ensure the in-editor tool remains an observer/controller, not a solver.

Setup:

- run a known state through commandlet and editor-view readout

Independent oracle:

- core state hashes before/after editor display

Expected result:

- UI interactions that are not approved commands leave simulation hashes
  unchanged

Fail if:

- actor/control-panel view mutates material, ownership, or process state

## Fixture Promotion Rule

A fixture can become an implementation gate only when:

1. Its source status is listed in `v2-source-anchor-table.md`.
2. Its state transitions are listed in `v2-state-transition-model.md`.
3. Its metrics are listed in `v2-metric-schema.md`.
4. The checkpoint states whether the fixture is paper-faithful, derived, lab
   policy, or diagnostic-only.
