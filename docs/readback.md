# CarrierLab Readback

Status: submitted for user approval. Stage 0 checkpoint work must not resume
until this readback receives explicit approval.

## Mission

CarrierLab is a clean-room paper-faithful reproduction of the Cortial
paper/thesis carrier layer with Aurous differential diagnosis. The paper and
thesis are treated as the gold standard. The lab's job is to implement the
carrier correctly in isolation and use the diff against Aurous's failed earlier
prototype to identify what Aurous got wrong.

This is not Aurous 2 and not a product path. CarrierLab stays scoped to the
carrier: rigid motion, projection, resampling, diagnostics, and checkpoint
reports. Subduction, collision, rifting, uplift, erosion, slab pull, terrain
beauty, control panels, and game integration remain out of scope.

## Verdict Structure

The lab has three reproduction-shaped outcomes:

1. Faithful reproduction succeeded. The carrier matches the paper/thesis
   behavior and Table 2 performance envelope; the Aurous diff becomes the bug
   list.
2. Faithful reproduction succeeded mechanically but performance/quality
   diverged. The implementation follows the thesis but measured behavior does
   not match the published envelope.
3. Faithful reproduction is blocked by underspecification. A specific
   paper/thesis gap prevents a faithful clean-room implementation.

"The paper does not work" is not a working verdict. If results point that way,
the next action is a thesis-spec audit for the implementation detail that was
missed.

## Stop Conditions

A stop condition pauses stage advancement and triggers an investigation
checkpoint. It is not automatically a final verdict.

Each investigation checkpoint must record:

- thesis section checked
- what the spec says
- what the implementation does
- the divergence, if any
- proposed fix, if a divergence exists

Only after a thesis-spec audit finds no implementation divergence and the
failure persists does the result become verdict-level evidence.

## Discipline Rules

- Global sample ownership is projection/output state, never tectonic authority.
- No ownership persistence, retention, recovery, repair, backfill, hysteresis,
  or anchoring.
- No Aurous V6, V9, Prototype A-E, exporter, control panel, sidecar
  infrastructure, ADR template, or skill-suite inheritance.
- Metrics come before pictures.
- Test oracles must be independent; projection output cannot validate itself.
- Same seed must produce identical hashes at every stage gate.
- Every stage ends with a written checkpoint and explicit user go/no-go.
- No silent stage transitions.

## Performance Framing

Performance is split into simulation kernel time and diagnostic time.

`step_kernel_ms` is the paper-comparable cost. CarrierLab's subset of processes
should be faster than paper Section 7.4 Table 2 because the lab omits
subduction, collision, rifting, and oceanic generation. `step_with_diagnostics`
includes PNG export, hashing, and metric computation and is not directly
comparable to Table 2.

Approved ceilings:

| Resolution | Kernel for 1000 steps | With diagnostics for 1000 steps | Memory |
| --- | ---: | ---: | ---: |
| 60k | <= 2 minutes | <= 5 minutes | <= 2 GB |
| 100k | <= 5 minutes | <= 15 minutes | <= 4 GB |
| 250k | <= 20 minutes | <= 45 minutes | <= 8 GB |
| 500k | <= 45 minutes | <= 90 minutes | <= 16 GB |

## Deliverable Revisions

The four pre-coding deliverables were revised to fold in the accepted briefing
edits and thesis findings:

- `docs/paper-carrier-extraction.md` now frames the lab as reproduction plus
  differential diagnosis, adds short paper/thesis anchors for plate-local
  authority, duplicated topology, ray queries, cadence, and resampling, and
  shrinks unresolved ambiguity to the no-subduction multi-hit tie-break.
- `docs/driftworld-carrier-comparison.md` now treats Driftworld as independent
  confirmation of the thesis carrier family, records the Crust/Data split,
  resampling agreement, barycentric-resampling failure mode, and the quaternion
  transform divergence.
- `docs/carrier-design.md` now commits the defaults: 40 plates, 0.3 land
  coverage, seed 42, `dt = 2 My`, Fibonacci samples, clean-room spherical
  Bowyer-Watson Delaunay, thesis cadence, per-step vertex motion,
  ray-from-origin queries, plate-local duplicated topology, and nearest-centroid
  no-subduction tie-break.
