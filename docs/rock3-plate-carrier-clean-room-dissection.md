# Rock 3 Plate Carrier Clean-Room Dissection

Date: 2026-06-08

Purpose: reconstruct the likely high-level plate-carrier design pressures in Rock 3 from public statements, local metadata, UI labels, and visible behavior, then translate those observations into independent CarrierLab design questions.

This is not an implementation extraction. Do not use this document as a source for copied code, assets, shaders, parameter values, or tuned constants.

## Evidence Classes

Permission audit:

- No explicit open-source license, decompilation permission, or code-use license was found in the local install.
- No local EULA, license, readme, legal, or notice file was found in the installed Rock 3 folder.
- SteamDB configuration for app `1892520` shows a normal Windows launch option for `Rock3.exe`; no custom EULA entry was visible in the inspected configuration.
- The game is installed and accessed through Steam. Steam's Subscriber Agreement restricts reverse engineering, source-code derivation, disassembly, and decompilation of Content and Services/software accessed via Steam unless otherwise permitted by the agreement, subscription terms/rules, applicable law, or prior written consent.
- The extracted UI bundle contains an official Discord invite URL: `https://discord.gg/3Apnn4C623`.
- The developer is openly discussing high-level algorithm details in public Reddit comments, which is a strong signal that public algorithm study is welcomed.
- This is still not explicit permission to decompile IL2CPP/native binaries, copy source, copy shaders, or reuse constants/assets.

Confirmed from public statements:

- Rock 3 presents itself as a real-time GPU procedural planet generator using tectonic, erosion, and climate simulations.
- Exported maps include equirectangular biomes, elevation/height, heat/temperature, water, and landmass-related outputs.
- Tectonics and erosion are run by specifying geological and eroded age with Megaannum sliders.
- Same seed is not guaranteed to replay the same tectonic geography because small random changes can cascade.
- Public update notes mention mantle-convection-driven plate motion, subduction, divergent and collision boundaries, plate splitting, active plate-count fluctuation, orogeny, crust-density evolution, and crust implosion/continental-creation bug fixes.
- The 2025 super-continent update states that inertia was removed and plate motion is now directly driven by mantle convection dynamics, with stronger pull from colder subduction boundaries and moderate influence from divergent/collision boundaries.
- The same update states that active plate count can fluctuate between the slider value and half that value.
- Plate splitting is described as probabilistic/factor-dependent, with random split axis and a bias toward predominantly oceanic plates.
- Orogeny over subducting plates was moved inland from the plate edge.
- Crust density evolution was reworked around thermal cooling, compressive loading, and hydration.
- A public Reddit explanation by `dawneater` gives a direct high-level algorithm sketch: icosahedral grid, nearest-neighbor/cell-polygon buffers, crust as particles per cell, position-based dynamics for local particle collisions, aggregation of local forces up to plate-level motion, center-of-mass/gravity-mediated supercontinent breakup, advection/merging, divergent-boundary spawning in empty cells, consumed-plate detection, largest-plate splitting to recycle empty plate ids, and smoothed isostasy for rendering.
- A second public Reddit thread states the system is "all particles"; particles can switch plates when surrounded and drift toward the plate center of mass to reduce stretching.

Confirmed locally:

- The installed build is Unity, IL2CPP, Burst-capable, and GPU-heavy.
- Local UI labels expose tectonic age, plate count, hotspots, rigidity, orogeny, initial land, quality, gravity, erosion age, hydraulic/thermal/tidal erosion, precipitation, insolation, greenhouse, obliquity, spin, and sidereal-year controls.
- The running app exposes surface probes for biome, elevation, latitude, longitude, density, temperature, and precipitation.
- Visual overlays include satellite, elevation, temperature, precipitation, biomes, ocean, magma/crust-age style visualization, sediment, and projection mode.

Inferred:

- Rock 3's plate carrier is a particle/cell field carrier, not a Cortial-style duplicated plate-local triangulation carrier.
- Its carrier uses a target plate-count plus dynamic active plate lifecycle rather than a fixed immutable plate set.
- Its plate object/state likely includes at least a region label, center-of-mass estimate, force/velocity state, boundary classifications, rigidity/viscosity/deformation behavior, and attached crust-particle fields.
- Its crust state likely separates density from volume/thickness or continental/oceanic classification, because public fixes reference oceanic densities with continental volumes and continental crust subducting incorrectly.
- Its process model lets local particle collisions, plate-level aggregate forces, crust fields, stochastic split/recycle events, and divergent spawning continually reshape the carrier.

