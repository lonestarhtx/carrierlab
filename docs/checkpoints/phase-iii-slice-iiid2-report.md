# Phase III Slice IIID.2 Report - Collision Candidate Grouping And Interpenetration Gate

Generated: `2026-05-03 06:14:18` UTC

## Scope

IIID.2 groups IIID.1 continental terrane evidence by opposing plate pair and applies the paper's 300 km interpenetration threshold as a read-only gate. It does not detach terranes, mutate topology, apply sutures, alter plate motion, resample, or change carrier authority.

Metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIID2/20260503T061331Z/metrics.jsonl`

## Gate Summary

| Gate | Result | Evidence |
|---|---:|---|
| Slice 5.5 fixed-fixture regression | pass | state `3b4a85366dab80db` / `3b4a85366dab80db`, ledger `bc3077100ba291b4` / `bc3077100ba291b4` |
| IIIB independent signature regression | pass | replay A `bf8818a26ed7b1dc`, replay B `bf8818a26ed7b1dc`, expected `bf8818a26ed7b1dc` |
| Sub-threshold candidate group rejected | pass | max d 72.000 km, accepted groups 0 |
| Forced collision reaches 300 km threshold | pass | step 2, max d 480.000 km, accepted groups 1 |
| Pure-oceanic negative emits no collision groups | pass | decisions 1, collision candidates 0, groups 0 |

## Fixture Results

| Fixture | Replay | Step | Matrix hits | Collision candidates | Terrane records | Groups | Accepted | Subthreshold | Max d km | Read-only stable | Hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| subthreshold_all_continental | 0 | 1 | 52 | 1 | 52 | 1 | 0 | 1 | 72.000 | yes | `d58c9fea824a63e8` |
| subthreshold_all_continental | 1 | 1 | 52 | 1 | 52 | 1 | 0 | 1 | 72.000 | yes | `d58c9fea824a63e8` |
| forced_collision_threshold | 0 | 2 | 346 | 1 | 176 | 1 | 1 | 0 | 480.000 | yes | `f8ceb65a27894b80` |
| forced_collision_threshold | 1 | 2 | 346 | 1 | 176 | 1 | 1 | 0 | 480.000 | yes | `f8ceb65a27894b80` |
| pure_oceanic_negative | 0 | 1 | 170 | 0 | 0 | 0 | 0 | 0 | 0.000 | yes | `3902224e451a8fb7` |
| pure_oceanic_negative | 1 | 1 | 170 | 0 | 0 | 0 | 0 | 0 | 0.000 | yes | `3902224e451a8fb7` |

## Representative Groups

- Sub-threshold replay A: pair `1`, records 52, valid distances 9412, max d 72.000 km, accepted no, hash `1439104ac3fdef47`
- Threshold replay A: pair `1`, records 176, valid distances 31858, max d 480.000 km, accepted yes, hash `be790d58e44a6ce3`

## Interpretation

The grouping key is the canonical opposing plate pair from IIIB collision-candidate evidence. The interpenetration distance is the maximum IIIB distance-to-front value over candidate terrane records in that pair, which matches the slice contract's `max(d(p))` gate. The low-speed fixture demonstrates that collision candidates are not accepted merely because contacts and terranes exist; the default-speed fixture demonstrates that the same group crosses the 300 km threshold within the bounded search window.

All grouping calls are audited before and after against projection, state, crust, and convergence hashes. Stable before/after hashes show this slice observes IIID.1 evidence without making topology, motion, or authority changes.

## Verdict

PASS. IIID.2 collision grouping and interpenetration gating are deterministic and read-only. IIID.3 may implement slab-break detachment using the accepted group output; it must not reinterpret this slice as already mutating topology.
