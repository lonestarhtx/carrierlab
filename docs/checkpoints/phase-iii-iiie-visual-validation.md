# Phase IIIE Diagnostic Contact Sheets

Verdict: PASS / DIAGNOSTIC CONTACT SHEETS WRITTEN; LIVE PROMOTION STILL HELD. This checkpoint exports dedicated diagnostic PNG contact sheets for the IIIE.3 -> IIIE.4 -> IIIE.5 -> IIIE.6 bounded chain as it exists today. It does not promote IIIE.6 into live cadence, resolve majority assignment, implement triple-junction handling, or convert continental-overwrite holds into production behavior.

## Scope

- The export runs the same bounded IIIE.6 event cases: no-hit divergent ocean creation and filter-exhausted continental-overwrite hold.
- Each replay writes pre-state maps, a selection/generation card, post-rebuild maps, and a ledger/hold card into one contact sheet.
- The commandlet deliberately leaves live auto-resampling on the existing comparison path. These images are diagnostic artifacts for the audited helper chain, not proof and not a production-cadence claim.
- The filter-exhausted continental case remains a hold. Its contact sheet is expected to show the generated oceanic signal while the ledger card marks the overwrite hold explicitly.

## Gates

| Gate | Result | Evidence |
|---|---:|---|
| maps written | pass | output root `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z` |
| `no_hit_oceanic_creation` replay A/B | pass | replay A `pass`, replay B `pass`, map hashes stable `yes` |
| `no_hit_oceanic_creation` forbidden policy counters | pass | policy/prior/projection `0/0/0`, unresolved routed `0` |
| `filter_exhausted_continental_overwrite_hold` replay A/B | pass | replay A `hold`, replay B `hold`, map hashes stable `yes` |
| `filter_exhausted_continental_overwrite_hold` forbidden policy counters | pass | policy/prior/projection `0/0/0`, unresolved routed `0` |

## Exported Contact Sheets

| Scenario | Replay | Selection | Generated | New ocean | Overwrite hold | Topology hash | Contact sheet |
|---|---:|---|---:|---:|---:|---|---|
| `no_hit_oceanic_creation` | 0 | `no-hit divergent gap` | 1 | 1 | 0 | `f4e282af487e88e7` | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_0/ContactSheet.png` |
| `no_hit_oceanic_creation` | 1 | `no-hit divergent gap` | 1 | 1 | 0 | `f4e282af487e88e7` | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_1/ContactSheet.png` |
| `filter_exhausted_continental_overwrite_hold` | 0 | `divergent gap after filtering` | 1 | 0 | 1 | `f9a8e8b58238b830` | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_0/ContactSheet.png` |
| `filter_exhausted_continental_overwrite_hold` | 1 | `divergent gap after filtering` | 1 | 0 | 1 | `f9a8e8b58238b830` | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_1/ContactSheet.png` |

## Exported Layers

