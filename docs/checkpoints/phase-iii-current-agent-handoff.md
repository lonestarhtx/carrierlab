# CarrierLab Phase III Current Agent Handoff

Date: 2026-05-17

Status: handoff only. This is not a checkpoint pass and not a go/no-go approval.

## Current Snapshot

Repository: `C:\Users\Michael\Documents\Unreal Projects\CarrierLab`

Current HEAD: `991ad69 Resolve IIIE6 mixed-material multi-hit holds`

Current state: Phase IIIE live remesh now applies at editor default scale, IIIE.6.12 is closed as a remesh coherence diagnostic, and IIIF has a written substrate checkpoint with `PASS_IIIF_CRUST_FIELD_SUBSTRATE`. Plate/material/projection coherence and crust-field substrate stability are now separate gates. The old unstable-crust no-go is resolved for the editor-default IIIF substrate scenario, but IIIE still needs its consolidation checkpoint before IIIG implementation work is treated as the next phase.

Do not proceed to rifting implementation or performance optimization as if the whole tectonic stack is complete. First record IIIE consolidation clearly, then start IIIG as Rifting / Divergence on top of the IIIF substrate gate.

## Dirty Worktree Warning

The worktree currently has unstaged visualization/bathymetry edits:

- `Source/CarrierLab/Private/CarrierLabPhaseIIIEVisualValidationCommandlet.cpp`
- `Source/CarrierLab/Private/CarrierLabPhaseIIIObservabilityCommandlet.cpp`
- `Source/CarrierLab/Private/CarrierLabVisualizationActor.cpp`
- `Source/CarrierLab/Public/CarrierLabVisualizationActor.h`
- `Source/CarrierLabEditor/Private/SCarrierLabControlPanel.cpp`

These were not part of the committed IIIE.6.11 mixed-material fix. Do not accidentally include them in a remesh stability commit unless explicitly intended.

## Project Objective

CarrierLab is a clean-room falsification lab for the Cortial et al. moving plate-local crust carrier model. It is not Aurous 2 and should not import Aurous ownership-recovery or sidecar infrastructure.

The goal is not just to make commandlets pass. The goal is to determine whether the paper-style carrier can survive motion and remesh while preserving coherent plate-local crust, continents, ocean generation, and process provenance.

Global samples must not become tectonic authority. Plate-local carrier state is the authority; global samples are projection and remesh targets.

## IIIE.6.X Timeline

- `e9a08f5` promoted IIIE.6 live remesh cadence.
- `cebf593` coalesced same-plate coincident duplicate hits.
- `2684d95` diagnosed cross-plate and third-plate multi-hit holds.
- `550fcd5` verified architecture: thesis/paper use global TDS plus duplicated plate-local triangulations, but are silent on same-distance multi-valid tie-breaks.
- `d9a250f` implemented shared-boundary tie-break hierarchy: continental fraction, older oceanic age, lower plate id.
- `6b10130` diagnosed post-motion live cadence multi-hit holds.
- `9868073` added unique-nearest resolution for cross-plate and third-plate hits.
- `73f0cf7` added distance-tie fallback.
- `30ffd03` diagnosed apply-path invalid records.
- `470743c` researched non-separating zero-hit generation.
- `c16c603` restored paper-literal zero-hit generation by demoting the signed-separation veto to diagnostic-only.
- `c0d7b48` diagnosed manual-cadence unsupported holds at editor defaults. The dominant class was `MixedMaterial -> UnsupportedHeld`.
- `991ad69` amended nearest-hit resolution to include strict unique-nearest mixed-material cases.

## Important Correction

Several earlier commandlet rows labeled as default were actually ocean-only because they used `ContinentalPlateFraction = 0.0`. The editor default uses land coverage around `0.30`.

That mismatch hid mixed-material failures until the user clicked remesh in the actual editor. Any future default-scale gate must assert the actor fields it uses, especially sample count, plate count, seed, land fraction, velocity, process flags, and policy flags.

## Previous No-Go Resolved By IIIF

At `991ad69`, the live actor applies instead of holding. One observed HUD line after manual remesh at step 16:

```text
phase_iii_e6_live_apply
gen=33493 applied=24644 rift_pending=8849 nonpos_sep=16404
coalesced=0 shared_tiebreak=0 nearest_hit=27757
distance_tie_fallback=3 majority=32307 tj_split=508
```

