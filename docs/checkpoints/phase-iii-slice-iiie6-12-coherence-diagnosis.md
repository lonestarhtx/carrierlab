# Phase IIIE.6.12 Coherence Diagnosis

Verdict: **PASS_COHERENT_WITH_TERRAIN_BLOCKER**. material, authoritative plate, and projected plate coherence stayed inside diagnostic thresholds, but terrain plausibility is blocked by unstable crust-field state.

This is a diagnostic-only slice for the editor-default manual remesh at step 16. It does not add a resolver, change remesh policy, or treat global samples as tectonic authority.

## 2026-05-17 Direction Update

This slice is closed as a remesh coherence diagnostic. Do not keep expanding IIIE.6.12 into crust-field lifecycle, rifting, erosion, or long-run validation work.

The IIIE.6.12 result is:

- Plate/material/projection coherence is no longer the only blocker.
- Terrain plausibility is not validated.
- The post-remesh bathymetric map is now sourced from authoritative sample elevation, and it exposes impossible crust-field state rather than only a projected visualization artifact.
- The next task is `IIIF: Crust Field Substrate`, documented in `docs/checkpoints/phase-iii-slice-iiif-crust-field-substrate.md`.

The corrected Phase III structure is: IIIE owns remesh, IIIF owns the crust-field substrate, and rifting stays downstream until IIIF is closed.

## Scenario

| Field | Value |
|---|---:|
| sample count | 100000 |
| plate count | 40 |
| seed | 42 |
| continental plate fraction | 0.30 |
| velocity mm/yr | 66.6666666667 |
| Phase III process layer | on |
| slab pull | off |
| mixed-material nearest hit | on |
| non-separating veto | off |
| manual remesh step | 16 |

## Numbers First

| Metric | Pre | Post | Delta |
|---|---:|---:|---:|
| authoritative CAF | 0.301040000 | 0.301017385 | -0.000022615 |
| projected CAF | 0.275672895 | 0.301017385 | 0.025344490 |
| continental components | 1 | 1 | 0 |
| largest continental component size | 30104 | 30104 | 0.000000000 loss |
| state plate components | 40 | 40 | 0.000000000 growth |
| projected plate components | 40 | 40 | 0.000000000 growth |
| state plate salt-pepper | 22 | 1413 | 0.013910000 growth |
| projected plate salt-pepper | 22 | 1413 | 0.013910000 growth |
| material salt-pepper | 7 | 7 | 0 |
| above-sea-level samples | 100000 | 75809 | -0.241910000 growth |
| continental above-sea-level samples | 30104 | 30104 | 0.000000000 growth |
| oceanic above-sea-level samples | 69896 | 45705 | -0.346099920 growth |
| elevation range km | 0.000000..0.000000 | -10.000000..844.363566 | mean 0.000000 -> 48.298942 |
| oceanic elevation range km | 0.000000..0.000000 | -10.000000..202.340798 | mean 0.000000 -> -0.196345 |

## Changed Samples

| Metric | Count | Fraction |
|---|---:|---:|
| state `PlateId` changed | 57183 | 0.571830000 |
| continental state `PlateId` changed | 14700 | 0.488307202 |
| projected `PlateId` changed | 57183 | 0.571830000 |
| continental projected `PlateId` changed | 14700 | 0.488307202 |
| material class changed | 0 | 0.000000000 |
| material class changed / pre-continental | 0 | 0.000000000 |

## Attribution For State Plate Changes

| Source class | Count |
|---|---:|
| mixed-material nearest | 2088 |
| cross/third nearest | 15409 |
| distance-tie fallback | 2 |
| generated ocean | 15511 |
| rifting-pending | 574 |
| majority rebuild context | 9712 |
| triple-junction context | 157 |
| shared boundary | 0 |
| coalesced duplicate | 0 |
| other | 22186 |

## Remesh Counters

| Counter | Value |
|---|---:|
| selection audit ran | true |
| remesh applied | true |
| last remesh mode | `phase_iii_e6_live_apply gen=33493 applied=24644 rift_pending=8849 nonpos_sep=16404 material_preserved=6911 mixed_material_preserved=1890 plate_regularized=10330 components=723_to_40 coalesced=0 shared_tiebreak=0 nearest_hit=27757 distance_tie_fallback=3 majority=16498 tj_split=74` |
| generated candidates | 33493 |
| applied generated | 24644 |
| rifting pending | 8849 |
| material-preserved resolved records | 6911 |
| mixed-material nearest material-preserved | 1890 |
| plate-component regularized samples | 10330 |
| nearest hit | 27757 |
| mixed-material nearest | 4301 |
| distance-tie fallback | 3 |
| triple-junction splits | 74 |

