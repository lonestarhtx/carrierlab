# Phase III Slice IIID.3 Report - Opposing Continental Mass Test

Generated: `2026-05-03 17:04:13` UTC

## Scope

IIID.3 consumes IIID.2 accepted collision groups and computes the reachable continental mass on the opposing destination plate. It is read-only: no slab break, topology detach, suture, uplift, plate motion change, resampling, or carrier-authority mutation occurs in this slice.

Metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIID3/20260503T170058Z/metrics.jsonl`

## Gate Summary

| Gate | Result | Evidence |
|---|---:|---|
| Slice 5.5 fixed-fixture regression | pass | state `3b4a85366dab80db` / `3b4a85366dab80db`, ledger `bc3077100ba291b4` / `bc3077100ba291b4` |
| IIIB independent signature regression | pass | replay A `bf8818a26ed7b1dc`, replay B `bf8818a26ed7b1dc`, expected `bf8818a26ed7b1dc` |
| Destination mass accepts all-continental collision | pass | records 176, accepted 176, min accepted ratio 0.998800 |
| Small destination continent rejected | pass | records 90, rejected 89, insufficient 2, min rejected ratio 0.000000 |
| Pure-oceanic negative emits no mass records | pass | decisions 1, collision candidates 0, mass records 0 |
| No lab multi-hit policy influence | pass | policy-resolved multi-hit counts 0 / 0 / 0 / 0 / 0 / 0 |

## Fixture Results

| Fixture | Replay | Step | Matrix hits | Collision candidates | Source terranes | Groups | Group accepted | Mass records | Mass accepted | Mass rejected | Max ratio | Min rejected ratio | Policy multi-hits | Read-only stable | Hash |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|
| all_continental_mass_accept | 0 | 2 | 346 | 1 | 176 | 1 | 1 | 176 | 176 | 0 | 1.001201 | 0.000000 | 0 | yes | `5903d212a4a659ff` |
| all_continental_mass_accept | 1 | 2 | 346 | 1 | 176 | 1 | 1 | 176 | 176 | 0 | 1.001201 | 0.000000 | 0 | yes | `5903d212a4a659ff` |
| small_destination_reverse_reject | 0 | 2 | 346 | 1 | 90 | 1 | 1 | 90 | 1 | 89 | 2501.000000 | 0.000000 | 0 | yes | `6ed4df42165e5b39` |
| small_destination_reverse_reject | 1 | 2 | 346 | 1 | 90 | 1 | 1 | 90 | 1 | 89 | 2501.000000 | 0.000000 | 0 | yes | `6ed4df42165e5b39` |
| pure_oceanic_negative | 0 | 1 | 170 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0.000000 | 0.000000 | 0 | yes | `3f971cad2cbbe4b4` |
| pure_oceanic_negative | 1 | 1 | 170 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0.000000 | 0.000000 | 0 | yes | `3f971cad2cbbe4b4` |

## Representative Mass Records

- All-continental replay A: pair `1`, source 0 -> destination 1, source area 12.571397163, destination area 12.556317518, ratio 0.998800, accepted yes, hash `de8dcd1dcb7743c2`
- Small-destination replay A: pair `1`, source 0 -> destination 1, source area 12.571397163, destination area 0.005026548, ratio 0.000400, accepted no, hash `8bd4e3efed18b149`

## Interpretation

The destination mass gate is directed: each IIID.2 accepted group is expanded into source-terrane -> destination-plate checks using the original matrix evidence's opposing triangle id as the destination seed. The all-continental fixture demonstrates that adequate opposing continental mass passes the 50% source-terrane threshold. The small-destination fixture sculpts a test-only destination patch after collision evidence is present, so it exercises the downstream fall-through/rejection path without pretending to be a natural plate-generation claim.

All mass detection calls are audited before and after against projection, state, crust, and convergence hashes. Stable before/after hashes show this slice observes collision evidence without making topology, motion, or authority changes.

## Verdict

PASS. IIID.3 opposing continental mass testing is deterministic and read-only for the exercised fixtures. IIID.4 may plan slab-break detachment as a dry run; it must not reinterpret this slice as topology mutation.
