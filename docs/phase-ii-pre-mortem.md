# Phase II Pre-Mortem

Assume Phase II fails after several slices. These are the most likely failure
modes, ranked by likelihood times blast radius.

## 1. Contact Labels Become Hidden Ownership

Symptom: subduction contacts stabilize global sample ownership or keep material
anchored while plate-local geometry moves.

Likely cause: contact labels are stored on global samples and reused as
authority instead of being recomputed from plate-local boundary evidence.

Evidence: same plate-local geometry with a different global sample resolution
changes process outcomes; material can remain stable while source triangles
move.

Detection: hash plate-local triangle labels separately from projected sample
labels. The process hash must not depend on prior global sample owner ids.

## 2. Centroid Policy Survives Under A New Name

Symptom: Phase II appears to fix Stage 1.5, but convergent contacts still choose
the nearest plate center rather than the physically labeled over/under plate.

Likely cause: fallback code from Stage 1.5 remains in the decisive path.

Evidence: changing the synthetic subduction polarity has little or no effect on
filtered triangle counts; small or elongated plates still lose area in the same
pattern as centroid tie-break.

Detection: run mirrored two-plate convergence with swapped polarity. The
filtered plate must swap.

## 3. Subduction Filter Removes Too Much

Symptom: miss rate drops after resampling, but authoritative CAF or plate area
collapses because broad triangle bands are filtered.

Likely cause: contact labels spread from a narrow convergent boundary into an
area mask.

Evidence: filtered-triangle mask is visibly thick; filtered area grows faster
than boundary length; per-plate area delta exceeds the gate in the first event.

Detection: report filtered triangle count, filtered area, and distance from
contact boundary every event.

## 4. Subduction Filter Removes Too Little

Symptom: explicit labels exist, but multi-hit rate remains close to the Stage
1.5 centroid/random conditions.

Likely cause: the label is attached to boundary contacts but not propagated to
the local triangles used by ray projection.

Evidence: contact count is nonzero, but `IsTriangleFiltered...` reports near
zero filtered triangles.

Detection: every contact must produce traceable triangle labels with source
triangle ids.

## 5. Polarity Is Numerically Unstable

Symptom: over/under plate polarity flickers at adjacent samples or across
deterministic replays.

Likely cause: signed convergence velocity is evaluated at unstable points near
triple junctions or exact boundaries.

Evidence: polarity flips cluster where q1/q2 or boundary nearest points are
nearly equidistant.

Detection: log signed velocity margin and ambiguous-polarity count. Ambiguous
contacts must be reported, not silently resolved.

## 6. Third-Plate Intrusion Becomes Misclassified Subduction

Symptom: triple junctions generate subduction labels between the wrong pair of
plates.

Likely cause: the two-nearest-plate assumption is used where three plates
contribute non-degenerate intersections.

Evidence: top plate triples explain most unresolved overlaps or material loss.

Detection: keep third-plate intrusion as its own class. Do not fold it into
ordinary convergent overlap.

## 7. Material Transfer Looks Conserved Only In Aggregate

Symptom: global CAF stays near 0.30, but individual continents smear or jump
between plates.

Likely cause: scalar CAF is conserved while provenance is not.

Evidence: per-plate continental area changes compensate globally; tagged
continental features have large discontinuities after resampling.

Detection: track per-plate continental mass and tagged-feature centroid drift,
not just global CAF.

## 8. Performance Cliff

Symptom: Phase II works at 10k/60k but cannot approach the paper budget at
250k.

Likely cause: contact detection performs all-pairs boundary searches or rebuilds
too many spatial indices.

Evidence: contact labeling time dominates projection and resampling time.

Detection: report timing slices for motion, projection, contact detection,
triangle labeling, filtering, resampling, and mesh rebuild.

## Required Negative Controls

- Zero-motion: no contacts, no filtered triangles, no material change.
- Forced divergence: gaps only, no subduction labels.
- Forced convergence: contacts appear with known polarity and filtered
  triangles.
- Polarity swap: identical setup with over/under polarity swapped changes the
  filtered plate.
- Single plate: no contacts and no event effects.
- All-continental: no spurious oceanization from subduction labels alone.
- Ocean-only: no spurious continental material from subduction labels alone.
- Triple junction fixture: third-plate intrusion remains separately classified.