This was mechanically successful, but the `PlateId` layer looked visually incoherent after remesh. The working interpretation at the time was:

- IIIE.6.11 fixed "apply is blocked."
- It did not establish "carrier remains stable after remesh."
- This is now a qualitative and structural stability problem, not another generic apply-path blocker.

Do not overclaim that the full paper carrier works from this alone. The terrain/crust-field part of this no-go is now handled by the IIIF checkpoint, but rifting, convergence/uplift cleanup, surface processes, and long-run validation still have their own gates.

## 2026-05-17 Direction Update

IIIE.6.12 should not keep expanding. Its scoped conclusion is:

- Plate/material/projection coherence after remesh is now largely diagnosable and no longer the only blocker.
- Terrain plausibility is not validated. The latest diagnosis reports `Post.oceanic_max_elevation_km = 202.340798`, global post max elevation `844.363566 km`, and `45705` oceanic samples above sea level.
- The likely source is crust-field authority drift: plate-local vertices, global samples, remesh records, and visualization do not yet enforce one coherent elevation/oceanic-age lifecycle.

Maintain the corrected Phase III sub-phase structure:

- IIIE remains paper remesh / divergent-zone oceanic crust generation.
- IIIF is Crust Field Substrate: elevation authority, oceanic age lifecycle, bathymetry bounds, uplift bounds, and sample/plate-vertex sync.
- IIIG is Rifting / Divergence: continental thinning, rift-pending state, breakup threshold, new oceanic crust generation, ridge age reset, passive margin creation, and divergent bathymetry.
- IIIH is Convergence / Uplift Cleanup: subduction, obduction, collision, slab pull, uplift bounds, and conservation checks on the stabilized substrate.
- IIII is Surface Processes: erosion, sediment, isostatic relaxation, and smoothing.
- IIIJ is Long-Run World Validation.

The completed IIIF gate is:

```text
IIIF Crust Field Substrate
```

IIIF demonstrates and enforces crust-field authority for its checkpoint scenario before IIIG rifting and IIII surface processes proceed. Surface processes must not be used to hide runaway uplift; rifting must not be built on incoherent oceanic age or bathymetry.

Approved IIIF completion token:

```text
PASS_IIIF_CRUST_FIELD_SUBSTRATE
```

The IIIF diagnostic map set includes a crust substrate classification map that explains apparent "land in ocean" visuals by separating continental land, continental shelf/submerged crust, oceanic bathymetry, sea-level oceanic clamp, generated oceanic crust, rifting-pending continental preservation, and invalid oceanic above-sea samples.

## Likely Risk Areas

The current leading hypotheses are:

- Mixed-material nearest-hit is material-blind and may choose the nearest geometric candidate even across a continental/oceanic material boundary.
- The IIIE.5 topology rebuild majority rule may be producing salt-and-pepper `PlateId` identity after rebuild.
- Rifting-pending prevents continental overwrite in one path, but it does not by itself guarantee coherent plate identity.
- Selection closure and apply closure are too weak as success criteria. They do not establish post-remesh carrier stability.

Do not treat these as confirmed root causes. They are hypotheses to test.

## Known IIIE Lab Policies

The provisional IIIE lab-policy ledger is:

1. zGamma sqrt-subsidence with `d_ref = 3000 km`.
2. 2-of-3 mixed-plate global triangle majority rule.
3. Triple-junction centroid split.
4. Continental overwrite routed to rifting-pending.
5. Shared-boundary tie-break hierarchy: continental fraction, older oceanic age, lower plate id.
6. Nearest-valid-hit for post-motion cross/third multi-hit, amended in IIIE.6.11 to include mixed-material strict unique-nearest.
7. Distance-tie fallback using the same hierarchy.

The `c16c603` non-positive-separation change is not a new lab policy. It removed an uncited CarrierLab veto and restored paper-literal zero-hit generation.

This ledger is provisional until IIIE consolidation, which should not start while the current stability no-go is unresolved.

## Verification Evidence For `991ad69`

Reported verification before the current visual no-go:

