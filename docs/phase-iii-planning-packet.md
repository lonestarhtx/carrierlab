# CarrierLab Phase III Planning Packet

Status: draft for user review. Phase III implementation may not start until the user records an explicit go/no-go against this packet.

## Packet Files

- `docs/phase-iii-paper-process-design.md`: ADR-shaped design contract. Eight sub-phases (IIIA–IIIH), authority rules, data model, sub-phase architectural decisions, gates, non-goals.
- `docs/phase-iii-pre-mortem.md`: ten ranked failure modes, required negative controls, and stop conditions.
- `docs/phase-iii-slice-plan.md`: per-sub-phase tiny slices with goals, work items, exit gates, and per-slice checkpoint artifacts.

Supporting reread material from CODEX (already in tree, untracked, to be landed with this packet):

- `docs/phase-iii-paper-process-extraction.md`: process-level reread of paper §3 / thesis §3.
- `docs/phase-iii-figure-reading.md`: figure-level reread.

## Grounding

Phase II is closed (commit `4ad8148`, see `docs/phase-ii-closeout.md`). The tested carrier mechanics reproduce faithfully within the Phase I/II scope; runtime is ~6× under paper Table 2 budget for the measured kernel. Slice 5.5 produced a load-bearing finding: continental loss in the resampling ledger is *not* mixed-triangle barycentric smear (only 0.7% of single-hit records, contributing -0.004 of the -0.375 net at 60k). It is *coherent transfer* — continental samples reading from uniformly-oceanic interior triangles of plates that have moved into their position. The asymmetry is consistent with continental plates losing area at convergent zones because no collision/suture process exists.

Phase III is therefore not "fix the carrier." It is "reconstruct the paper processes whose absence produces the Slice 5.5 asymmetry." The architectural answer to that asymmetry is continental collision (sub-phase IIID), which transfers continental material rather than losing it.

## Phase III Goal

Reconstruct the paper's full tectonic process layer on top of the validated Phase I/II carrier foundation, in eight sub-phases sequenced storage → tracking → mutation → topology → events → equilibrium:

- IIIA: paper crust state schema (storage only)
- IIIB: convergence tracking (read-only)
- IIIC: continuous subduction/obduction (mutation, including slab-pull feedback into carrier authority)
- IIID: continental collision (topology mutation; the architectural answer to Slice 5.5)
- IIIE: divergent zone / oceanic crust generation (full process plus ledger reframe)
- IIIF: plate rifting (discrete event)
- IIIG: per-step elevation evolution
- IIIH: tectonic-only long-horizon validation

The success condition is narrow:

- All sub-phases checkpointed and gated.
- Long-horizon Auth CAF equilibrates rather than drifts.
- Slice 5.5 continental-loss asymmetry quantifiably reduced after IIID, with ≥80% reduction in the uniform-oceanic-source bucket as the target threshold. Missing that threshold pauses advancement for an investigation checkpoint rather than being treated as an automatic design verdict.
- Slab pull deterministic and bounded.
- No persistent global sample ownership introduced. Centroid/random remain comparison controls only.
- Performance at 250k within paper Table 2 budget for the kernel; Phase III event costs reported separately.

## Non-Negotiables

- Plate-local crust geometry remains carrier authority.
- Global samples remain projection/resampling targets, never persistent tectonic authority.
- Process state may persist across timesteps within a remesh window. It is invalidated at every remesh per thesis §3.3.2.3.
- Slab pull is the only place process output feeds back into carrier authority. It is opt-in, hash-gated separately, and always has a parallel "off" baseline computed for differential testing.
- Centroid and random tie-breaks remain comparison controls only.
- No terrain beauty, displaced geometry, exemplar synthesis, or amplification — those belong to Phase IV.
- No additional carrier numerical hardening beyond what Phase III state requires.
- Optimization acceptance is hash-gated as in Phase II: same seed produces identical projection and carrier-state hashes before and after each commit.
- Every sub-phase ends with a written checkpoint and explicit go/no-go.

## Review Questions

1. Is the eight-sub-phase decomposition (IIIA–IIIH) acceptable, with the storage → tracking → mutation → topology → events → equilibrium sequencing as binding?
2. Do you approve continental collision (IIID) as the architectural answer to Slice 5.5, with the IIID.8 quantification target (≥80% reduction in uniform-oceanic-source continental loss, investigated if missed rather than tuned around)?
3. Do you approve slab pull (IIIC.4) as the single allowed process-to-carrier-authority feedback, with the off/on differential gate?
4. Do you approve the Phase III gates as the success condition (long-horizon equilibrium, deterministic replay, Slice 5.5 reduction, kernel within Table 2)?
5. Do you approve the non-negotiable that Phase IV (amplification) is downstream and never bleeds into Phase III scope, including the "vertex positions remain on the unit sphere" stop condition?
6. Do you accept that Phase III sub-phases will land slowly, with per-slice checkpoints for tiny units of work (e.g., IIIA has 4 sub-slices, IIID has 8), to keep change blast radius small?
7. Are the Phase III pre-mortem's ten failure modes the right ones to instrument against, or are there project-specific ones (e.g., Unreal-engine-specific topology concerns) that should be added before approval?

## Proposed Approval Statement

If accepted, record:

`Phase III planning packet approved. Begin sub-phase IIIA only. No subsequent sub-phase is approved beyond IIIA's first slice (inert Elevation field, hash-gated against Phase II baseline). Sub-phase IIIA will be reviewed at consolidation before IIIB starts; subsequent sub-phases are reviewed at their own consolidation.`

## Out-Of-Scope For This Packet

- Phase IV (amplification) planning. A separate Phase IV planning packet will follow Phase III closeout. Per the agreed roadmap, Phase IV will lead with thesis §4.2 hyper-amplification; §4.1 collage is optional comparison/control.
- Phase V (integrated reproduction + comparison) planning. The current intended comparison tier is: paper Table 2 performance and paper-process diagnostics as required quantitative checks; paper-class macro-morphology as required qualitative comparison; geophysical realism as aspirational validation. This tier is recorded here only as a planning note and will be formalized in a separate Phase V packet as Phase IV closes.

## Tidy Items Before Packet Lands

These untracked items should be folded into the same commit as the Phase III planning packet:

- `docs/ProceduralTectonicPlanets/` (paper PDF + page PNGs)
- `docs/Synthèse de terrain à léchelle planétaire/` (thesis PDF + page PNGs)
- `docs/phase-iii-paper-process-extraction.md` (CODEX reread)
- `docs/phase-iii-figure-reading.md` (CODEX reread)
- `docs/phase-iii-paper-process-design.md` (this packet)
- `docs/phase-iii-pre-mortem.md` (this packet)
- `docs/phase-iii-slice-plan.md` (this packet)
- `docs/phase-iii-planning-packet.md` (this file)

That commit should land *before* any IIIA implementation work begins.
