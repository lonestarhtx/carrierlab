# Phase III Sub-phase IIIB Consolidation Checkpoint

Artifacts root: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIBConsolidation/20260503T070339Z`

This hardening checkpoint keeps IIIB.1-IIIB.7 read-only and addresses the GPT-5.5 Pro pause-pending-investigation review. It adds no subduction mutation, no triangle consumption, no material transfer, and no new global ownership authority. The gates below demonstrate deterministic local-vs-pair discriminator evidence, a true no-admissible-convergence negative, and an independent component signature for future Phase III regression checks.

## Gate Summary

| Gate | Result | Evidence |
|---|---|---|
| Slice 5.5 baseline replay regression | pass | replay hashes stable: pass; state `3b4a85366dab80db`, material ledger `bc3077100ba291b4` |
| local_vs_pair_discriminator | pass | pair signed -0.075341390677, accepted local positives 432, rejected local non-positives 293, propagated 22 |
| no_admissible_convergence_negative | pass | pair signed -0.000000000000, matrix pairs 0, decisions 0, seed hits 0, added 0 |
| IIIB independent signature replay | pass | discriminator `bf8818a26ed7b1dc` vs `bf8818a26ed7b1dc`; negative `531e8de4f37715c4` vs `531e8de4f37715c4` |
| IIIB expected-token gate | pass | discriminator computed `bf8818a26ed7b1dc` / `bf8818a26ed7b1dc`, expected `bf8818a26ed7b1dc`; negative signatures are diagnostic only |

## Slice 5.5 Baseline Regression

This is a fixed-fixture regression against Phase II Slice 5.5, not a global no-mutation proof. It protects the known 60k/40/seed-42 filtered-resampling baseline while the IIIB tracking data is active.

| Replay | State hash | Material ledger hash | Contact hash | Label hash | Filter hash | Expected state | Expected ledger | Seconds |
|---:|---|---|---|---|---|---|---|---:|
| 0 | `3b4a85366dab80db` | `bc3077100ba291b4` | `b21ee68fa450142a` | `5279c470b8ff7c1c` | `42a8f46665086a2f` | `3b4a85366dab80db` | `bc3077100ba291b4` | 3.330 |
| 1 | `3b4a85366dab80db` | `bc3077100ba291b4` | `b21ee68fa450142a` | `5279c470b8ff7c1c` | `42a8f46665086a2f` | `3b4a85366dab80db` | `bc3077100ba291b4` | 3.297 |

## Local-Vs-Pair Discriminator Fixture

Fixture: two plates, forced-divergence motion, searched up to 80 steps. The gate requires pair-level signed convergence `<= 0` while the same pair still has both accepted local positive evidence and rejected local non-positive evidence. This demonstrates deterministic local-vs-pair discriminator evidence; it does not overclaim a global proof of all possible contact geometries.

| Replay | Step | Pair sign | Probe local sign | Accepted local + | Rejected local <=0 | Matrix pairs | Decisions | Seed evidence | Seed triangle | Seed velocity | Seed hits | Added | Closure hash | Legacy smoke token | Independent signature |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|
| 0 | 51 | -0.075341390677 | 0.001023993020 | 432 | 293 | 1 | 1 | 21 | 956 | 0.010932563893 | 413 | 22 | `5445f4e8caa2c020` | `df36a5bc9e8f175e` | `bf8818a26ed7b1dc` |
| 1 | 51 | -0.075341390677 | 0.001023993020 | 432 | 293 | 1 | 1 | 21 | 956 | 0.010932563893 | 413 | 22 | `5445f4e8caa2c020` | `df36a5bc9e8f175e` | `bf8818a26ed7b1dc` |

### Representative Matrix Evidence

| Bucket | Evidence id | Plate | Other plate | Local triangle | Other triangle | Contact id | Signed local velocity | Disposition |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| accepted local positive | 0 | 0 | 1 | 16584 | 11955 | 0 | 0.001023993020 | accepted |
| accepted local positive | 1 | 0 | 1 | 16696 | 11607 | 1 | 0.002913662377 | accepted |
| accepted local positive | 2 | 0 | 1 | 16697 | 11586 | 2 | 0.004042634612 | accepted |
| accepted local positive | 3 | 0 | 1 | 16888 | 11643 | 3 | 0.005896253960 | accepted |
| accepted local positive | 4 | 0 | 1 | 18044 | 26590 | 4 | 0.005529146451 | accepted |
| accepted local positive | 5 | 0 | 1 | 18050 | 26532 | 5 | 0.006894299807 | accepted |
| accepted local positive | 6 | 0 | 1 | 18056 | 26615 | 6 | 0.008045217915 | accepted |
| accepted local positive | 7 | 0 | 1 | 18061 | 26589 | 7 | 0.009224028300 | accepted |
| rejected local non-positive | 19 | 1 | 0 | 263 | 18593 | 19 | -0.005328091449 | rejected |
| rejected local non-positive | 20 | 1 | 0 | 780 | 18360 | 20 | -0.003206985581 | rejected |
| rejected local non-positive | 26 | 1 | 0 | 8950 | 16686 | 26 | -0.001652689792 | rejected |
| rejected local non-positive | 27 | 1 | 0 | 9046 | 16686 | 27 | -0.001966722813 | rejected |
| rejected local non-positive | 30 | 1 | 0 | 11310 | 23649 | 30 | -0.004744095483 | rejected |
| rejected local non-positive | 32 | 1 | 0 | 11518 | 23669 | 32 | -0.002846648367 | rejected |
| rejected local non-positive | 33 | 1 | 0 | 11571 | 16694 | 33 | -0.002985124814 | rejected |
| rejected local non-positive | 34 | 1 | 0 | 11572 | 23765 | 34 | -0.006704782455 | rejected |

## No-Admissible-Convergence Negative

Fixture: two plates, zero-motion plus one synthetic rejected local-evidence record. The gate requires real rejected local non-positive evidence while matrix admission, polarity decisions, and propagation remain empty; this is a no-admission downstream diagnostic, not a natural ray-query geometry claim.

| Replay | Step | Pair sign | Accepted local + | Rejected local <=0 | Matrix pairs | Decisions | Seed hits | Added | Matrix evidence hash | Independent signature |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|
| 0 | 1 | -0.000000000000 | 0 | 1 | 0 | 0 | 0 | 0 | `05913fcbdf31ffbf` | `531e8de4f37715c4` |
| 1 | 1 | -0.000000000000 | 0 | 1 | 0 | 0 | 0 | 0 | `05913fcbdf31ffbf` | `531e8de4f37715c4` |

## IIIB Independent Signature Components

| Component | Discriminator replay 0 | Discriminator replay 1 | Negative replay 0 | Negative replay 1 |
|---|---|---|---|---|
| Slice 5.5 baseline | `5d14e0433e3a1e96` | `5d14e0433e3a1e96` | `5d14e0433e3a1e96` | `5d14e0433e3a1e96` |
| IIIB.1 active list | `b224faa17a2f3671` | `b224faa17a2f3671` | `70734b200cf5066f` | `70734b200cf5066f` |
| IIIB.2 distance-to-front | `74fd0a310cbf3387` | `74fd0a310cbf3387` | `62bc202e8b68f8c5` | `62bc202e8b68f8c5` |
| IIIB.3 matrix evidence | `30d9b017a369c45b` | `30d9b017a369c45b` | `32cafdbf46e1d63d` | `32cafdbf46e1d63d` |
| IIIB.4 polarity | `454fde119a98913c` | `454fde119a98913c` | `60ecbc76f203575a` | `60ecbc76f203575a` |
| IIIB.6 propagation | `958d94c1ee8cff33` | `958d94c1ee8cff33` | `58b6f08b764af422` | `58b6f08b764af422` |
| IIIB.7 closure | `ee128f05184d6b3c` | `ee128f05184d6b3c` | `55d720eb628817aa` | `55d720eb628817aa` |
| Independent IIIB signature | `bf8818a26ed7b1dc` | `bf8818a26ed7b1dc` | `531e8de4f37715c4` | `531e8de4f37715c4` |

## Historical Smoke Token

The previous consolidated IIIB smoke token `df36a5bc9e8f175e` is retained only as historical comparison. The expected-token gate now compares the local-vs-pair independent signature directly to `bf8818a26ed7b1dc`; negative-fixture signatures remain diagnostic because they intentionally describe a different no-admission fixture.


## Notes

- Matrix admission is based on local signed convergence at the active triangle barycenter. Pair keys remain canonical metadata, not the convergence oracle.
- Propagation seed provenance is now reported by evidence id, plate ids, triangle id, and signed local velocity.
- Phase IIIC remains paused until this hardening checkpoint has explicit user review.

## Recommendation

IIIB hardening gates pass. Pause for user review before Phase IIIC planning or implementation.
