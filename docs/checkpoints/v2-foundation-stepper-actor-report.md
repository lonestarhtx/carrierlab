# V2 Foundation Stepper Actor Report

Date: 2026-06-08

Branch: `codex/v2-0-carrier`

Base commit: `98d8e1b84d2cf4334b67b265ea4b0ff1a01ffe07`

Structure note: as of 2026-06-09, go-forward planning uses
`docs/carrierlab-milestone-roadmap.md`. This report is Milestone 0 diagnostic
evidence. The V2 label is preserved for traceability only.

## Scope

This checkpoint adds an in-editor diagnostic actor, `ACarrierFoundationStepperActor`, for visual inspection of the completed V2 carrier foundation pipeline. It is a viewer over the V2 core results, not a new carrier authority path.

The actor exposes the following visual steps:

| Step | Default visual layer |
|---|---|
| Cold Start | plate assignment |
| Rigid Motion | post-motion contact/sample hit counts |
| Contact Candidates | post-motion contact/sample hit counts |
| Process Marking | subducting/colliding process marks |
| Filtered Global Sampling | valid, zero-valid, and filtered hits |
| Q1/Q2 Gap Fill | generated oceanic q1/q2 samples |
| Topology Rebuild | rebuilt global-triangle assignment |
| Process Reset | pre-reset marks shown as cleared when post-reset marks are zero |

It also exposes explicit layers for forbidden fallback counters and replay determinism.

## User Validation Path

1. Add `CarrierFoundationStepperActor` to an editor level.
2. Use the Details panel buttons:
   - `Rebuild Foundation Snapshot`
   - `Step Forward`
   - `Step Backward`
   - `Reset To Cold Start`
   - `Use FX014`
   - `Use FX015`
   - `Use Inspectable Scale`
3. Default fixture is `Inspectable Scale`, because a fresh actor should present a sphere-like carrier surface instead of the tiny micro-audit fixture.
4. Use `FX-015` for the q1/q2 gap-fill microscope view.
5. Use `FX-014` as the rebuild/reset control fixture with no generated oceanic gap.
6. `Inspectable Scale` defaults to `2000`; set `InspectableScaleSampleCount=50000` for paper-scale visual inspection when editor responsiveness is acceptable.

## Deferred Visual Inspection Checklist

Visual inspection is deferred by user choice because the current actor is a
diagnostic evidence viewer, not a tectonic-behavior validation surface. Use this
checklist later if the actor needs to be inspected for diagnostic usability.

1. Open `CarrierLab.uproject` in Unreal Editor.
2. Add a fresh `CarrierFoundationStepperActor` to an empty level, or select the existing actor and click `Use Inspectable Scale`.
3. Confirm the mesh is a dense sphere-like carrier surface, not the 6-sample octahedron.
4. Confirm the status text starts with `INSPECT-2000-Q1Q2-REBUILD` unless `InspectableScaleSampleCount` was changed.
5. Confirm status text reports `pass=true`, `replay=true`, and `forbidden=false`.
6. Confirm the Details panel metrics show non-micro scale counts: `SampleCount=2000` and a correspondingly dense `TriangleCount`.
7. Click `Step Forward` through each stage and check that the default layer changes are visible:
   - `Cold Start`: plate assignment colors.
   - `Contact Candidates`: contact evidence colors.
   - `Process Marking`: subducting/colliding mark colors.
   - `Filtered Global Sampling`: valid, zero-valid, and filtered-hit evidence.
   - `Q1/Q2 Gap Fill`: generated oceanic q1/q2 evidence.
   - `Topology Rebuild`: rebuilt global-triangle assignment.
   - `Process Reset`: post-reset marks cleared.
8. Click `Use FX015` and confirm the mesh intentionally becomes the tiny octahedron microscope fixture with `samples=6` and `triangles=8`.
9. Click `Use Inspectable Scale` again and confirm it returns to the dense sphere-like view.

Stop and treat the validation as failed if the fresh actor defaults to the octahedron, if `forbidden=true`, if replay is false, if the dense fixture reports micro counts, or if a layer is blank/uniform while the status text claims matching evidence exists.

## Implementation

- Added `FCarrierV2FoundationStepper::BuildSnapshot` to `CarrierLabV2Core`.
- Added sample and triangle visual snapshot records that expose:
  - cold-start plate assignment,
  - assigned plate after global sampling/gap fill,
  - raw/filtered/valid hit counts,
  - q1/q2 generated oceanic flags,
  - process mark flags,
  - rebuilt triangle plate assignment,
  - replay determinism and forbidden fallback summary.
- Added `ACarrierFoundationStepperActor` as a new placeable actor using `UDynamicMeshComponent` vertex colors.
- Added `CarrierLab.V2.FoundationStepper.Snapshot` automation coverage for the snapshot data feed.

## Evidence

Build:

```text
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\Users\Michael\.codex\skills\carrierlab-build\scripts\Run-CarrierLabEditorBuild.ps1
Result: Succeeded
```

Automation:

```text
UnrealEditor-Cmd.exe CarrierLab.uproject -NullRHI -NoSound -Unattended -nop4 -nosplash -ExecCmds="Automation RunTests CarrierLab.V2.FoundationStepper.Snapshot; Quit" -TestExit="Automation Test Queue Empty"
Result={Success}
```

Key automation evidence:

| Fixture | Snapshot | Pass | Replay | Samples | Triangles | Zero valid | Generated oceanic | Rebuilt | Post-reset marks | Forbidden fallback |
|---|---|---|---|---:|---:|---:|---:|---:|---:|---|
| `FX-014` | built | true | true | 6/6 | 8/8 | 0 | 0 | 8/8 | 0 | false |
| `FX-015` | built | true | true | 6/6 | 8/8 | 1 | 1 | 8/8 | 0 | false |

## Guardrails

This checkpoint does not add terrain, elevation beauty, rifting, erosion, slab pull, collision terrain effects, or subduction terrain effects. The actor does not use persistent global ownership as tectonic authority; it renders a diagnostic snapshot from the existing V2 carrier pipeline.

## Known Limitations

- This actor is a user inspection surface. The numeric gates remain the authority.
- The default actor fixture is sphere-like for visual inspection. Micro fixtures remain opt-in through `Use FX014` and `Use FX015`.
- The rigid-motion step is visualized through post-motion hit/contact evidence on the fixed global TDS, not by animating plate-local carrier geometry.

## Verdict

The stepper actor is available for later diagnostic inspection. It is not a new
milestone, it is not simulation authority, and it does not authorize advancement
by itself. Milestone 0 closeout remains governed by numeric gates, claim audit,
checkpoint evidence, and explicit user go/no-go.
