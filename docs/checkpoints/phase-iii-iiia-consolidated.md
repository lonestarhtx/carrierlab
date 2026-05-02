# Phase IIIA Consolidated Checkpoint

Date: 2026-05-02

Branch: `master`

Scope: Phase IIIA crust-state schema and inert interpolation support.

## Verdict

Phase IIIA passes. The carrier now has the Phase III crust-state fields needed for paper-process work:

- `Elevation` in kilometers
- `OceanicAge` in mega-annum
- `RidgeDirection` as tangent vector state
- `FoldDirection` as tangent vector state

The fields are included in the additive `crust_state_hash`, survive deterministic replay, and do not alter the existing Phase II authority hashes or material-ledger behavior when they are zero. No subduction, collision, rifting, uplift, erosion, sediment, amplification, repair, recovery, ownership persistence, or terrain rendering behavior was introduced.

Recommendation: accept Phase IIIA and require explicit user go/no-go before beginning Phase IIIB.1.

## Shared Phase II Baseline

The Phase IIIA slices were compared against the accepted Phase II resampling/filter/accounting baseline.

| Metric | Value | Notes |
|---|---:|---|
| Post-resampling state hash | `3b4a85366dab80db` | Phase II-compatible state authority |
| Material ledger hash | `bc3077100ba291b4` | Phase II material accounting |
| Projection replay hash after resampling | `a411b6aad7877a55` | Deterministic projection replay |

Slice 5.5 did not publish a separate Phase II baseline projection hash. IIIA therefore relies on the accepted Phase II hashes above plus same-slice replay determinism for the new crust-state hash.

## Slice Results

### IIIA.1 - Elevation Field

Added inert `Elevation` scalar state to global samples and plate-local vertices.

| Gate | Result |
|---|---:|
| Crust state replay hash A | `8e1bd41e6b0225b6` |
| Crust state replay hash B | `8e1bd41e6b0225b6` |
| Max global sample elevation before/after resampling | `0.000e+00 km` |
| Max plate-local vertex elevation before/after resampling | `0.000e+00 km` |

Result: passed. Elevation storage is inert and deterministic.

### IIIA.2 - Oceanic Age Field

Added inert `OceanicAge` scalar state to global samples and plate-local vertices.

| Gate | Result |
|---|---:|
| Crust state replay hash A | `29d14205f7fe6fc4` |
| Crust state replay hash B | `29d14205f7fe6fc4` |
| Max sample elevation | `0.000e+00 km` |
| Max sample oceanic age | `0.000e+00 Ma` |
| Max plate vertex oceanic age | `0.000e+00 Ma` |

Result: passed. Oceanic age storage is inert and deterministic.

### IIIA.3 - Direction Fields

Added inert tangent vector fields `RidgeDirection` and `FoldDirection`. Non-zero vectors rotate with plate geodetic motion and remain tangent.

| Gate | Result |
|---|---:|
| Production crust state replay hash A | `a4e4e99de216c31c` |
| Production crust state replay hash B | `a4e4e99de216c31c` |
| Max production sample vector magnitude | `0.000e+00` |
| Max production plate vertex vector magnitude | `0.000e+00` |
| Forced ridge rotation oracle error | `2.719e-16 rad` |
| Forced fold rotation oracle error | `4.996e-16 rad` |
| Forced position oracle error | `5.495e-16 rad` |
| Forced vector replay crust hash A | `a39d037a6b669e74` |
| Forced vector replay crust hash B | `a39d037a6b669e74` |

Result: passed. Direction fields are inert in production and rotate correctly when explicitly seeded for oracle testing.

### IIIA.4 - Field Interpolation

Added field interpolation through the resampling path. Valid ray hits barycentric-interpolate `Elevation`, `OceanicAge`, `RidgeDirection`, and `FoldDirection`. Interpolated vectors are projected back to the tangent plane and normalized when non-zero. Gap-fill fields stay zero until IIIE gives ridge creation its paper-process semantics.

| Gate | Result |
|---|---:|
| Zero-field crust state replay hash A | `a4e4e99de216c31c` |
| Zero-field crust state replay hash B | `a4e4e99de216c31c` |
| Zero-field non-zero global samples | `0` |
| Zero-field non-zero plate vertices | `0` |
| Boundary smear samples observed | `29` |
| Boundary smear elevation range | `0.572750..10.000000 km` |
| Boundary smear replay hash A | `867333924f292f10` |
| Boundary smear replay hash B | `867333924f292f10` |
| Gap-fill samples checked | `21273` |
| Max gap-fill elevation | `0.000e+00 km` |
| Max gap-fill vector magnitude | `0.000e+00` |
| Max interpolated-vector radial dot | `1.665e-16` |

Result: passed. Field interpolation is deterministic, tangent-safe, and inactive for zero-field production runs. Boundary smear is visible and expected when seeded fields cross resampling boundaries; it is documented as the first explicit evidence that future process fields need source-aware semantics, not hidden smoothing.

## Consolidated Gates

| Gate | Result | Evidence |
|---|---|---|
| Phase II authority hashes preserved | Pass | `state_hash=3b4a85366dab80db`, `ledger_hash=bc3077100ba291b4` |
| Additive field-aware hash exists | Pass | `crust_state_hash` changes as fields are added |
| Same-seed replay deterministic | Pass | All IIIA replay hash pairs match |
| Zero fields remain zero | Pass | IIIA.1-4 report zero max values and zero non-zero production counts |
| Vector fields remain tangent | Pass | Max radial dot `1.665e-16` in IIIA.4 |
| Forced vector rotation has independent oracle | Pass | Direction and position errors are floating-point noise |
| Gap-fill does not fabricate process fields | Pass | 21,273 gap-fill samples, zero elevation/vector fields |
| No new mutation processes | Pass | IIIA only added state storage, rotation, hashing, and interpolation |

## Deferred Work

- IIIE will give gap-fill ridge creation explicit paper-process semantics for oceanic age, ridge direction, and initial oceanic elevation.
- IIIC and IIID will mutate elevation and fold direction through subduction and collision.
- IIIG will implement per-step erosion, oceanic dampening, and sediment accretion.
- Phase IV handles terrain amplification; vertex positions remain on the unit sphere through Phase III.

## Go / No-Go Recommendation

Go for Phase IIIB.1 after user approval.

The next slice should begin tracking paper-process provenance on top of the now-complete inert crust-state schema. It should not introduce process mutation until its own tracking gates are written and reviewed.