Sources:

- Steam store page: `https://store.steampowered.com/app/1892520/Rock_3/`
- Steam Subscriber Agreement: `https://store.steampowered.com/subscriber_agreement/`
- SteamDB app/config page: `https://steamdb.info/app/1892520/config/`
- SteamDB patch notes for build `20989222`: `https://steamdb.info/patchnotes/20989222/`
- Reddit high-level algorithm thread: `https://www.reddit.com/r/proceduralgeneration/comments/1hatu7u/simulated_tectonics_of_a_molten_planet_another/`
- Reddit preview/discussion thread: `https://www.reddit.com/r/proceduralgeneration/comments/1h4t7pt`

## Creator-Disclosed Algorithm Reconstruction

This section is derived from public creator comments, not from decompiled code.

Likely carrier substrate:

- Icosahedral/spherical grid rather than a rectangular texture-first carrier.
- Per-cell nearest-neighbor and polygon buffers for local topology.
- Crust represented as particles associated with cells.
- At least two crust particles per cell to represent overlap/collision cases.
- More-than-two collisions merged into existing particles through a performance-minded policy.

Likely local physics:

- Position-based dynamics solver.
- Distance constraints for particles in the same and neighboring cells.
- Local collision step determines interaction type from plate identity, buoyancy, and relative velocity.
- The solver produces soft-body/viscosity behavior rather than perfectly rigid plates.

Likely plate coupling:

- Local collision forces aggregate upward to plate-level state.
- Plate-level aggregation gives partial rigid-body behavior.
- Subduction at an edge can pull the whole plate, approximating slab pull / convection effects.
- Particles can change plate membership if surrounded.
- Particles are biased toward the plate center of mass to reduce stretching.

Likely supercontinent dynamics:

- Global center of mass is estimated.
- A gravity-mediated force pushes mass away from an over-concentrated global center.
- That force helps split supercontinents.

Likely lifecycle/update order:

1. Solve local particle collisions/constraints.
2. Classify interactions from plate, buoyancy, and relative velocity.
3. Aggregate local forces to plate-level motion.
4. Apply center-of-mass / supercontinent breakup forces.
5. Advect particles into new positions.
6. Merge particle overlaps as needed.
7. Find empty cells and spawn new particles at divergent boundaries.
8. Detect consumed plates.
9. Split the largest plate and assign half to an empty plate id when a plate is consumed.
10. Render smoothed isostasy/elevation from neighboring particle state.

CarrierLab-safe takeaway:

- Rock 3's carrier is not primarily a fixed rigid-plate mesh carrier. It is much closer to a particle/cell field carrier with plate-level aggregate forces layered on top.
- The plate id is a mutable label on moving crust particles, not necessarily an immutable owner of a fixed plate-local triangulation.
- This explains why it can support deformable plates, stochastic splitting, active plate recycling, and long-running emergent supercontinents.
- It also explains why deterministic replay and strict material accounting are not the same goals as CarrierLab.

## Permission Conclusion

The creator has publicly given enough algorithm-level explanation that we can study and cite the approach at a high level.

That does not grant permission to decompile the shipped IL2CPP build or copy internals. If we want to inspect binaries or source-level implementation, the right next step is to ask directly in Discord/Reddit/Steam:

> I am building an independent clean-room procedural tectonics research project. Would you be comfortable granting explicit permission for educational inspection of Rock 3 internals, such as decompilation/source-map/source review, with no copied code/assets/constants and with attribution? If not, may we cite and analyze your public algorithm descriptions only?

Until permission is explicit, the deepest defensible route is public-algorithm reconstruction plus black-box experiments.

That route is now much stronger than pure black-box study because the public comments reveal the core carrier architecture.

## Likely Plate Carrier Shape

Rock 3 appears to manage plates as dynamic regions in a simulation field.

The key design idea is not just "move rigid plates." It is a coupled carrier/process loop:

1. Store per-cell or per-surface-sample crust fields.
2. Partition those cells into active plates.
3. Track per-plate aggregate properties such as center of mass, velocity, rigidity/deformability, and maybe dominant crust type.
4. Derive plate motion from a mantle/convection or boundary-force field rather than only fixed Euler poles.
5. Classify plate interfaces as divergent, collision, or subduction boundaries.
6. Update crust at boundaries through creation, destruction/subduction, density evolution, deformation, and orogeny.
7. Allow active plate count to fluctuate through plate splitting and probably plate disappearance/merging.
8. Run long enough for emergent continent/supercontinent patterns.
9. Render/export diagnostic maps from the evolving fields.

