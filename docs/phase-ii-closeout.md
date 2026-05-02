# CarrierLab Phase II Closeout

Status: complete.

Phase II extended the validated carrier foundation with the minimum process-coupled subduction machinery needed to answer the Stage 1.5 open question: whether explicit process state can make convergent multi-hit filtering auditable without reintroducing Aurous-style ownership recovery. It did not implement full paper subduction, collision, rifting, slab pull, uplift, erosion, terrain rendering, or amplification.

## Scope Closed

- Slice 0: preserved the Phase I baseline and optimized the projection kernel without changing hashes.
- Slice 1: added deterministic, read-only contact detection.
- Slice 2: added fixture-only triangle polarity labels with explicit same-material ambiguity and third-plate out-of-scope handling.
- Slice 3: wired subducting triangle labels into the resampling filter.
- Slice 4: added material accounting with named ledger categories and closed audit equations.
- Slice 5: scaled the Slice 3+4 stack across 60k, 100k, 250k, and 500k.
- Slice 5.5: subdivided single-hit ledger records by hit-triangle material uniformity.

## Evidence

The full Slice 5 scaling checkpoint passes through 500k samples. Average step-kernel times remain below the paper Table 2 total rows even though CarrierLab is running diagnostics around the process stack:

| Samples | Kernel s/step | Paper Table 2 s/step |
|---:|---:|---:|
| 60k | 0.029110 | 0.19 |
| 100k | 0.057007 | 0.28 |
| 250k | 0.129427 | 1.24 |
| 500k | 0.275426 | 1.90 |

Material ledger reconciliation remains closed at floating-point scale. The Slice 5 primary rows report continental residuals from `3.827e-13` at 60k to `1.342e-12` at 500k, with max per-plate residuals at or below `1.094e-13`.

The Slice 5.5 targeted 60k run adds source-triangle provenance for single-hit transfers. The important result:

| Source triangle class | Records | Net continental delta |
|---|---:|---:|
| Uniform continental | 7,057 | +0.148074 |
| Uniform oceanic | 16,742 | -0.518782 |
| Mixed | 168 | -0.004269 |
| Unknown | 0 | 0.000000 |

This means the single-hit continental delta is not primarily mixed-triangle barycentric smear. It is mostly coherent transfer into uniformly oceanic hit triangles, partly offset by coherent transfer into uniformly continental triangles. That is process-state interpretation work for the paper reproduction path, not evidence of a Phase II carrier numerical failure.

## Conclusions

Phase II validates the foundation needed for paper-process work:

- Contact records, triangle labels, filter decisions, material ledger rows, and post-state hashes are deterministic under replay.
- Third-plate evidence is explicit and not silently converted into two-plate subduction.
- Same-material convergent contacts remain ambiguous, as they should without collision/age/process state.
- The subducting-triangle filter is structurally clean and does not need ownership recovery or global sample authority.
- The material ledger can explain every active continental delta through named categories.

The remaining material deltas are now categorized handoff questions:

- Gap-fill deltas belong with paper-faithful oceanic crust generation and ridge-state tracking.
- Same-material multi-hit cases belong with collision and oceanic-age polarity.
- Triple-junction evidence needs a separate model and must stay out of two-plate labels until specified.
- Single-hit transfers into uniform oceanic triangles need full crust/process fields before they can be judged as legitimate creation, process consumption, or missing preservation logic.

## Explicit Non-Claims

Phase II does not claim full paper reproduction. It does not implement the thesis subduction state machine, collision, divergence/rifting, slab pull feedback, uplift, global elevation evolution, erosion/dampening/sediment accretion, or amplification.

Phase II does claim that the carrier plus deterministic contact/label/filter/accounting substrate is ready to receive those paper processes under the same slice discipline.

## Handoff

Do not keep extending Phase II. The next work should begin only after a separate Phase III planning packet is accepted. That packet should be about paper-process reproduction, not more carrier rescue work.