- `docs/pre-mortem.md` now adds the ray-vs-containment category error,
  provisional Stage 0 scaffold risk, cumulative floating-point drift from
  per-step vertex rotation, BVH precision degeneracy, third-plate intrusion,
  and adversarial negative controls.

## Stage 0 Code Audit

Thesis sections checked:

- thesis sec. 3.2.4: plate triangulations are copied from the global TDS into
  dedicated per-plate triangulations by duplicating, re-indexing, and
  compacting topology, with empty neighborhood at plate borders.
- thesis sec. 3.3.2.3 step 2: each global TDS vertex is projected by a ray
  through the planet center and tested against existing plate triangles.
- thesis sec. 3.3.1.3: ray-triangle intersection is the interaction/query
  family and BVHs accelerate it.

Provisional implementation before audit:

- `ProjectColdStart` seeded each sample with `Sample.PlateId`.
- It then added plate ids from `SampleIncidentTriangleIds`.
- It did not query per-plate triangle meshes.
- That made Stage 0 coverage partly agree with initialization rather than
  prove the carrier projection.

Corrected implementation in this commit:

- Added `FCarrierVertex` and `FCarrierPlateTriangle`.
- `FCarrierPlate` now owns compact local vertex/triangle arrays plus a
  global-sample-to-local-vertex map.
- `BuildPlateLocalTriangulations` duplicates global TDS triangles into the
  owning plate's local mesh.
- `ProjectColdStart` now loops over plate-local triangles and calls
  `IntersectRayWithPlateTriangle` using the center-to-sample ray.
- Raw candidate plates are counted before resolution.
- Boundary degeneracy is detected from barycentric edge/vertex weights.
- Non-degenerate overlaps remain separate from boundary-degenerate overlaps.
- Third-plate intrusion is counted explicitly.

Known limitation:

- Stage 0 ray projection is currently brute-force. This is acceptable for the
  audit and small automation tests, but it is not the final 60k-500k
  performance path. Stage 0 checkpoint work must address acceleration and
  scaling before any performance claim.

## Verification

Build command:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat' CarrierLabEditor Win64 Development 'C:\Users\Michael\Documents\Unreal Projects\CarrierLab\CarrierLab.uproject'
```

Result: succeeded.

Automation command:

```powershell
& 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Users\Michael\Documents\Unreal Projects\CarrierLab\CarrierLab.uproject' -unattended -nop4 -nosplash -NullRHI -NoSound -stdout -FullStdOutLogOutput -ExecCmds="Automation RunTests CarrierLab.Stage0; Quit" -abslog="C:\Users\Michael\Documents\Unreal Projects\CarrierLab\Saved\Logs\CarrierLabStage0ExecAudit.log"
```

Result: 2 tests found, 2 tests passed, exit code 0.

Small-run metrics from the audit log:

| Test | Samples | Plates | Global tris | Local tris | Ray tests | Miss | Multi | Boundary-degenerate | Third-plate intrusion | CAF | Hash |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| ColdStartDeterminism | 1024 | 40 | 2044 | 2044 | 2,093,056 | 0 | 548 | 548 | 76 | 0.302734 | c87b02ac2a69f305 |
| ColdStartSmoke | 512 | 12 | 1020 | 1020 | 522,240 | 0 | 189 | 189 | 21 | 0.322266 | f598ef34bdcddce8 |

Interpretation:

- The corrected projection path is executing ray-triangle tests.
- The local plate-triangle copy covers the global TDS in these small runs.
- No raw misses and no non-degenerate overlaps were reported.
- All raw multi-hit samples in these tests were classified as
  boundary-degenerate.
- Third-plate intrusion appears as boundary degeneracy in these vertex-sample
  small runs; Stage 0 checkpoint reporting must classify this explicitly at
  target resolutions.

## Repository Hygiene

- Git repository is initialized.
- Remote is set to `git@github.com:LonestarHTX/CarrierLab.git`.
- `AGENTS.md` contains the push-verification rule: after a push, verify the
  remote SHA with `git ls-remote origin <branch>`.
- `.gitignore` excludes `Saved/`, `Intermediate/`, `Binaries/`, and
  `DerivedDataCache/`.
- Initial commit `dc2e4f39d482b57227570f13de2bc37b33d48220` contains the four
  pre-coding deliverables.

## Approval Boundary

After this commit is pushed and verified, work pauses here. The next action is
user review of this readback. Stage 0 checkpoint execution resumes only after
explicit user approval.