## Images

Contact sheet: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE612CoherenceDiagnosis/ContactSheet.png`

| Image | Hash | Path |
|---|---|---|
| `PreStatePlateId` | `8783cecf07aaa5e9` | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE612CoherenceDiagnosis/PreStatePlateId.png` |
| `PreContinentalFraction` | `48b53893d0e724ef` | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE612CoherenceDiagnosis/PreContinentalFraction.png` |
| `PreBathymetricElevation` | `cb6579bfa55de686` | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE612CoherenceDiagnosis/PreBathymetricElevation.png` |
| `PostStatePlateId` | `3fef168b0f55055c` | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE612CoherenceDiagnosis/PostStatePlateId.png` |
| `PostPlateProjectionMismatch` | `e7068cc480cab84a` | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE612CoherenceDiagnosis/PostPlateProjectionMismatch.png` |
| `PostContinentalFraction` | `d5013e17c674e4fe` | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE612CoherenceDiagnosis/PostContinentalFraction.png` |
| `PostPhaseIIIERemeshSummary` | `8ee4ea0de0f192bd` | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE612CoherenceDiagnosis/PostPhaseIIIERemeshSummary.png` |
| `PostProjectionDiagnostics` | `efb3053f2cd4f224` | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE612CoherenceDiagnosis/PostProjectionDiagnostics.png` |
| `PostBathymetricElevation` | `f13011d1ee2d8a0b` | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE612CoherenceDiagnosis/PostBathymetricElevation.png` |

## Map Purposes

| Image | Why it is included |
|---|---|
| `PreStatePlateId` | Baseline authoritative plate ownership before the manual remesh. |
| `PreContinentalFraction` | Baseline continental/oceanic material distribution before the remesh. |
| `PreBathymetricElevation` | Baseline sea-level and terrain read before the remesh. |
| `PostStatePlateId` | Authoritative plate ownership after remesh; this is the primary topology result. |
| `PostPlateProjectionMismatch` | Render/state agreement check; green means the viewport plate stream matches authoritative state. |
| `PostContinentalFraction` | Post-remesh material distribution; compare with the baseline material map. |
| `PostPhaseIIIERemeshSummary` | Where remesh-generated/rifting records affected the surface. |
| `PostProjectionDiagnostics` | Projection ray health in one view: red=miss, orange=overlap, white=plate boundary. |
| `PostBathymetricElevation` | Human-readable terrain outcome after remesh, sourced from authoritative sample elevation. |

## Interpretation Boundaries

- `FAIL_MATERIAL_COHERENCE` means the continental/material carrier changed beyond the diagnostic threshold, regardless of how the `PlateId` map looks.
- Material-preserved resolved records keep pre-remesh crustal fields when a non-divergent resolved hit would otherwise flip material class; they do not restore old `PlateId` ownership.
- Plate-component regularization absorbs detached post-selection ownership fragments into adjacent post-selection plates; it does not query or restore pre-remesh owners.
- `StatePlateId` maps render authoritative `State.Samples[*].PlateId`; `PlateProjectionMismatch` is green for agreement, orange for disagreement, and red for missing IDs.
- `ProjectionDiagnostics` consolidates the lower-level miss, overlap, and boundary maps so the contact sheet stays focused.
- Projection ray casts still drive diagnostic masks and projected continental fraction; viewport `PlateId` and bathymetric elevation should not perform a second ownership/elevation solve after remesh.
- `PASS_UNSAFE_REMESH_HELD` means the default live-remesh path detected that threshold before mutation and refused to apply the remesh.
- `FAIL_PLATE_COHERENCE` means material survived, but authoritative state `PlateId` connectivity/salt-pepper degraded beyond threshold.
- `FAIL_ELEVATION_COHERENCE` means plate/material coherence held but oceanic above-sea-level samples grew beyond the diagnostic threshold.
- This report's current elevation gate is known to be insufficient because the pre-remesh baseline had all oceanic samples at exactly sea level and the first gate tested growth rather than absolute oceanic elevation plausibility.
- `PASS_VISUAL_ARTIFACT_LIKELY` is only allowed when material and authoritative state plate metrics hold while projected `PlateId` degrades.
- Attribution counts are overlapping diagnostic tags, not an exclusive partition.

Metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE612CoherenceDiagnosis/phase-iii-slice-iiie6-12-coherence-diagnosis.jsonl`.
Output root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE612CoherenceDiagnosis`.
