# Phase IIIE Diagnostic Contact Sheets

Verdict: PASS / DIAGNOSTIC CONTACT SHEETS WRITTEN FOR PROMOTED IIIE.6 CHAIN. This checkpoint exports dedicated diagnostic PNG contact sheets for the IIIE.3 -> IIIE.4 -> IIIE.5 -> IIIE.6 bounded chain as it exists today. It is a visual regression diagnostic for the promoted IIIE.6 cadence; it does not itself mutate the editor's live actor, relax unresolved multi-hit holds, or claim images are proof.

## Scope

- The export runs the same bounded IIIE.6 event cases: no-hit divergent ocean creation and filter-exhausted continental rifting-pending route.
- Each replay writes pre-state maps, a selection/generation card, post-rebuild maps, and a ledger card into one contact sheet.
- The commandlet is a read-only export diagnostic; live auto-remesh promotion is owned by the IIIE.6 commandlet/report, not by these images.
- The filter-exhausted continental case now records `rifting pending`: IIIE.4 provenance is visible, but no generated oceanic record is applied over continental material.

## Gates

| Gate | Result | Evidence |
|---|---:|---|
| maps written | pass | output root `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z` |
| `no_hit_oceanic_creation` replay A/B | pass | replay A `pass`, replay B `pass`, map hashes stable `yes` |
| `no_hit_oceanic_creation` forbidden policy counters | pass | policy/prior/projection `0/0/0`, unresolved routed `0` |
| `filter_exhausted_continental_rifting_pending` replay A/B | pass | replay A `pass`, replay B `pass`, map hashes stable `yes` |
| `filter_exhausted_continental_rifting_pending` forbidden policy counters | pass | policy/prior/projection `0/0/0`, unresolved routed `0` |

## Exported Contact Sheets

| Scenario | Replay | Selection | Applied generated | New ocean | Rifting pending | Overwritten | Topology hash | Contact sheet |
|---|---:|---|---:|---:|---:|---:|---|---|
| `no_hit_oceanic_creation` | 0 | `no-hit divergent gap` | 1 | 1 | 0 | 0 | `4f877056e71d554d` | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_0/ContactSheet.png` |
| `no_hit_oceanic_creation` | 1 | `no-hit divergent gap` | 1 | 1 | 0 | 0 | `4f877056e71d554d` | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_1/ContactSheet.png` |
| `filter_exhausted_continental_rifting_pending` | 0 | `divergent gap after filtering` | 0 | 0 | 1 | 0 | `727f492f1e99c50e` | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_0/ContactSheet.png` |
| `filter_exhausted_continental_rifting_pending` | 1 | `divergent gap after filtering` | 0 | 0 | 1 | 0 | `727f492f1e99c50e` | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_1/ContactSheet.png` |

## Exported Layers

