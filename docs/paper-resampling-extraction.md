# Paper Resampling Extraction

Status: focused extraction of the resampling/remeshing layer from Cortial et al.,
Procedural Tectonic Planets (2019) and the Cortial PhD thesis (Synthese de
terrain a l'echelle planetaire, 2020). This doc supersedes the resampling
sections of `docs/paper-carrier-extraction.md` and
`docs/phase-iii-paper-process-extraction.md`.
It is the canonical reference for IIIE design, for the eventual Stage 1.5
architectural revision, and for any future review of multi-hit policy,
divergent gap-fill, or process state persistence across resamples.

## Scope

What this doc covers:

- The architectural relationship between the global TDS, plate-local
  triangulations, and global Fibonacci samples.
- The full per-resample algorithm (pre-treatment, sampling, plate rebuild).
- Multi-hit policy as actually specified by the paper.
- Divergent gap-fill (q1, q2, qGamma) at the algorithmic level, not just the
  conceptual `z = alpha z_bar + (1-alpha) z_Gamma` formula.
- Process state persistence rules across resample events.
- The cadence formula from the thesis, which is more precise than the paper's
  "10-60 iterations" wording.
- Implications for our staged architecture, especially Stage 1.5.

What this doc does NOT cover:

- Per-step convergence tracking internals (IIIB), except where they feed
  resampling.
- Subduction uplift, slab pull, or collision uplift formulas (those live in
  `phase-iii-paper-process-extraction.md`).
- Erosion and oceanic dampening (§3.3.3).
- Amplification (Chapter 4 of the thesis).

## Sources And Citation

Primary source: Cortial PhD thesis, especially §3.2.4 (Etat initial), §3.3.1.3
(Implementation of convergence), §3.3.2.1 (Generation du plancher oceanique),
§3.3.2.3 (Implementation of divergence). The paper is consistent with the
thesis but less detailed; thesis page numbers below are the authoritative
references.

Citations use thesis page numbers from the HAL-distributed PDF
(`docs/Synthèse de terrain à léchelle planétaire/Synthèse de terrain à
l'échelle planétaire.pdf`). Paraphrased throughout to respect copyright; short
anchor phrases retained for traceability only.

## Architectural Model

### One global TDS, used as both initial substrate and resampling target

Thesis §3.2.4 (p. 54-55): the Fibonacci sphere sampling and the spherical
Delaunay triangulation built over those samples are pre-computed once.
Anchor: "pre-calculees une fois pour toutes, et re-utilisees de nombreuses
fois (y compris en cours de simulation pour le re-maillage episodique de la
planete)". The global TDS is not just an initialization artifact; it is the
substrate that resampling re-uses every cadence period.

### Plates own duplicated, re-indexed sub-triangulations

Thesis §3.2.4 (p. 55): each plate is materialized by extracting its
triangles and vertices from the global TDS and copying them into a
dedicated per-plate spherical Delaunay triangulation. Anchor: "nous
instancions N triangulations de Delaunay spheriques, qui serviront de
support numerique au modele". The data structures are duplicated,
re-indexed, and re-compacted; plate borders carry empty neighborhoods.

This means plate-local triangulations are first-class objects, not
partition pointers into the global TDS. Our CarrierLab architecture is
correct on this point.

### Plate-local vertices rotate rigidly between resamples

Thesis §3.3.1.3 (p. 61): at the start of each step, the geodesic rotation
G is applied to every vertex of every plate's triangulation. The fold
direction f and ridge direction r vector fields are also rotated to
preserve geometric coherence. Plate-local vertices therefore drift
arbitrarily far from their original Fibonacci positions between resample
events; they only re-sync to the canonical Fibonacci grid at the next
resample.

This is the carrier authority: plate-local triangles, with vertices that
move with the plate, are the source of truth. The global Fibonacci grid
is fixed in space; the plates rotate over it.

## Resampling Algorithm (Thesis §3.3.2.3, p. 67-69)

The resample event is specified as a complete planet remesh, not an
incremental update. The full algorithm:

### Step 1: Plate pre-treatment

For each plate, in parallel:

- Recompute the plate boundary EXCLUDING triangles currently marked as
  in-subduction. This is important for the gap-fill step.
- If the plate is entirely subducted (zero non-subducting triangles),
  destroy the plate.
- Otherwise build a per-plate BVH over its remaining triangles for the
  ray-triangle intersection tests in Step 2.

### Step 2: Per-vertex sampling over the global TDS

For each vertex p of the pre-computed global TDS:

a) Cast a ray from the planet center through p. For each existing plate,
   test ray-triangle intersection against that plate's BVH.

