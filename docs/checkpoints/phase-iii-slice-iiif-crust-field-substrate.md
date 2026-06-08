# IIIF: Crust Field Substrate Checkpoint

Date: 2026-05-17

Verdict: **PASS_IIIF_CRUST_FIELD_SUBSTRATE**.

IIIF is closed for the editor-default live-remesh substrate scenario. The crust fields are finite, bounded, age-bearing after remesh, and synchronized from authoritative samples into plate-local vertex records.

## Scope

IIIF is the shared crust-field substrate that later tectonic processes consume. It establishes:

- elevation authority
- oceanic age lifecycle
- bathymetry bounds
- uplift bounds
- sample/plate vertex sync
- visual classification that distinguishes real substrate classes from invalid state

IIIF does **not** implement rifting, erosion, or long-horizon climate/terrain equilibrium. IIIG rifting, IIIH convergence/uplift cleanup, IIII surface processes, and IIIJ long-run validation remain downstream of this checkpoint. Surface processes are not allowed to hide impossible crust-field values.

## Authority Decision

The authoritative post-remesh crust record is the global sample record. Plate-local vertices must mirror the sample fields after remesh, not diverge into their own elevation, material, or oceanic-age state.

The remaining visual concern from the earlier bathymetry maps was resolved by classifying substrate state directly. The apparent land/ocean ambiguity is not corrupted live vertex records: post-remesh sample and plate vertex fields match exactly for material, elevation, and oceanic age. The classification map separates sea-level oceanic clamps and rifting-pending continental preservation from invalid oceanic-above-sea samples.

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
| oceanic age lifecycle | true |
| manual remesh step | 16 |

## Exit Gate Evidence

| Gate | Result | Evidence |
|---|---:|---|
| Oceanic age lifecycle | pass | pre plate positive `74004`, post plate positive `46423`, post sample positive `42326` |
| Pre-remesh plate-local bounds | pass | elevation `-10.000000..5.087743 km`; oceanic max `0.000000 km`; allowed `-20.000000..10.000000 km`, oceanic `<= 0.000000 km` |
| Manual remesh applied | pass | `phase_iii_e6_live_apply gen=33775 applied=24926 rift_pending=8849 nonpos_sep=16557 material_preserved=6927 mixed_material_preserved=1932 plate_regularized=8791 components=692_to_40 coalesced=0 shared_tiebreak=0 nearest_hit=26044 distance_tie_fallback=3 majority=16703 tj_split=76` |
| Remesh coherence audit | pass | `PASS_COHERENT`: material, authoritative plate, and projected plate coherence stayed inside diagnostic thresholds |
| Post-remesh crust bounds | pass | sample `-10.000000..4.657125 km`, sample oceanic max `0.000000 km`; plate `-10.000000..4.657125 km`, plate oceanic max `0.000000 km` |
| Post-remesh vertex records | pass | material mismatches `0`, field mismatches `0`, max elevation delta `0.000000000 km`, max age delta `0.000000000 Ma` |

## Core Measurements

| Metric | Pre samples | Post samples | Pre plate vertices | Post plate vertices |
|---|---:|---:|---:|---:|
| elevation range km | 0.000000..0.000000 | -10.000000..4.657125 | -10.000000..5.087743 | -10.000000..4.657125 |
| oceanic elevation range km | 0.000000..0.000000 | -10.000000..0.000000 | -10.000000..0.000000 | -10.000000..0.000000 |
| oceanic above sea level strict | 0 | 0 | 0 | 0 |
| oceanic positive age count | 0 | 42326 | 74004 | 46423 |
| oceanic max age Ma | 0.000000 | 32.000000 | 32.000000 | 32.000000 |
| invalid crust fields | 0 | 0 | 0 | 0 |

## Authority Coherence

| Metric | Pre | Post |
|---|---:|---:|
| plate vertices with sample ids | 105929 | 110600 |
| material mismatches | 0 | 0 |
| field mismatches | 105918 | 0 |
| max elevation delta km | 10.000000000 | 0.000000000 |
| mean elevation delta km | 0.969939888 | 0.000000000 |
| max oceanic age delta Ma | 32.000000000 | 0.000000000 |

The pre-remesh mismatch count is expected diagnostic evidence that plate-local copies had not yet been synchronized with the global sample record. The post-remesh zero mismatch result is the closure condition.

## Remesh Context

| Metric | Value |
|---|---:|
| remesh verdict | `PASS_COHERENT` |
| generated candidates | 33775 |
| applied generated | 24926 |
| rifting pending | 8849 |
| material-preserved records | 6927 |
| plate regularized samples | 8791 |
| oceanic strict above-sea growth fraction | 0.000000000 |

