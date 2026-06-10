# Rock 3 Clean-Room Study Notes

Date: 2026-06-08

Purpose: learn from Rock 3's observable procedural planet behavior for CarrierLab research without copying proprietary implementation, assets, shaders, UI source, or tuned constants.

## Clean-Room Boundary

Allowed:

- Inspect installed-file metadata, public manifests, Unity logs, visible UI labels, and runtime behavior.
- Use screenshots and exported maps as behavioral evidence.
- Design independent experiments and metrics from observed outputs.
- Compare high-level concepts against the Cortial/PTP model and CarrierLab's existing clean-room requirements.

Not allowed for CarrierLab:

- Decompile, disassemble, patch, or bypass protections around `GameAssembly.dll`, `global-metadata.dat`, or Steam integration.
- Copy Rock 3 source, assets, shaders, UI code, parameter values, or generated texture assets into CarrierLab.
- Tune CarrierLab to reproduce Rock 3 output.
- Treat Rock 3 as paper authority. It is a useful comparison target, not a source of truth.

## Local Evidence

Install path inspected:

`C:\Program Files (x86)\Steam\steamapps\common\Rock 3`

Key local facts:

- Steam app id: `1892520`.
- Local Steam build id: `20989222`.
- Install size from Steam manifest: `114980453` bytes.
- Last played from Steam manifest: 2026-06-08 02:05:07 local time.
- Publisher/developer string in `Rock3_Data\app.info`: `Insight Games`, `Rock3`.
- Unity runtime from `Player.log`: `6000.2.10f1`.
- Build shape: Unity IL2CPP-style player with `Rock3.exe`, `UnityPlayer.dll`, `GameAssembly.dll`, and `Rock3_Data\il2cpp_data\Metadata\global-metadata.dat`.
- Runtime graphics/physics evidence: NVIDIA RTX 3080, D3D device setup, PhysX 4.1.2, multithreaded physics.
- `Rock3_Data\boot.config` enables graphics jobs/threaded rendering and disables HDR display.
- `Rock3_Data\ScriptingAssemblies.json` lists `Assembly-CSharp.dll`, `Unity.Burst`, `Unity.Mathematics`, `Unity.InputSystem`, `Unity.AI.Navigation`, `OneJS.Runtime`, `com.tencent.puerts`, `Steamworks.NET`, and UI/support libraries.
- LocalLow runtime data exists under:
  `C:\Users\Michael\AppData\LocalLow\Insight Games\Rock3`
- LocalLow contains `Player.log` plus an extracted OneJS/esbuild UI bundle under `App\@outputs\esbuild`.
- The first safe window-only evidence screenshot is:
  `C:\Users\Michael\Documents\Unreal Projects\CarrierLab\Saved\Rock3_window_001.png`

## Public Source Evidence

Steam store page:

- Rock 3 is presented as a procedural Earth-like planet generator/simulation.
- The public description emphasizes tectonic plates, erosion, climate, biomes, oceans, rivers, lakes, and terrain.
- The store page states that every pixel is simulated in real time on the GPU and that equirectangular maps can be exported.

Steam update notes:

- The 2025 erosion update discusses hydraulic, thermal, tidal, and glacial erosion, plus rock hardness and erosion-age behavior.
- The 2025 tectonics update discusses mantle-convection-inspired plate motion, subduction, divergent boundaries, continent collision, orogeny, crust-density evolution, and nondeterministic simulation behavior.

SteamDB:

- Confirms the app is a Unity title with IL2CPP/Burst-style depots and Unity player components.

Sources:

- https://store.steampowered.com/app/1892520/Rock_3/
- https://steamcommunity.com/games/1892520/announcements/detail/559986827892073361
- https://steamcommunity.com/games/1892520/announcements/detail/517675310613887759
- https://steamdb.info/app/1892520/

## Visible Runtime Behavior

The window-only screenshot shows:

- A 3D planet with ocean, atmosphere, polar/snow cover, visible landforms, shelf/coastline color bands, and large-scale continent shapes.
- A surface probe panel reporting:
  - biome class
  - elevation
  - latitude
  - longitude
  - biome code
  - density
  - temperature
  - precipitation
- The observed biome at the capture point was tropical monsoon.
- The UI appears to blend three use modes:
  - planet inspection
  - parameterized generation/simulation
  - map/export visualization

## Exposed Model Knobs

These are UI-level concepts, not copied implementation details.

Seed and reset:

- Seed/random controls exist.
- UI text indicates seed changes reset tectonics and erosion.

Quality and initial state:

- Quality/resolution.
- Initial land amount.
- Plate count.

Tectonics:

- Geological age.
- Plates.
- Hotspots.
- Plate rigidity.
- Orogeny or mountain-building activity.
- Visible tectonic-time overlays include magma and crust-age style information.

Erosion:

- Erosion age.
- Hydraulic erosion.
- Thermal erosion/talus steepness.
- Tidal erosion/coastline and shelf shaping.
- Gravity affects erosion strength and mountain height.
- Visible erosion-time overlays include sediment.

Climate/orbit:

- Precipitation/rainfall.
- Temperature.
- Insolation/solar constant.
- Greenhouse.
- Obliquity/axial tilt.
- Spin/rotation direction.
- Sidereal year.
- Milankovitch/seasonal framing appears in the UI terminology.

Views/overlays:

- Satellite.
- Elevation.
- Temperature.
- Precipitation.
- Biomes using Koppen-Geiger style classification.
- Ocean toggle.
- Grid lines.
- Equirectangular projection.
- Atmosphere/stars/galactic dust.
- Day/night and orbital animation.
- Export maps.

## Inferences

These are hypotheses from public text, local metadata, and UI behavior:

- Rock 3 is likely organized around GPU-generated 2D planet maps projected onto a sphere, with equirectangular export as a first-class output.
- The user-facing model separates tectonic evolution from erosion evolution, with separate time controls.
- The product prioritizes immediate exploratory feedback over deterministic scientific replay. Public update notes explicitly mention nondeterminism in tectonic simulations.
- Visual layers are not just presentation; they are the main observability channel.
- Parameter names are geophysics-inspired but game-facing. They are useful as a UX reference and experiment checklist, not as proof of paper-faithful process.
- The exported maps are probably the safest and most useful comparison surface for CarrierLab.

## CarrierLab-Relevant Lessons

Strong ideas to learn from:

- Keep map exports central. A procedural planet tool becomes much easier to reason about when users can export elevation, climate, biome, crust/age, magma/sediment, and satellite layers.
- Hover probes matter. Per-location readouts for elevation, material/state, density, temperature, precipitation, and biome are high-value diagnostics.
- Separate simulation phases in the UI. Tectonics age and erosion age being visibly distinct helps users reason about causality.
- Give users direct parameter handles, but make reset/invalidating behavior explicit.
- Maintain a planet view and a rectangular projection view. The projection view is better for metrics and debugging; the globe is better for intuition.

CarrierLab cautions:

- CarrierLab cannot adopt Rock 3's nondeterministic exploration style for checkpoints. Commandlets and gates need deterministic replay, independent recomputation, and explicit failure predicates.
- CarrierLab's authority model remains plate-local carriers and paper-grounded state, not global sample ownership or output-map persistence.
- CarrierLab should not treat visually pleasing output as correctness evidence. The Rock 3 comparison should inspire observability, not lower our verification standard.

## Proposed Black-Box Experiment Matrix

Baseline capture:

- Record build id, install date, engine version, GPU, and screenshot hash.
- Pick one seed and one quality level.
- Export all available maps before changing parameters.
- Record dimensions, file names, hashes, and channel formats.

Repeatability:

- Same seed, same parameters, run tectonics three times.
- Compare exported map hashes and visual deltas.
- Goal: measure nondeterminism rather than assuming seed determinism.

Age sweeps:

- Tectonics: sample geological ages at 0, 25, 100, 250, and 500 Ma if the UI permits.
- Erosion: sample erosion ages at 0, 10, 25, 50, 60, and 100 Ma.
- For each export, measure land fraction, elevation histogram, mean/sigma elevation by latitude band, coast length proxy, ice fraction, and biome area histogram.

Single-parameter sweeps:

- Initial land: low/mid/high.
- Plate count: low/mid/high.
- Hotspots: low/mid/high.
- Rigidity: low/mid/high.
- Orogeny: low/mid/high.
- Gravity: low/mid/high.
- Hydraulic, thermal, and tidal erosion: one-at-a-time low/mid/high.
- Rainfall, solar constant, greenhouse, obliquity, and spin: one-at-a-time sweeps.

Layer semantics:

- Capture/export satellite, elevation, temperature, precipitation, biomes, ocean, magma/crust-age, and sediment views.
- For each layer, infer whether color encodes continuous values, categorical classes, masks, or display-only shading.
- Do not reuse palettes/assets; only document semantics.

Cross-model comparison:

- Compare Rock 3 outputs to CarrierLab only through independent metrics:
  - land fraction
  - boundary/coastline proxies
  - mountain-height distribution
  - age/thermal layer distribution if exported
  - climate-band coherence
  - biome-area distribution
- Do not compare or copy algorithms.

## Immediate Next Steps

1. Interact with Rock 3 manually or through approved window-only automation to export all maps for one seed.
2. Place exports under an ignored evidence folder, for example `Saved/Rock3Study/<timestamp>/`.
3. Write a small CarrierLab-side analysis script only after exports exist.
4. Produce a follow-up report that contains hashes, map dimensions, metric tables, and a side-by-side list of observability features worth independently implementing.
5. Keep any CarrierLab implementation work behind the normal stage/slice checkpoint discipline and user go/no-go.