b) If the intersection hits a triangle currently marked as in-subduction
   OR in-collision, IGNORE that intersection. Anchor (p. 68): "Si
   l'intersection se fait avec un triangle en subduction ou en collision,
   nous ne la prenons pas en compte." This is the rule that resolves
   multi-hit cases - subducting/colliding triangles are invisible to
   resampling rays.

c) If after filtering there is at least one valid intersection, the
   vertex's new crust parameters are computed by barycentric
   interpolation of the intersected triangle's three vertex parameters.
   The vertex records the index of the intersected plate.

d) If after filtering there are zero valid intersections, the vertex is
   in a divergent gap zone. Run the gap-fill algorithm (see below) to
   compute new oceanic crust parameters. The vertex records the index of
   the nearest existing plate.

### Step 3: New plate construction

Partition the global TDS triangles based on the per-vertex plate
assignments computed in Step 2. For each resulting partition:

- Duplicate the relevant triangles and vertices into a fresh per-plate
  spherical triangulation, re-indexed and re-compacted, exactly as in
  initial state construction (§3.2.4).
- The plate inherits its previous geodesic motion G unchanged.
- Empty neighborhoods are added at the new plate borders.

The effect: plate shapes from immediately before the remesh are
preserved, but augmented to cover former divergence zones, and the
covering is watertight (every direction on the sphere is covered by
exactly one plate triangle, modulo subduction/collision exclusions).

### Step 4: Subduction state reset

Anchor (p. 69): "les subductions sont en pratique reinitialisees car tous
les triangles subduits disparaissent lors du remaillage; la matrice de
subduction est donc au passage invalidee et reconstruite pour les pas de
temps suivants." All subducted triangles are gone (excluded from the new
plate meshes). The subduction polarity matrix is invalidated; the
convergence-tracking phase rebuilds it from current plate boundaries
in subsequent steps.

### Step 5: Stochastic rifting test

After remesh, the rifting probability test runs (per §3.3.2.2). Plates
that pass the test are fragmented into 2-4 sub-plates via Voronoi cells.
Rifting is itself implemented by the same duplication/re-indexing
machinery used elsewhere.

## Divergent Gap-Fill (q1, q2, qGamma)

When a global TDS vertex has zero valid plate intersections (after
filtering subducting/colliding triangles), it is in a divergent gap.

The algorithm (Thesis §3.3.2.3, p. 68):

1. Find q1: the nearest point on any plate's boundary to the vertex p.
   This is a continuous nearest-point query over plate boundary edges,
   not a discrete sample lookup. The phrase "le point q1 le plus proche
   du sommet situe sur une frontiere de plaque" implies the closest
   point on any boundary edge, computed by projecting onto each edge's
   great-circle plane and clipping.

2. Find q2: the nearest point on the boundary of a DIFFERENT plate.
   Anchor: "une zone de divergence est entouree par au moins deux
   plaques." So q1 and q2 always belong to different plates.

3. Compute qGamma as the spherical midpoint of q1 and q2:
   `qGamma = R * (q1 + q2) / norm(q1 + q2)`

4. Compute the new oceanic crust parameters at p using the formula from
   §3.3.2.1:
   - `dGamma = geodesic distance from p to qGamma`
   - `dPlate = geodesic distance from p to the nearer of q1, q2`
   - `alpha = dGamma / (dGamma + dPlate)`
   - `z(p) = alpha * z_bar(p) + (1 - alpha) * z_Gamma(p)`
   - `z_bar` is linear interpolation of elevation between the two plates'
     boundary points
   - `z_Gamma` is a generic ridge profile (peak at the ridge, dropping
     to abyssal depth at distance)
   - The local ridge direction `r(p) = (p - qGamma) cross p`