| Scenario | Replay | Layer | Hash | Non-background pixels | Path |
|---|---:|---|---|---:|---|
| `no_hit_oceanic_creation` | 0 | `PrePlateId` | `7247524875b2fa86` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_0/PrePlateId.png` |
| `no_hit_oceanic_creation` | 0 | `PreCrustType` | `7961b30d24421606` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_0/PreCrustType.png` |
| `no_hit_oceanic_creation` | 0 | `PreElevation` | `7961b30d24421606` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_0/PreElevation.png` |
| `no_hit_oceanic_creation` | 0 | `SelectionAndGeneration` | `c51c3ee3cf245816` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_0/SelectionAndGeneration.png` |
| `no_hit_oceanic_creation` | 0 | `PostPlateId` | `7247524875b2fa86` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_0/PostPlateId.png` |
| `no_hit_oceanic_creation` | 0 | `PostCrustType` | `7961b30d24421606` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_0/PostCrustType.png` |
| `no_hit_oceanic_creation` | 0 | `PostElevation` | `cff15680243c3480` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_0/PostElevation.png` |
| `no_hit_oceanic_creation` | 0 | `PostOceanicAge` | `d1085712494112ea` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_0/PostOceanicAge.png` |
| `no_hit_oceanic_creation` | 0 | `PostRidgeDirection` | `ad66f86ae8b942fa` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_0/PostRidgeDirection.png` |
| `no_hit_oceanic_creation` | 0 | `PostPhaseIIIERemesh` | `df14694d1941d9a4` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_0/PostPhaseIIIERemesh.png` |
| `no_hit_oceanic_creation` | 0 | `LedgerAndHolds` | `b26ad41d51c2a176` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_0/LedgerAndHolds.png` |
| `no_hit_oceanic_creation` | 1 | `PrePlateId` | `7247524875b2fa86` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_1/PrePlateId.png` |
| `no_hit_oceanic_creation` | 1 | `PreCrustType` | `7961b30d24421606` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_1/PreCrustType.png` |
| `no_hit_oceanic_creation` | 1 | `PreElevation` | `7961b30d24421606` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_1/PreElevation.png` |
| `no_hit_oceanic_creation` | 1 | `SelectionAndGeneration` | `c51c3ee3cf245816` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_1/SelectionAndGeneration.png` |
| `no_hit_oceanic_creation` | 1 | `PostPlateId` | `7247524875b2fa86` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_1/PostPlateId.png` |
| `no_hit_oceanic_creation` | 1 | `PostCrustType` | `7961b30d24421606` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_1/PostCrustType.png` |
| `no_hit_oceanic_creation` | 1 | `PostElevation` | `cff15680243c3480` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_1/PostElevation.png` |
| `no_hit_oceanic_creation` | 1 | `PostOceanicAge` | `d1085712494112ea` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_1/PostOceanicAge.png` |
| `no_hit_oceanic_creation` | 1 | `PostRidgeDirection` | `ad66f86ae8b942fa` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_1/PostRidgeDirection.png` |
| `no_hit_oceanic_creation` | 1 | `PostPhaseIIIERemesh` | `df14694d1941d9a4` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_1/PostPhaseIIIERemesh.png` |
| `no_hit_oceanic_creation` | 1 | `LedgerAndHolds` | `b26ad41d51c2a176` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/no_hit_oceanic_creation/replay_1/LedgerAndHolds.png` |
| `filter_exhausted_continental_overwrite_hold` | 0 | `PrePlateId` | `7247524875b2fa86` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_0/PrePlateId.png` |
| `filter_exhausted_continental_overwrite_hold` | 0 | `PreCrustType` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_0/PreCrustType.png` |
| `filter_exhausted_continental_overwrite_hold` | 0 | `PreElevation` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_0/PreElevation.png` |
| `filter_exhausted_continental_overwrite_hold` | 0 | `SelectionAndGeneration` | `d20fb4b74053bd16` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_0/SelectionAndGeneration.png` |
| `filter_exhausted_continental_overwrite_hold` | 0 | `PostPlateId` | `7247524875b2fa86` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_0/PostPlateId.png` |
| `filter_exhausted_continental_overwrite_hold` | 0 | `PostCrustType` | `28f22c7867173956` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_0/PostCrustType.png` |
| `filter_exhausted_continental_overwrite_hold` | 0 | `PostElevation` | `91c43ced5ccc3ed0` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_0/PostElevation.png` |
| `filter_exhausted_continental_overwrite_hold` | 0 | `PostOceanicAge` | `85a0cebb00ff209c` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_0/PostOceanicAge.png` |
| `filter_exhausted_continental_overwrite_hold` | 0 | `PostRidgeDirection` | `ad66f86ae8b942fa` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_0/PostRidgeDirection.png` |
| `filter_exhausted_continental_overwrite_hold` | 0 | `PostPhaseIIIERemesh` | `c3172582c2aa064b` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_0/PostPhaseIIIERemesh.png` |
| `filter_exhausted_continental_overwrite_hold` | 0 | `LedgerAndHolds` | `cda16dc97f119736` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_0/LedgerAndHolds.png` |
| `filter_exhausted_continental_overwrite_hold` | 1 | `PrePlateId` | `7247524875b2fa86` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_1/PrePlateId.png` |
| `filter_exhausted_continental_overwrite_hold` | 1 | `PreCrustType` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_1/PreCrustType.png` |
| `filter_exhausted_continental_overwrite_hold` | 1 | `PreElevation` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_1/PreElevation.png` |
| `filter_exhausted_continental_overwrite_hold` | 1 | `SelectionAndGeneration` | `d20fb4b74053bd16` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_1/SelectionAndGeneration.png` |
| `filter_exhausted_continental_overwrite_hold` | 1 | `PostPlateId` | `7247524875b2fa86` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_1/PostPlateId.png` |
| `filter_exhausted_continental_overwrite_hold` | 1 | `PostCrustType` | `28f22c7867173956` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_1/PostCrustType.png` |
| `filter_exhausted_continental_overwrite_hold` | 1 | `PostElevation` | `91c43ced5ccc3ed0` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_1/PostElevation.png` |
| `filter_exhausted_continental_overwrite_hold` | 1 | `PostOceanicAge` | `85a0cebb00ff209c` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_1/PostOceanicAge.png` |
| `filter_exhausted_continental_overwrite_hold` | 1 | `PostRidgeDirection` | `ad66f86ae8b942fa` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_1/PostRidgeDirection.png` |
| `filter_exhausted_continental_overwrite_hold` | 1 | `PostPhaseIIIERemesh` | `c3172582c2aa064b` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_1/PostPhaseIIIERemesh.png` |
| `filter_exhausted_continental_overwrite_hold` | 1 | `LedgerAndHolds` | `cda16dc97f119736` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/filter_exhausted_continental_overwrite_hold/replay_1/LedgerAndHolds.png` |

## Interpretation

- `PrePlateId`, `PreCrustType`, and `PreElevation` show the bounded fixture before any IIIE mutation.
- `SelectionAndGeneration` is a card, not a map. It records the IIIE.3 route, raw/post-filter candidate counts, q1/q2 provenance, and zGamma authority flags.
- `PostPlateId`, `PostCrustType`, `PostElevation`, `PostOceanicAge`, `PostRidgeDirection`, and `PostPhaseIIIERemesh` show the state after the accepted IIIE.5 duplicate/re-index/re-compact helper.
- `LedgerAndHolds` is the audit card for the IIIE.6 ledger line. The continental-overwrite fixture is supposed to remain a hold until the rifting-pending route is implemented.
- The maps are human-spatial diagnostics only. The commandlet gates determinism, read-only export, no forbidden fallback counters, and expected ledger classification; it does not replace the numeric IIIE.2-6 commandlets.

## Next Required Work

1. Add the IIIE consolidation paragraph that explicitly approves or scopes the two-of-three majority assignment rule.
2. Add the triple-junction centroid-split slice so one-one-one global triangles stop being a live-cadence hold.
3. Convert continental ridge overwrite into a rifting-pending route instead of a IIIE production hold.
4. Promote IIIE.6 to live cadence only after those live-geometry cases have explicit handling.

Metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260507T035548Z/metrics.jsonl`.
