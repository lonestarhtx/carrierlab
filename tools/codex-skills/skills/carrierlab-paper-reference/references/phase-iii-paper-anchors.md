# Phase III Paper And Thesis Anchors

Use these as search anchors, not as a replacement for reading the source.

## Paper

- Paper section 3: plate-local carrier model and process overview.
- Paper section 6: geometric model and meshing.
- Paper section 7.4: performance table and runtime comparison.
- Figures 14-17: full process morphology, out of scope for carrier-only validation.

## Thesis

- Thesis section 3.2.2: plate model.
- Thesis section 3.2.3: numerical aspect, Fibonacci sampling, spherical Delaunay.
- Thesis section 3.2.4: initial state and plate-local triangulation construction.
- Thesis section 3.3.1.3: convergence implementation, per-step geodetic rotation, subduction/trench/uplift/slab-pull details.
- Thesis section 3.3.2.1: oceanic crust generation.
- Thesis section 3.3.2.3: divergence implementation and resampling pseudocode.
- Thesis section 3.3.3: erosion/dampening/sediment-accretion elevation evolution.
- Thesis Table 3.2: constants such as dt, radius, zt, zc, rs, v0, u0.
- Thesis Figure 37: subduction uplift transfer functions.

## Known Formula Anchors

- Uplift: `u_j(p) = u0 * f(d(p)) * g(v(p)) * h(ztilde_i(p))`.
- Distance transfer from Figure 37: `f(d) = exp(3d/rs) * exp(-9d^2/rs^2)`.
- Speed transfer: `g(v) = v / v0`.
- Relief transfer: `h(ztilde) = ztilde^2`, with `ztilde = (z_i(p) - zt) / (zc - zt)`.
- Resampling cadence: `DeltaT = (1-alpha)M + alpha m`, `alpha = min(1, vm/v0)`.