5. The vertex is assigned to the nearer of the two plates (q1's plate if
   `dist(p, q1) < dist(p, q2)`, else q2's plate). This determines plate
   ownership of the new oceanic vertex.

## Multi-Hit Policy Resolution

The paper does NOT specify a multi-hit tie-break policy in the sense of
"pick by centroid distance" or "pick by smaller triangle area." Instead:

- Subducting and colliding triangles are filtered OUT before evaluating
  intersections.
- Polarity decisions made by the convergence-tracking phase (subduction
  matrix entries, collision marks) determine which triangle is the
  subducting/colliding one in any plate-pair encounter.
- After filtering, the remaining intersection (if any) is the answer.
- If multiple non-subducting/non-colliding intersections remain, this is
  a degenerate state that shouldn't occur in steady operation. The thesis
  doesn't specify a tie-break for this case because the convergence
  tracking is supposed to have resolved polarity before resampling runs.

Implications:

- Our Stage 1.5 multi-hit policies (nearest-centroid, etc.) are LAB
  POLICY introduced because Stage 1.5 was tested in isolation without
  convergence tracking. They are not paper-faithful and should be
  retired once Stage 1.5 is wired to read the subduction matrix.
- The "convergent overlap" class our metrics track is, in the paper's
  model, never actually a resampling decision - it's resolved upstream
  by polarity decisions and triangle marking.

## Process State Persistence Across Resamples

| State | Persistence rule |
|---|---|
| Plate motion G (rotation axis, angular speed) | Preserved exactly across the resample event. |
| Continental crust scalar parameters (xC, e, z, ao, ac, o) | Preserved via barycentric interpolation at the resample step. |
| Continental crust vector parameters (f, r) | Preserved via barycentric interpolation. They are continuously rotated by G between resamples. |
| Subduction triangle marks | Reset. All subducting triangles disappear (excluded from the new plate meshes). |
| Subduction polarity matrix (per plate-pair) | Invalidated and rebuilt by the convergence-tracking phase in subsequent steps. |
| Convergence-tracking lists (per plate, per triangle) | Reset; rebuilt from new plate boundaries. |
| Distance-to-convergence-front d(p) | Reset; the new plate boundary triangles start at d=0 when they next enter convergence. |
| Continental terranes (already-sutured) | Persist as part of the destination plate's mesh (suture is a topology mutation, not a separate mark). |
| Collision history | None tracked. Sutures are baked into mesh topology. |
| Visible elevation (trench-depth at front) | Preserved via barycentric interpolation. New ocean floor at divergent gaps gets elevation from the gap-fill formula. |
| Historical elevation (preserved for uplift) | Preserved via barycentric interpolation, on triangles that are not currently in subduction (since those are excluded). |

The general rule: state stored on plate-local vertices survives because
barycentric interpolation transfers it to the new global TDS vertices.
State stored on triangle marks does NOT survive because triangles are
discarded and rebuilt.

## Continental Persistence Mechanism

The paper does not preserve continental coastlines via clever resampling.
It preserves them via collision-driven terrane transfer that runs in the
convergence phase BEFORE the next resample. Detailed flow (Thesis
§3.3.1.3, p. 63-64):

1. Convergence tracking detects two intersecting continental triangles
   (one from each of two plates).
2. The pair switches from subduction-tracking mode to collision-tracking
   mode.
3. The accumulated distance d(p) for the leading triangle is monitored.
4. When d(p) exceeds 300 km (constant rc-related threshold), the
   collision is authorized.
5. Slab Break: terranes (connected continental triangles, plus enclosed
   interior seas) are extracted from the carrier plate Pi by deleting
   their triangles from Pi's mesh and copying them into a temporary
   structure.
6. Continental mass on Pj is verified to be sufficient relative to the
   incoming terranes; otherwise the collision is aborted for this Pj.
7. Uplift is propagated from the intersection triangles outward through
   Pj's mesh by neighborhood traversal, applying delta-z to each vertex
   within the collision radius r.
8. Suture: the terrane triangles are added to Pj's mesh, with adjacency
   reconstructed at the seam.
9. Pj's boundary list and convergence-tracking list are reset.
10. Only ONE collision is processed per time step. Other pending
    collisions wait for subsequent steps.

This mechanism is what makes the resampler's "skip subducting/colliding
triangles" rule safe for continental material: by the time the resample
runs, continental crust that would otherwise be lost has already been
transferred to its natural destination plate.

## Cadence Formula

Thesis §3.3.2.3 (p. 68): the resample period is

```
DeltaT = (1 - alpha) * M + alpha * m
```

where

```
alpha = min(1, vm / v0)
```

and

- `vm` = maximum observed plate speed (mm/yr)
- `v0` = 100 mm/yr (constant from Table 3.2)
- `m` = 32 Ma (minimum period, when plates are at maximum speed)
- `M` = 128 Ma (maximum period, when plates are at zero speed)

At our default `dt = 2 Ma/step`, the period spans 16 to 64 steps. Our
current configuration uses a fixed 32 steps (= 64 Ma), which is in the
middle of the legal range but is not speed-driven.

The paper's looser wording ("every 10-60 iterations" / "every 20 to 120
My") is less precise than the thesis specification. The thesis is
authoritative.

Implementation note: the period is recomputed at each resample event
based on the maximum plate speed observed since the previous resample.
A burst of fast plate motion shortens the next period; a quiescent
phase lengthens it.

## Stage 1.5 Reframing

Stage 1.5 was implemented as a standalone slice that resamples without
input from a convergence-tracking layer. The thesis algorithm makes it
clear this cannot be paper-faithful in isolation:

- The resampler's filtering rule depends on subduction and collision
  marks. Without those marks, every plate-triangle intersection is
  "valid," and multi-hit cases must be resolved by some other policy.
- Continental coastline preservation depends on collision-driven terrane
  transfer that runs in the convergence-tracking phase BEFORE the
  resample. Without that phase, continental material that would be
  rescued by collision is instead overwritten by oceanic plate
  interpolation.

Slice 5.5's diagnosed "coherent transfer from uniformly-oceanic interior
triangles" failure mode is consistent with this reframing: a fast oceanic
plate sweeps over a former continental coast, and at resample time the
canonical TDS vertex at that direction is intersected by the oceanic
plate's interior triangle (which is not marked subducting because no
tracking phase ran). Barycentric interpolation reads three oceanic
vertices and outputs oceanic. The continental sample is lost.