That is a different authority model from CarrierLab. Rock 3 seems to prioritize emergent, visually plausible world evolution. CarrierLab prioritizes paper-faithful carrier invariants, deterministic checkpoints, and independent recomputation.

## Carrier Parameters Worth Studying

These are the important parameter families Rock 3 exposes or implies.

Initial substrate:

- Surface resolution/quality.
- Initial land amount.
- Initial or target plate count.
- Seed/random galaxy/world initial conditions.

Plate lifecycle:

- Target plate count.
- Active plate-count floor or fluctuation band.
- Plate splitting criteria.
- Split probability.
- Split axis selection.
- Preference for splitting oceanic plates.
- Plate disappearance, absorption, or merge rules.

Plate motion:

- Mantle/convection forcing.
- Boundary pull from cold subduction zones.
- Divergent-boundary forcing.
- Collision-boundary forcing.
- Center-of-mass calculation.
- Velocity cap/range.
- Velocity fluctuation.
- Inertia on/off or damping.
- Rigidity/viscosity/deformation.

Boundary classification:

- Divergent boundary detection.
- Collision boundary detection.
- Subduction boundary detection.
- Relative density/volume/age checks for subduction eligibility.
- Continental-vs-oceanic crust classification.
- Boundary drift/meander damping.
- Boundary process width or falloff.

Crust/material state:

- Crust density.
- Crust volume or thickness.
- Crust age/temperature/cooling.
- Hydration.
- Compressive loading.
- Continental crust creation rate.
- Oceanic crust creation at divergent boundaries.
- Crust destruction/subduction.
- Material consistency checks that prevent "oceanic density with continental volume" contradictions.

Orogeny and volcanism:

- Orogeny frequency/intensity.
- Boundary-distance offset for mountain formation behind subduction edges.
- Collision mountain formation.
- Hotspot count.
- Hotspot scale.
- Hotspot activity fluctuation.
- Island-forming volcanism.

Coupled later processes:

- Gravity effect on erosion and mountain height.
- Erosion age.
- Hydraulic erosion.
- Thermal erosion/talus.
- Tidal erosion/coastline and shelf shaping.
- Climate controls that can run beside or after tectonics.

Observability:

- Surface probe values.
- Magma/crust-age visualization.
- Sediment visualization.
- Elevation/temperature/precipitation/biome maps.
- Equirectangular export.
- Same-seed rerun comparison.

## What Rock 3 Seems To Have Learned The Hard Way

Public bug-fix notes reveal several carrier traps that matter to us:

- Plate center-of-mass errors can bias global plate distribution. Rock 3 reportedly fixed a center-of-mass issue that pushed plates toward even equatorial distribution.
- Velocity bugs can collapse motion diversity. A bug caused most plates to move near maximum velocity most of the time.
- Orogeny at boundaries can explode mountain heights if boundary intensity and falloff are wrong.
- Continental crust creation is easy to overproduce.
- Crust type cannot be a single ambiguous flag. Density, volume/thickness, age, and continental/oceanic identity must stay internally consistent.
- Divergent boundaries can drift/meander too aggressively if the boundary process is not stabilized.
- A crust-implosion failure mode can erase crust except near boundaries. That suggests carrier/process update ordering and conservation checks are critical.
- Plate rigidity is expensive and hard: public notes say plates remain deformable at simulation timescales, and better rigidity would require more sophistication to remain performant.

These are excellent independent design warnings for CarrierLab. They do not tell us how Rock 3 implemented anything, but they tell us which invariants are worth guarding.

## CarrierLab Translation

CarrierLab should not copy Rock 3's likely carrier model.

CarrierLab's authority remains:

- plate-local duplicated triangulations
- plate-local material/crust fields
- center-to-point ray queries against plate-local geometry
- barycentric material transfer
- explicit miss/overlap classification
- deterministic replay and independent oracles

Rock 3's strongest contribution to our thinking is a list of process-state variables we need to make explicit once CarrierLab moves beyond the carrier foundation:

- active plate identity vs target plate count
- velocity diversity and velocity caps
- center-of-mass/aggregate recomputation oracle
- boundary type and boundary provenance
- crust density/volume/age/temperature consistency
- subduction eligibility
- collision vs subduction orogeny
- divergent gap creation and new crust provenance
- plate split/merge lifecycle if we ever choose to model it
- visual exports for each hidden state

