# Phase IIIE.3 Filtered Remesh Candidate Selection

Verdict: PASS / IIIE.4 UNBLOCKED. This slice implements the Phase IIIE remesh source selector and audit gates against the paper contract only. It does not mutate global samples, rebuild topology, reset process state, generate new oceanic fields, optimize replay, or promote Stage 1.5 lab policy.

## Scope

- The current-state audit casts from the planet center through global TDS sample positions against plate-local BVHs.
- True subducting marks, Pre-IIIE.8 obduction-pending marks, and explicit collision-pending candidates are invisible before source selection.
- Exactly one remaining hit is the only barycentric transfer path. Zero remaining hits route to the IIIE.2 q1/q2/qGamma divergent gap path. Multiple remaining hits are reported as unresolved same-material, mixed-material, or third-plate anomalies.
- Prior-owner fallback, centroid/random/synthetic winners, recovery, repair, retention, hysteresis, and anchoring remain forbidden in this primary IIIE path.

## Gates

| Gate | Result | Evidence |
|---|---:|---|
| subducting invisible over-plate transfer | pass | class `resolved single hit`, raw `2`, visible `1`, filters sub/obd/coll `1/0/0`, resolved plate `1`, policy/prior `0/0`, hash `fed4e6557a4d1a0a`. |
| obduction pending invisible | pass | class `resolved single hit`, raw `2`, visible `1`, filters sub/obd/coll `0/1/0`, resolved plate `1`, policy/prior `0/0`, hash `6defb1c4584b170f`. |
| collision pending invisible | pass | class `resolved single hit`, raw `2`, visible `1`, filters sub/obd/coll `0/0/1`, resolved plate `1`, policy/prior `0/0`, hash `6ecc529e5a3df6f8`. |
| filter-exhausted divergent gap route | pass | class `divergent gap after filtering`, raw `2`, visible `0`, filters sub/obd/coll `1/1/0`, resolved plate `-1`, policy/prior `0/0`, hash `e9cbf32187d9a59e`. |
| no raw hit divergent gap route | pass | class `no-hit divergent gap`, raw `0`, visible `0`, filters sub/obd/coll `0/0/0`, resolved plate `-1`, policy/prior `0/0`, hash `e3140cf4eda96f84`. |
| same-material multi-hit fails loud | pass | class `unresolved same-material multi-hit`, raw `2`, visible `2`, filters sub/obd/coll `0/0/0`, resolved plate `-1`, policy/prior `0/0`, hash `c5d91e860ace443f`. |
| mixed-material multi-hit fails loud | pass | class `unresolved mixed-material multi-hit`, raw `2`, visible `2`, filters sub/obd/coll `0/0/0`, resolved plate `-1`, policy/prior `0/0`, hash `a055a50c9a3f2910`. |
| third-plate multi-hit fails loud | pass | class `unresolved third-plate multi-hit`, raw `3`, visible `3`, filters sub/obd/coll `0/0/0`, resolved plate `-1`, policy/prior `0/0`, hash `ad9c95058968de25`. |
| Same-seed filtered-selection replay | pass | Replay hashes `fed4e6557a4d1a0a` and `fed4e6557a4d1a0a`. |
| Current-state plate-local ray smoke | pass | samples `64`, raw hits `64`, gaps `0`, unresolved `51`, policy/prior `0/0`, hashes `5feb44ddb3e99f7b` / `5feb44ddb3e99f7b`. |
| Inherited IIIB independent signature regression | pass | `CarrierLabPhaseIIID7` regression produced computed signatures `bf8818a26ed7b1dc` / `bf8818a26ed7b1dc`, expected `bf8818a26ed7b1dc`. |

## Inherited Regression Note

IIIE.3 consumes convergence-side process state, so the checkpoint closeout also cites the inherited IIIB independent-signature regression. The focused IIIE.3 commandlet owns remesh-selection gates; the existing IIID7 commandlet owns the larger independent-signature harness. The regression artifact was written to `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE3/phase-iii-slice-iiie3-iiid7-regression.md`.

## Contract Table

| Paper requirement | CarrierLab support now | IIIE obligation still ahead | Gate needed |
|---|---|---|---|
| Center ray from planet center through each global TDS vertex | IIIE.3 current-state audit uses plate-local BVH ray candidates | Wire into the actual remesh event in IIIE.5 | End-to-end remesh event fixture |
| Ignore subducting and colliding/obducting-in-process triangles before source selection | Selector filters true subducting, obduction-pending, and collision-pending reasons separately | Map any future persistent collision-pending carrier state into the same reason before remesh | Per-reason invisibility counters remain nonzero in fixtures |
| Single valid hit interpolates crust fields barycentrically | Single visible candidate copies the interpolated/probed fields and source plate id | Mutate global samples only when remesh event is implemented | Independent scalar/vector field residuals |
| Zero valid hits become divergent gap fill | No-hit and filter-exhausted samples route to IIIE.2 q1/q2/qGamma | IIIE.4 creates oceanic fields from that provenance | Gap-fill q1/q2 field oracle |
| Multiple valid hits after filtering are not silently resolved | Same-material, mixed-material, and third-plate unresolved classes fail loud | Decide only with paper citation or explicit lab policy | Unresolved counts block primary remesh |

## Stop Conditions For IIIE.4+

- Stop if any primary remesh path uses previous sample owner, previous continental fraction, centroid/random/synthetic winner policy, or recovery/backfill/retention/hysteresis/anchoring.
- Stop if obduction-pending marks are collapsed into true subduction marks or left visible during remesh source selection.
- Stop if unresolved multi-hit samples are converted into winners without a paper-cited rule or explicit approved lab policy.
- Stop if zero-hit/filter-exhausted samples bypass the IIIE.2 continuous q1/q2/qGamma route.
- Stop if actual remesh mutation is added before topology rebuild and process-state reset gates are specified.

## Next Slice Boundary

IIIE.4 should implement divergent oceanic field generation from the IIIE.2 provenance route for samples that IIIE.3 classifies as no-hit or filter-exhausted gaps. It should not rebuild topology or reset process state yet unless the slice boundary is explicitly revised.

Metrics: `C:/Users/Michael/Documents/Unreal Projects/CarrierLab/Saved/CarrierLab/PhaseIII/IIIE3/phase-iii-slice-iiie3-metrics.jsonl`.