The architectural fix is therefore not to make Stage 1.5 cleverer in
isolation. The fix is to integrate Stage 1.5 with Phase III's tracking
data: the resampler must read the subduction matrix and the collision
state. Stage 1.5 cannot stand alone.

This reframes the staged architecture:

- Stage 0: plate-local carrier authority + ray-from-origin projection.
- Stage 1: rigid motion only.
- Stage 1.5: not a useful standalone slice. The resample step exists
  but requires Phase III input to operate paper-faithfully.
- Phase II: per-step process layer baseline. Material/elevation state
  on plate-local vertices.
- Phase III: convergence tracking, subduction marks, collision marks,
  terrane transfer. PRECEDES the next resample event.
- IIIE: implements the resample event as the full algorithm above,
  consuming Phase III state.

The Stage 1.5 hard-gate failure is therefore not an architectural defect
of the carrier model. It is a layering defect of our staged extraction.

## Implementation Contract for IIIE

When IIIE is implemented, the resample commandlet should follow the
algorithm above byte-for-byte where feasible, with the following
contract points:

1. The global TDS is pre-computed and immutable. It is loaded once at
   simulation start, alongside its Fibonacci sample positions.
2. Resample triggers are evaluated against the cadence formula at each
   step. The maximum observed plate speed since the previous resample
   determines the next period.
3. Pre-treatment runs first: each plate's boundary is recomputed
   excluding subducting triangles. Fully-subducted plates are destroyed.
4. Per-vertex sampling iterates the global TDS in a deterministic order.
   For each vertex, the BVH-accelerated ray-triangle intersection test
   filters subducting/colliding triangles. Barycentric interpolation
   transfers parameters from the surviving intersection.
5. Divergent gap-fill uses continuous nearest-point queries on plate
   boundary edges. Endpoint+midpoint discrete approximation is
   acceptable lab policy if metrics show the residual error is small,
   but exact continuous queries are paper-faithful and should be the
   eventual target.
6. New plate construction partitions global TDS triangles by per-vertex
   plate assignment. Triangles whose three vertices disagree on plate
   assignment require a tie-break (the thesis does not specify this
   case explicitly; the most natural rule is majority, with the
   plate-with-larger-area as a secondary tiebreak; this is a lab policy
   choice that needs to be named and tested).
7. Subduction matrix is invalidated at the end of the resample.
   Convergence-tracking state is reset to fresh from the new plate
   boundaries.
