# Phase II Pre-Mortem

Assume Phase II fails after several slices. These are the most likely failure
modes, ranked by likelihood times blast radius.

## 1. Contact Labels Become Hidden Ownership

Symptom: subduction labels stabilize a global-sample footprint while
plate-local geometry moves.

Likely cause: contact state is stored on output samples and reused as authority
instead of being recomputed from plate-local boundary evidence.

Evidence:

- changing global resolution changes process state with the same plate geometry
- material appears anchored while source triangles rotate
- process hash changes when prior projected owner ids are perturbed

Detection:

- hash plate-local triangle labels separately from projected sample labels
- run the same plate-local setup against different global output resolutions
- forbid prior resolved owner id as an input to contact authority

## 2. Centroid Policy Survives Under A New Name

Symptom: Phase II claims subduction labels, but convergent samples still choose
the nearest plate center.

Likely cause: Stage 1.5 fallback logic remains in the decisive path.

Evidence:

- polarity swap does not swap the filtered plate
- small or elongated plates lose area in the same pattern as centroid tie-break
- label counts do not affect filter decisions

Detection:

- mirrored forced-convergence fixture
- polarity-swap fixture
- report primary path separately from centroid/random comparison controls

## 3. Polarity Is Underspecified And Invents Physics

Symptom: same-material contacts get labeled as subduction even though Phase II
lacks oceanic age, density, collision, or slab state.

Likely cause: the implementation forces every convergent overlap into an
over/under answer.

Evidence:

- continental-continental contacts produce subducting labels
- oceanic-oceanic contacts pick a plate without age or fixture polarity
- ambiguity counts are suspiciously zero

Detection:

- same-material contacts must be ambiguous unless fixture polarity is enabled
- mixed-material fixture proves oceanic-under-continental separately
- report ambiguous-polarity count as a first-class metric

## 4. Subduction Filter Removes Too Much

Symptom: miss and multi-hit rates improve, but CAF or per-plate area collapses.

Likely cause: labels spread from a narrow contact locus into a broad area mask.

Evidence:

- filtered triangle area grows faster than boundary length
- filtered mask becomes visually thick
- first event produces large unexplained per-plate deltas

Detection:

- report filtered triangle count, filtered area, and distance from contact
- gate filtered area relative to contact-boundary length
- export a subducting-triangle mask at every checkpoint

## 5. Subduction Filter Removes Too Little

Symptom: contacts exist, but unresolved multi-hit remains close to Stage 1.5.

Likely cause: contact labels are not attached to the triangles hit by
ray-from-origin projection.

Evidence:

- contact count is high but filtered candidate count is near zero
- triangle ids in labels do not match projection candidate ids
- unresolved overlaps cluster around labeled contacts

Detection:

- every filtered candidate must point to a contact and label
- report contact-to-label and label-to-filter traceability tables
- run a small brute-force replay to compare expected candidate ids

## 6. Sign Classification Regresses

Symptom: forced convergence and forced divergence again produce near-symmetric
miss/multi distributions.

Likely cause: signed velocity became magnitude-only, used `abs`, or evaluated at
the wrong evidence point.

Evidence:

- reversing motion vectors does not reverse signed velocity
- convergence controls emit gap classes, divergence controls emit subduction
  labels

Detection:

- log per-fixture axes, angular speeds, and signed pair velocity at step 0
- assert sign expectations before labels are emitted
- keep direction controls as hard gates, not inspection notes

## 7. Third-Plate Intrusion Becomes Misclassified Subduction

Symptom: triple junctions generate ordinary two-plate subduction labels.

Likely cause: contact detection assumes two nearest plates even when three
non-boundary candidates are present.

Evidence:

- top plate triples explain most unresolved overlap or material loss
- contact ids omit the third plate even when the projection candidates include
  it

Detection:

- keep third-plate intrusion as its own class
- export third-plate contact masks
- forbid two-plate labels when three non-boundary candidates are present unless
  an explicit disambiguation rule is approved

## 8. Material Looks Conserved Only In Aggregate

Symptom: global CAF stays stable while individual continents smear or jump
between plates.

Likely cause: scalar material totals are conserved but provenance is not.

Evidence:

- per-plate continental areas compensate globally
- tagged features have discontinuous centroid jumps after resampling
- event logs cannot explain where material moved

Detection:

- track per-plate continental and oceanic mass
- track tagged-feature drift and source triangle lineage
- require material deltas to reconcile with named events

## 9. Performance Cliff

Symptom: Phase II works at 10k or 60k but cannot approach the paper budget at
250k.

Likely cause: contact detection performs all-pairs boundary searches, rebuilds
too many spatial indices, or logs unbounded event detail.

Evidence:

- contact detection or label propagation dominates projection and resampling
- runtime scales worse than sample count and boundary length suggest

Detection:

- report timing slices for projection, contact detection, triangle labeling,
  filtering, resampling, hashing, and export
- run 60k profiling before escalating to 100k/250k

## Required Negative Controls

- Zero-motion: no contacts, no labels, no material change.
- Single plate: no contacts and no event effects.
- Forced divergence: gaps only, no subduction labels.
- Forced convergence: contacts appear with known signed velocity.
- Polarity swap: filtered plate swaps.
- Mixed material: oceanic-under-continental can be labeled.
- Same-material: ambiguous unless fixture polarity is enabled.
- All-continental: no spurious oceanization from labels alone.
- Ocean-only: no spurious continental material from labels alone.
- Triple junction: third-plate intrusion remains separately classified.