- Build passed.
- `CarrierLabPhaseIIIE65NearestHit` passed.
- `CarrierLabPhaseIIIE66DistanceTieFallback` passed.
- `CarrierLabPhaseIIIE610ManualCadenceUnsupported` passed selection sweep and targeted live apply proof.
- `git diff --check` passed.
- `carrierlab-claim-audit -ChangedOnly`: clean.
- `carrierlab-remesh-guard` ran; heuristic advisory hits remained expected due remesh-policy terms in code/docs.
- `CarrierLabPhaseIIIE3` printed gate pass/result 0, but Unreal returned 1 due local Zen/DDC writable-cache noise. Treat that as machine cache noise unless reproduced with gate failures.

## Superseded Recommended Next Task

The previous next task was to create a diagnostic-only slice:

```text
IIIE.6.12 Continental / Plate Coherence After Remesh Diagnosis
```

Do not add another resolver first.

The goal is to determine whether the visual shredding is just a `PlateId` or visualization artifact, or whether continental/material carrier stability is actually broken.

Required diagnostics:

- Pre/post connected components of continental material.
- Pre/post global CAF and per-plate CAF.
- Count continental samples whose `PlateId` changes.
- Fragmentation/salt-pepper metric for `PlateId` and continental material.
- Attribute changed samples by resolver/source class:
  - mixed-material nearest
  - cross/third nearest
  - distance-tie fallback
  - generated ocean
  - rifting-pending
  - majority topology rebuild
  - triple-junction split
- Compare layers:
  - `PlateId`
  - material / `ContinentalFraction`
  - `PhaseIIISummary`
  - oceanic age
  - elevation
- Generate diagnostic contact sheets, but report numbers first, images second, verdict last.

That task has been performed as IIIE.6.12 and is now closed for planning purposes. Do not keep adding unrelated terrain work to IIIE.6.12.

## Recommended Next Task

Write the IIIE consolidation checkpoint if it is still missing, then begin IIIG: Rifting / Divergence.

The IIIE consolidation should:

- Close IIIE as remesh/resampling, not as full terrain validation.
- Preserve the IIIE.6.12 finding that plate/material/projection coherence and crust-field stability are separate concerns.
- Point to the IIIF checkpoint as the substrate gate that resolved the crust-field blocker.
- Avoid claiming rifting, convergence/uplift cleanup, surface processes, or long-run validation are complete.

The first IIIG slice should be:

```text
IIIG.1: Rift Candidate And Thinning Dry Run
```

Required diagnostics and gates:

- Select a forced test plate and compute rift candidacy from continental coverage, plate area, and configured rift probability inputs.
- Produce audit-only thinning and rift-pending records without topology mutation.
- Report pre/post IIIF invariants to show the dry run does not mutate crust fields.
- Confirm candidate selection and thinning records are deterministic for fixed seed.
- Confirm no topology mutation occurs.

Do not start IIIH convergence/uplift cleanup or IIII surface processes until IIIG rifting/divergence has its own passed checkpoint.

## Decision Tree

If material and continental components are stable but `PlateId` looks noisy, investigate topology rebuild and visualization before changing material semantics.

If continents are fragmented or material coherence is lost, IIIE.6.11 should be considered suspect and likely redesigned or disabled as default.

If the topology majority rebuild is the culprit, revisit IIIE.5 majority and triple-junction policies before further live-remesh work.

If the architecture cannot preserve carrier stability without an expanding policy stack, mark this CarrierLab IIIE implementation path as falsified or no-go. Do not hide that result behind another patch.

## Performance Note

A single 100k remesh is currently around five minutes. That is not acceptable for live use, but performance work should be deferred. Optimizing a path that may be semantically wrong would waste effort and make bad behavior harder to inspect.

## Instructions To The Next Agent

Treat this as a project-level go/no-go, not a routine bug.

Do not claim that IIIE works because the live actor mutates. The current proof is weaker: selection and apply no longer hold, but qualitative carrier stability appears to fail.

Start with evidence. Reproduce the editor default state, capture pre/post metrics, and separate `PlateId` noise from true material/continent instability. Keep paper authority, named lab policy, and forbidden authority drift distinct.

The user is rightly concerned because this is exactly the failure mode earlier projects hit: remesh applies, but coherence collapses. Be direct, careful, and do not over-reassure.