| Scenario | Replay | Layer | Hash | Non-background pixels | Path |
|---|---:|---|---|---:|---|
| `no_hit_oceanic_creation` | 0 | `PrePlateId` | `7247524875b2fa86` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_0/PrePlateId.png` |
| `no_hit_oceanic_creation` | 0 | `PreCrustType` | `7961b30d24421606` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_0/PreCrustType.png` |
| `no_hit_oceanic_creation` | 0 | `PreElevation` | `7961b30d24421606` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_0/PreElevation.png` |
| `no_hit_oceanic_creation` | 0 | `SelectionAndGeneration` | `c51c3ee3cf245816` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_0/SelectionAndGeneration.png` |
| `no_hit_oceanic_creation` | 0 | `PostPlateId` | `7247524875b2fa86` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_0/PostPlateId.png` |
| `no_hit_oceanic_creation` | 0 | `PostCrustType` | `7961b30d24421606` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_0/PostCrustType.png` |
| `no_hit_oceanic_creation` | 0 | `PostElevation` | `cff15680243c3480` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_0/PostElevation.png` |
| `no_hit_oceanic_creation` | 0 | `PostOceanicAge` | `d1085712494112ea` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_0/PostOceanicAge.png` |
| `no_hit_oceanic_creation` | 0 | `PostRidgeDirection` | `ad66f86ae8b942fa` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_0/PostRidgeDirection.png` |
| `no_hit_oceanic_creation` | 0 | `PostPhaseIIIERemesh` | `df14694d1941d9a4` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_0/PostPhaseIIIERemesh.png` |
| `no_hit_oceanic_creation` | 0 | `LedgerAndHolds` | `812e374f9a8f16f6` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_0/LedgerAndHolds.png` |
| `no_hit_oceanic_creation` | 1 | `PrePlateId` | `7247524875b2fa86` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_1/PrePlateId.png` |
| `no_hit_oceanic_creation` | 1 | `PreCrustType` | `7961b30d24421606` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_1/PreCrustType.png` |
| `no_hit_oceanic_creation` | 1 | `PreElevation` | `7961b30d24421606` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_1/PreElevation.png` |
| `no_hit_oceanic_creation` | 1 | `SelectionAndGeneration` | `c51c3ee3cf245816` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_1/SelectionAndGeneration.png` |
| `no_hit_oceanic_creation` | 1 | `PostPlateId` | `7247524875b2fa86` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_1/PostPlateId.png` |
| `no_hit_oceanic_creation` | 1 | `PostCrustType` | `7961b30d24421606` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_1/PostCrustType.png` |
| `no_hit_oceanic_creation` | 1 | `PostElevation` | `cff15680243c3480` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_1/PostElevation.png` |
| `no_hit_oceanic_creation` | 1 | `PostOceanicAge` | `d1085712494112ea` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_1/PostOceanicAge.png` |
| `no_hit_oceanic_creation` | 1 | `PostRidgeDirection` | `ad66f86ae8b942fa` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_1/PostRidgeDirection.png` |
| `no_hit_oceanic_creation` | 1 | `PostPhaseIIIERemesh` | `df14694d1941d9a4` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_1/PostPhaseIIIERemesh.png` |
| `no_hit_oceanic_creation` | 1 | `LedgerAndHolds` | `812e374f9a8f16f6` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/no_hit_oceanic_creation/replay_1/LedgerAndHolds.png` |
| `filter_exhausted_continental_rifting_pending` | 0 | `PrePlateId` | `7247524875b2fa86` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_0/PrePlateId.png` |
| `filter_exhausted_continental_rifting_pending` | 0 | `PreCrustType` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_0/PreCrustType.png` |
| `filter_exhausted_continental_rifting_pending` | 0 | `PreElevation` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_0/PreElevation.png` |
| `filter_exhausted_continental_rifting_pending` | 0 | `SelectionAndGeneration` | `d20fb4b74053bd16` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_0/SelectionAndGeneration.png` |
| `filter_exhausted_continental_rifting_pending` | 0 | `PostPlateId` | `7247524875b2fa86` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_0/PostPlateId.png` |
| `filter_exhausted_continental_rifting_pending` | 0 | `PostCrustType` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_0/PostCrustType.png` |
| `filter_exhausted_continental_rifting_pending` | 0 | `PostElevation` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_0/PostElevation.png` |
| `filter_exhausted_continental_rifting_pending` | 0 | `PostOceanicAge` | `4ab63b0402b1c65a` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_0/PostOceanicAge.png` |
| `filter_exhausted_continental_rifting_pending` | 0 | `PostRidgeDirection` | `ad66f86ae8b942fa` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_0/PostRidgeDirection.png` |
| `filter_exhausted_continental_rifting_pending` | 0 | `PostPhaseIIIERemesh` | `4ab63b0402b1c65a` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_0/PostPhaseIIIERemesh.png` |
| `filter_exhausted_continental_rifting_pending` | 0 | `LedgerAndHolds` | `e4026df41a467bf6` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_0/LedgerAndHolds.png` |
| `filter_exhausted_continental_rifting_pending` | 1 | `PrePlateId` | `7247524875b2fa86` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_1/PrePlateId.png` |
| `filter_exhausted_continental_rifting_pending` | 1 | `PreCrustType` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_1/PreCrustType.png` |
| `filter_exhausted_continental_rifting_pending` | 1 | `PreElevation` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_1/PreElevation.png` |
| `filter_exhausted_continental_rifting_pending` | 1 | `SelectionAndGeneration` | `d20fb4b74053bd16` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_1/SelectionAndGeneration.png` |
| `filter_exhausted_continental_rifting_pending` | 1 | `PostPlateId` | `7247524875b2fa86` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_1/PostPlateId.png` |
| `filter_exhausted_continental_rifting_pending` | 1 | `PostCrustType` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_1/PostCrustType.png` |
| `filter_exhausted_continental_rifting_pending` | 1 | `PostElevation` | `259a365d0028cc06` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_1/PostElevation.png` |
| `filter_exhausted_continental_rifting_pending` | 1 | `PostOceanicAge` | `4ab63b0402b1c65a` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_1/PostOceanicAge.png` |
| `filter_exhausted_continental_rifting_pending` | 1 | `PostRidgeDirection` | `ad66f86ae8b942fa` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_1/PostRidgeDirection.png` |
| `filter_exhausted_continental_rifting_pending` | 1 | `PostPhaseIIIERemesh` | `4ab63b0402b1c65a` | 1647140 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_1/PostPhaseIIIERemesh.png` |
| `filter_exhausted_continental_rifting_pending` | 1 | `LedgerAndHolds` | `e4026df41a467bf6` | 524288 | `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/filter_exhausted_continental_rifting_pending/replay_1/LedgerAndHolds.png` |

## Interpretation

- `PrePlateId`, `PreCrustType`, and `PreElevation` show the bounded fixture before any IIIE mutation.
- `SelectionAndGeneration` is a card, not a map. It records the IIIE.3 route, raw/post-filter candidate counts, q1/q2 provenance, and zGamma authority flags.
- `PostPlateId`, `PostCrustType`, `PostElevation`, `PostOceanicAge`, `PostRidgeDirection`, and `PostPhaseIIIERemesh` show the state after the accepted IIIE.5 duplicate/re-index/re-compact helper.
- `LedgerAndHolds` is the audit card for the IIIE.6 ledger line. The continental case should show `RIFT PENDING 1` and `OVERWRITTEN 0`.
- The maps are human-spatial diagnostics only. The commandlet gates determinism, read-only export, no forbidden fallback counters, and expected ledger classification; it does not replace the numeric IIIE.2-6 commandlets.

## Next Required Work

1. Keep the contact-sheet export as a regression diagnostic after live promotion.
2. Use IIIE consolidation, not visual inspection alone, to disclose lab-policy choices and rerun the numeric gate chain.

Metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIEVisualValidation/20260508T010829Z/metrics.jsonl`.