8. Hash regression: the resample event must be deterministic given the
   same input plate state and cadence trigger. Hashes should cover the
   global TDS vertex assignments, the new per-plate triangulation
   topology, and the new per-vertex crust parameters.
9. Continental terrane transfer (collision events) happens in IIID, not
   IIIE. IIIE assumes IIID has already done its work; IIIE should never
   attempt to handle continental persistence on its own.

## Deviations And Lab-Policy Approximations

| Item | Current implementation | Paper-faithful target | Status |
|---|---|---|---|
| Cadence | Fixed 32 steps | Speed-driven via DeltaT formula, m=32 Ma, M=128 Ma | Lab policy; should be replaced when IIIE lands. |
| Multi-hit policy | Configurable lab policies (nearest-centroid, etc.) | Filter subducting/colliding triangles, take remaining intersection | Lab policy; required because Stage 1.5 has no Phase III input. Retire when integrated. |
| q1/q2 nearest points | Discrete: boundary endpoints and edge midpoints | Continuous: nearest point on each great-circle arc edge | Lab approximation; reasonable at 50k+ sample resolution but not exact. |
| qGamma | Spherical midpoint of q1 and q2 | Spherical midpoint of q1 and q2 | Faithful. |
| Subduction state reset on resample | Implemented (IIIA remesh-reset) | Reset all subducted triangles + invalidate matrix | Faithful. |
| Visible/historical elevation split | IIIC.2 implements both | Both elevations on subducting triangles, historical preserved for uplift | Faithful. |
| Continental persistence | Not yet implemented | Collision-driven terrane transfer at 300 km interpenetration | Pending IIID. |
| Triangle assignment from per-vertex plate ids | Not yet implemented | Specified as "partition triangles per assignments"; mixed-vertex triangles need a tie-break the thesis does not specify | Lab policy decision needed in IIIE. |
| Resample as a complete planet remesh | Stage 1.5 is partial | Full planet remesh per the algorithm above | Pending IIIE. |

## Open Questions Remaining

1. The thesis does not explicitly specify the rule for global TDS
   triangles whose three vertices end up assigned to different plates
   after Step 2. We need a deterministic, hash-stable rule. Candidate:
   majority assignment, with plate-area as tiebreaker.

2. The continuous nearest-point-on-boundary-edge query is described
   conceptually but not implemented in the thesis pseudocode. We need
   to either implement the closed-form solution (project onto each
   edge's great-circle plane, clip to arc, take min over edges) or
   prove that our discrete approximation is bounded.

3. The "one collision per step" rule (§3.3.1.3) plus the cadence-driven
   resample period interact: in a step where a collision triggers, does
   the resample also run if its cadence trigger fires the same step? The
   thesis is silent. Implementation choice needed.

4. The thesis implies (but does not state explicitly) that subduction
   marks made between resamples accumulate on plate-local triangles
   that do persist through the next resample (the over-rider's
   triangles, which are not subducting). We should verify this against
   §3.3.1.3's tracking-list propagation rules.

5. Rifting (§3.3.2.2) is described as happening immediately after the
   resample event. Whether rifting can ALSO happen at non-resample
   steps is unclear. The thesis's Bernoulli probability formula
   suggests one stochastic test per resample period, not per step.

## References

- Cortial, Yann. Synthese de terrain a l'echelle planetaire. PhD thesis,
  Universite de Lyon / INSA Lyon, 2020. NNT 2020LYSEI094, HAL tel-03186765.
  Chapter 3, especially sections 3.2.4, 3.3.1.3, 3.3.2.1, 3.3.2.3.
- Cortial, Y., Peytavie, A., Galin, E., Guerin, E. Procedural Tectonic
  Planets. Computer Graphics Forum 38(2), 2019. HAL hal-02136820. Section
  6 (Implementation Details).
- `docs/paper-carrier-extraction.md` (predecessor; the resampling rows of
  its specification table are superseded by this doc).
- `docs/phase-iii-paper-process-extraction.md` (process layer extraction;
  complementary).
- `docs/checkpoints/stage-1.5-report.md` (the original Stage 1.5
  hard-gate failure record; should be reviewed in light of the
  reframing in this doc).
- `docs/checkpoints/phase-iii-iiic-consolidated.md` (current IIIC
  state; consistent with this doc on the visible/historical elevation
  split and remesh reset).
