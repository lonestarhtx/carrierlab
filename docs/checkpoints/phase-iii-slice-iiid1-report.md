# Phase III Slice IIID.1 Report - Connected Continental Terrane Detection

Generated: `2026-05-03 05:48:19` UTC

## Scope

IIID.1 adds read-only connected terrane detection from existing IIIB collision-candidate evidence. It does not detach terranes, mutate topology, apply sutures, change plate motion, resample, or alter carrier authority.

Metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIID1/20260503T054747Z/metrics.jsonl`

## Gate Summary

| Gate | Result | Evidence |
|---|---:|---|
| Slice 5.5 fixed-fixture regression | pass | state `3b4a85366dab80db` / `3b4a85366dab80db`, ledger `bc3077100ba291b4` / `bc3077100ba291b4` |
| IIIB independent signature regression | pass | replay A `bf8818a26ed7b1dc`, replay B `bf8818a26ed7b1dc`, expected `bf8818a26ed7b1dc` |
| Forced collision terrane determinism | pass | hash A `7a8cfc63c5d67d70`, hash B `7a8cfc63c5d67d70`, records 170 / 170 |
| Pure-oceanic negative | pass | matrix hits 170, decisions 1, terrane records 0 |

## Fixture Results

| Fixture | Replay | Step | Matrix hits | Collision candidates | Terrane records | Total triangles | Inner sea triangles | Invalid seeds | Non-collision hits | Hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| forced_collision_all_continental | 0 | 1 | 170 | 1 | 170 | 1699672 | 0 | 0 | 0 | `7a8cfc63c5d67d70` |
| forced_collision_all_continental | 1 | 1 | 170 | 1 | 170 | 1699672 | 0 | 0 | 0 | `7a8cfc63c5d67d70` |
| pure_oceanic_negative | 0 | 1 | 170 | 0 | 0 | 0 | 0 | 0 | 170 | `7bb3738c9f272cfc` |
| pure_oceanic_negative | 1 | 1 | 170 | 0 | 0 | 0 | 0 | 0 | 170 | `7bb3738c9f272cfc` |

## Representative Terrane

- Forced collision replay A: plate 0 seed 191 triangles 10004 vertices 5141 mean_xC 1.000000 hash `a1bc78a32ea75072`
- Forced collision replay B: plate 0 seed 191 triangles 10004 vertices 5141 mean_xC 1.000000 hash `a1bc78a32ea75072`

## Interpretation

The all-continental forced-collision fixture exercises the IIIB `CollisionCandidate` path and traverses the source plate's plate-local triangle connectivity from each candidate triangle. Since both fixture plates are entirely continental, every emitted terrane is expected to contain only continental triangles and no inner-sea additions. The pure-oceanic fixture proves that accepted convergence evidence alone does not fabricate terranes when polarity is not continental collision.

Inner-sea inclusion is implemented in the traversal: enclosed oceanic triangle regions that do not touch a plate boundary and whose continental neighbors all belong to the detected component are included in the terrane. This fixture has no inner seas, so the count is expected to remain zero.

## Verdict

PASS. IIID.1 connected terrane detection is deterministic and read-only for the exercised fixtures. IIID.2 may group collision candidates without treating this slice as topology mutation.