## Independent Original Carrier Design Questions

For our own plate carrier, ask these before implementation:

1. Is plate motion fixed analytic Euler motion, dynamically forced motion, or a two-mode system?
2. Are plates rigid carriers, deformable regions, or rigid carriers with local process-state overlays?
3. Is plate count fixed, target-bounded, or emergent?
4. Where does crust authority live: plate-local mesh vertices, plate-local samples, global grid samples, or explicit material particles?
5. Which data fields define crust type: density, thickness/volume, age, temperature, hydration, composition, or a categorical material tag?
6. What state makes subduction legal?
7. What state makes collision legal?
8. What invariant prevents continental crust from being silently destroyed by a projection or remesh step?
9. What invariant prevents new continental crust from appearing inside oceanic crust without a named process?
10. What boundary-width/falloff model prevents orogeny spikes?
11. What cadence separates carrier motion, boundary classification, remesh/projection, crust mutation, erosion, and climate?
12. Which outputs demonstrate that the carrier works numerically before any beautiful planet render is trusted?

## Experiment Plan To Reverse-Engineer Behavior Safely

Use exports and metrics, not code.

Baseline:

- Choose one seed and fixed quality.
- Export all maps at geological age 0.
- Record dimensions, hashes, and file names.
- Record visible UI parameter values manually.

Tectonic age sweep:

- Ages: 0, 25, 100, 250, 500, 1000 Ma, and more if practical.
- Export satellite, elevation, temperature, precipitation, biomes, magma/crust-age, and any ocean/land masks.
- Compute land fraction, connected-component count, largest-continent fraction, coastline length proxy, mountain-area fraction, and elevation histogram.

Repeatability:

- Same seed and same parameters, rerun tectonics three times.
- Measure map deltas and component changes.
- Classify deterministic, weakly stochastic, or strongly stochastic behavior.

Plate-count sweep:

- Low, medium, high target plate count.
- Compare continent component count, mountain line density, coastline complexity, and time to supercontinent.
- Look for the active plate count floor implied by public notes.

Rigidity sweep:

- Low, medium, high rigidity.
- Compare boundary width, continent smearing, mountain sharpness, and coastline deformation.
- Watch for differences between rigid translation-like plate behavior and field-like deformation.

Orogeny sweep:

- Low, medium, high orogeny.
- Measure max elevation, high-elevation area, mountain belt distance from inferred boundaries/coasts, and whether mountains sit directly on boundaries or inland.

Hotspot sweep:

- Low, medium, high hotspots.
- Measure isolated island count, oceanic volcanic chains, and hotspot persistence.

Initial land sweep:

- Low, medium, high initial land.
- Measure whether continental crust tends to grow, shrink, equilibrate, or collapse.

Crust consistency probes:

- Compare apparent continental/oceanic regions against elevation, density readout, and crust-age/magma visualization.
- Look for impossible states: continental elevation with oceanic density, oceanic elevation with continental behavior, or disappearing crust.

Boundary-process inference:

- Use time-lapse exports to infer boundaries by new crust lines, mountain belts, trenches/low belts, and magma/age changes.
- Do not need a plate-id map to infer process zones; compare consecutive exports.

## Minimum Observability We Should Add To CarrierLab

To learn the right lessons while staying original, CarrierLab should expose:

- Plate id / process-state overlay.
- Candidate count overlay.
- Resolved authority overlay.
- Divergent-gap overlay.
- Convergent-overlap overlay.
- Subduction/collision eligibility overlay.
- Crust density, thickness/volume, age, and material class maps.
- Boundary type map.
- Boundary-distance map.
- Orogeny contribution map.
- New-crust provenance map.
- Per-plate center of mass, area, velocity, and mass tables.
- Same-seed replay hash table.

The Rock 3 lesson is that users understand planets through maps. The CarrierLab addition is that each map needs a metric gate, not just a visual.

## Clean-Room Verdict

Rock 3's carrier appears to be an emergent GPU process carrier: dynamic regions, mantle/boundary-forced motion, mutable crust fields, stochastic splitting, and heavy visual observability.

CarrierLab's carrier should remain a deterministic paper-faithful material carrier: plate-local duplicated triangulations, explicit projection/remesh semantics, independent oracles, and checkpoint reports.

The practical bridge is not implementation. The bridge is observability and parameter taxonomy.
