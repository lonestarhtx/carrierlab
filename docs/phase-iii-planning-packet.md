# CarrierLab Phase III Planning Packet

Status: draft for user review. Phase III implementation may not start until the user records an explicit go/no-go against this packet.

## Packet Files

- `docs/phase-iii-paper-process-design.md`: ADR-shaped design contract. Ten sub-phases (IIIA–IIIJ), authority rules, data model, sub-phase architectural decisions, gates, non-goals.
- `docs/phase-iii-pre-mortem.md`: ten ranked failure modes, required negative controls, and stop conditions.
- `docs/phase-iii-slice-plan.md`: per-sub-phase tiny slices with goals, work items, exit gates, and per-slice checkpoint artifacts.
- `docs/paper-resampling-extraction.md`: focused paper/thesis extraction for the §3.3.2.3 remesh operation. This supersedes earlier resampling notes where they treated Stage 1.5 as standalone.

Supporting reread material:

- `docs/phase-iii-paper-process-extraction.md`: process-level reread of paper §3 / thesis §3.
- `docs/phase-iii-figure-reading.md`: figure-level reread.

## Grounding

Phase II is closed (commit `4ad8148`, see `docs/phase-ii-closeout.md`). Stage 0/1 validated the paper carrier substrate within their scope; Stage 1.5 remains foundation characterization, not the thesis remesh path. Its evidence is split: pre-remesh miss/overlap accumulation is a rigid-window projection finding, while remesh/material preservation requires Phase III process state before it can claim thesis alignment. The paper/thesis remesh operation consumes Phase III process state: subducting and colliding triangles are filtered before remesh rays, and continental terranes persist through collision/suture before the next remesh. Runtime is ~6× under paper Table 2 budget for the measured kernel. Slice 5.5 produced a load-bearing finding: continental loss in the resampling ledger is *not* mixed-triangle barycentric smear (only 0.7% of single-hit records, contributing -0.004 of the -0.375 net at 60k). It is *coherent transfer* — continental samples reading from uniformly-oceanic interior triangles of plates that have moved into their position. The asymmetry is consistent with continental plates losing area at convergent zones because no collision/suture process exists.

Phase III is therefore not "fix the carrier" through a projection tie-break. It is "reconstruct the paper processes whose absence produces the Slice 5.5 asymmetry." The architectural answer to continental persistence is continental collision (sub-phase IIID), which transfers continental material before remesh. The architectural answer to thesis-aligned resampling is IIIE, which must implement the full §3.3.2.3 remesh rather than merely extending the old Stage 1.5 lab path.

## 2026-05-17 Direction Update

IIIF is the crust-field substrate/stability layer, not rifting.

Current direction:

- Close IIIE.6.12 as a remesh coherence diagnostic, not as full terrain validation.
- Define **IIIF: Crust Field Substrate** as the shared field rules later tectonic processes depend on.
- Promote rifting/divergence into **IIIG**, after IIIF has passed.
- Reintegrate convergence/uplift cleanup in **IIIH** on top of the stabilized substrate.
- Keep surface processes in **IIII** so erosion, sediment, isostatic relaxation, and smoothing do not hide invalid tectonic state.
- Close Phase III with **IIIJ** long-run world validation.
- The IIIF substrate pass token is `PASS_IIIF_CRUST_FIELD_SUBSTRATE`.

Reason: IIIE.6.12 showed that remesh can preserve plate/material/projection coherence while exposing impossible crust fields (`202+ km` oceanic elevation and `844+ km` global elevation). IIIF diagnosed and bounded that substrate. Rifting now gets its own phase, convergence/uplift gets a cleanup phase, and erosion remains a realism/stability process rather than a patch over broken uplift.

## Phase III Goal

Reconstruct the paper's full tectonic process layer on top of the measured
Stage 0/1 carrier substrate and Phase II process scaffolding, in ten
sub-phases sequenced storage → tracking → mutation → topology → remesh →
substrate → divergence → cleanup → surface processes → validation:

- IIIA: paper crust state schema (storage only)
- IIIB: convergence tracking (read-only)
- IIIC: continuous subduction/obduction (mutation, including slab-pull feedback into carrier authority)
- IIID: continental collision (topology mutation; the architectural answer to Slice 5.5)
- IIIE: paper remesh / divergent-zone oceanic crust generation (full §3.3.2.3 integration plus ledger reframe)
- IIIF: crust field substrate (shared elevation, oceanic age, bathymetry, uplift, and sample/plate-vertex sync rules)
- IIIG: rifting / divergence
- IIIH: convergence / uplift cleanup
- IIII: surface processes (erosion, sediment, isostatic relaxation, smoothing)
- IIIJ: long-run world validation

IIIF substrate scope:

- IIIF Crust Field Substrate: explicit authority and invariants for elevation, historical elevation, oceanic age, ridge direction, and fold direction across plate-local vertices, global samples, remesh records, and visualization. Its diagnostics must include a crust substrate classification map that separates continental land, continental shelf/submerged crust, oceanic bathymetry, sea-level oceanic clamp, generated oceanic crust, and rifting-pending continental preservation.

The success condition is narrow:

- All sub-phases checkpointed and gated.
- IIIF reports `PASS_IIIF_CRUST_FIELD_SUBSTRATE` before IIIG rifting begins.
- IIIH demonstrates bounded convergence/uplift before IIII surface processes begin.
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
- Centroid and random tie-breaks remain comparison controls only. They are not paper-faithful remesh policy once IIIB/IIIC/IIID state exists.
- No terrain beauty, displaced geometry, exemplar synthesis, or amplification — those belong to Phase IV.
- No additional carrier numerical hardening beyond what Phase III state requires.
- Optimization acceptance is hash-gated as in Phase II: same seed produces identical projection and carrier-state hashes before and after each commit.
- Every sub-phase ends with a written checkpoint and explicit go/no-go.

## Review Questions

1. Is the ten-sub-phase decomposition (IIIA–IIIJ) acceptable, with the storage → tracking → mutation → topology → remesh → substrate → divergence → cleanup → surface processes → validation sequencing as binding?
2. Do you approve the Stage 1.5 reframing: standalone Stage 1.5 remains foundation characterization, while thesis-aligned remeshing is owned by IIIE after convergence/collision state exists?
3. Do you approve continental collision (IIID) as the architectural answer to Slice 5.5, with the IIID.8 quantification target (≥80% reduction in uniform-oceanic-source continental loss, investigated if missed rather than tuned around)?
4. Do you approve slab pull (IIIC.4) as the single allowed process-to-carrier-authority feedback, with the off/on differential gate?
5. Do you approve the Phase III gates as the success condition (long-horizon equilibrium, deterministic replay, Slice 5.5 reduction, kernel within Table 2)?
6. Do you approve the non-negotiable that Phase IV (amplification) is downstream and never bleeds into Phase III scope, including the "vertex positions remain on the unit sphere" stop condition?
7. Do you accept that Phase III sub-phases will land slowly, with per-slice checkpoints for tiny units of work (e.g., IIIA has 4 sub-slices, IIID has 8), to keep change blast radius small?
8. Are the Phase III pre-mortem's failure modes the right ones to instrument against, or are there project-specific ones (e.g., Unreal-engine-specific topology concerns) that should be added before approval?

## Proposed Approval Statement

If accepted, record:

`Phase III planning packet approved. Begin sub-phase IIIA only. No subsequent sub-phase is approved beyond IIIA's first slice (inert Elevation field, hash-gated against Phase II baseline). Sub-phase IIIA will be reviewed at consolidation before IIIB starts; subsequent sub-phases are reviewed at their own consolidation.`

Recorded follow-up approvals:

- 2026-05-03: Stage 1.5 reframing approved after
  `docs/paper-resampling-extraction.md` review. Standalone Stage 1.5 remains
  foundation characterization; paper-faithful remeshing is owned by IIIE after
  convergence/collision state exists. IIID.1 is unblocked as the next staged
  lab slice, not as a claim that full PTP/Cortial alignment is complete.

## Out-Of-Scope For This Packet

- Phase IV (amplification) planning. A separate Phase IV planning packet will follow Phase III closeout. Per the agreed roadmap, Phase IV will lead with thesis §4.2 hyper-amplification; §4.1 collage is optional comparison/control.
- Phase V (integrated reproduction + comparison) planning. The current intended comparison tier is: paper Table 2 performance and paper-process diagnostics as required quantitative checks; paper-class macro-morphology as required qualitative comparison; geophysical realism as aspirational validation. This tier is recorded here only as a planning note and will be formalized in a separate Phase V packet as Phase IV closes.

## Tidy Items Before Packet Lands

Historical note from the original packet landing. These files were the packet
contents to keep together before Phase III began:

- `docs/ProceduralTectonicPlanets/` (paper PDF + page PNGs)
- `docs/Synthèse de terrain à léchelle planétaire/` (thesis PDF + page PNGs)
- `docs/phase-iii-paper-process-extraction.md` (CODEX reread)
- `docs/phase-iii-figure-reading.md` (CODEX reread)
- `docs/phase-iii-paper-process-design.md` (this packet)
- `docs/phase-iii-pre-mortem.md` (this packet)
- `docs/phase-iii-slice-plan.md` (this packet)
- `docs/phase-iii-planning-packet.md` (this file)

That commit should land *before* any IIIA implementation work begins.