## Substrate Classification

| Class | Pre samples | Post samples | Interpretation |
|---|---:|---:|---|
| continental land | 30104 | 21255 | Valid continental samples above sea level. |
| continental shelf/submerged crust | 0 | 0 | Valid continental samples at or below sea level. |
| oceanic bathymetry | 0 | 660 | Valid oceanic samples below sea level. |
| sea-level oceanic clamp | 69896 | 44310 | Valid oceanic samples clamped at sea level, not land. |
| generated oceanic crust | 0 | 24926 | Valid remesh-generated oceanic crust. |
| rifting-pending continental preservation | 0 | 8849 | Valid preserved continental material awaiting a downstream rifting process. |
| oceanic above-sea invalid | 0 | 0 | Must remain zero. |
| invalid / non-finite | 0 | 0 | Must remain zero. |

This table is the main answer to the "why are there still landmasses in the ocean?" concern. The post-remesh maps include continental preservation and sea-level oceanic clamp classes that can look like terrain mass in a bathymetric-only render, but they are not invalid oceanic uplift. The invalid class count is zero.

## Visual Artifacts

Contact sheet:
`C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIFCrustFieldSubstrate/ContactSheet.png`

Metrics:
`C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIFCrustFieldSubstrate/phase-iii-slice-iiif-crust-field-substrate.jsonl`

Output root:
`C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIFCrustFieldSubstrate`

| Image | Hash | Why it is included |
|---|---|---|
| `PreStatePlateId` | `8783cecf07aaa5e9` | Baseline authoritative sample ownership before remesh. |
| `PreContinentalFraction` | `48b53893d0e724ef` | Baseline material map before remesh; used to detect continental fragmentation. |
| `PreBathymetricElevation` | `cb6579bfa55de686` | Baseline global-sample terrain. Plate-local metrics show the hidden live carrier elevation before sync. |
| `PreCrustSubstrateClass` | `cae2d1dccc155350` | Baseline substrate classification. |
| `PreOceanicAge` | `40928199f376bcb6` | Baseline global-sample oceanic age; compare with plate-local age metrics. |
| `PostBathymetricElevation` | `836346584da4763c` | Primary visual check for impossible post-remesh landmass or oceanic uplift. |
| `PostCrustSubstrateClass` | `53edcbc86e91d398` | Primary interpretive map for IIIF closure. |
| `PostOceanicAge` | `28255a9eb4e84c7f` | Checks whether aged oceanic crust survives remesh while generated crust starts young. |
| `PostPhaseIIIERemeshSummary` | `a3327cc046f1fa40` | Shows generated oceanic and rifting-pending remesh provenance. |
| `PostPlateProjectionMismatch` | `e7068cc480cab84a` | Separates authoritative state problems from projected visualization mismatch. |
| `PostProjectionDiagnostics` | `bcc7557053d3f346` | Condenses miss, overlap, and boundary-ray health into one projection QA map. |

## Validation

The remote Windows checkout was validated with:

- `CarrierLabEditor Win64 Development` build: passed
- `CarrierLabPhaseIIIFCrustFieldSubstrate` commandlet: passed with `PASS_IIIF_CRUST_FIELD_SUBSTRATE`
- `Automation RunTests CarrierLab.Stage0`: passed 2/2 tests, exit code 0
- `git diff --check`: clean aside from existing CRLF warnings

The IIIF commandlet wrote this checkpoint data and the visual artifacts under `Saved/CarrierLab/PhaseIII/IIIFCrustFieldSubstrate`.

## Residual Risk

This checkpoint demonstrates the substrate gate for the editor-default live-remesh scenario. It does not claim:

- long-horizon geological equilibrium
- finished rifting behavior
- finished erosion behavior
- visual quality of final Phase IV terrain amplification
- proof that every future process preserves these invariants

Those are downstream gates. Future IIIG rifting, IIIH convergence/uplift cleanup, IIII surface processes, and terrain amplification work must preserve the IIIF invariants instead of compensating for broken fields after the fact.

## Closure Decision

IIIF is accepted as the crust-field substrate gate. Work may proceed to the next planned Phase III slice only if it treats this checkpoint as a dependency:

- oceanic samples must not drift above sea level without being diagnosed
- generated oceanic crust must have coherent age lifecycle semantics
- sample records remain authoritative after remesh
- plate-local vertex records must sync exactly from samples after remesh
- classification maps must continue to expose invalid state instead of hiding it in bathymetry colors
