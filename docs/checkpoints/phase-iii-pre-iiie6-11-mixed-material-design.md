# Pre-IIIE.6.11 Design — Mixed-Material Multi-Hit Resolution

## Status

**DESIGN READY.** Recommend **Option A** (amendment to existing nearest-hit lab policy)
with explicit empirical justification from IIIE.6.10. This slice closes the
default-scale live remesh blocker.

The IIIE consolidation lab-policy count stays at SEVEN. This is an amendment
to the existing nearest-hit lab policy (#6), not a new policy.

## Required nuance

Do not claim authorial intent unless the page image says it directly. Frame as:
"the convergence section explicitly uses material-aware routing; the
divergence/remesh section is silent on material-aware routing." Do NOT write
"the authors deliberately rejected material-aware divergence routing" unless
quoted from the source.

## Sources verified

- IIIE.6.10 diagnostic report
  (`docs/checkpoints/phase-iii-slice-iiie6-10-manual-cadence-unsupported-multihit-diagnosis.md`)
- IIIE.6.11 mixed-material research
  (`docs/checkpoints/phase-iii-pre-iiie6-11-mixed-material-research.md`,
  uncommitted)
- IIIE.6.5 design + implementation (`commit 9868073`) — precedent for
  amendment pattern
- IIIE.6.6 distance-tie fallback (`commit 73f0cf7`) — composition target
- IIIE.6.9 design (`docs/checkpoints/phase-iii-pre-iiie6-9-restore-paper-literal-zero-hit-design.md`)
  — precedent for "remove a check, restore paper-literal," contrasted with
  this slice which is amendment, NOT removal
- Thesis §3.3.2.3 (page 68, image `cc5c6807-079.png`)
- Thesis §3.3.1.3 (page 62, image `cc5c6807-073.png`)
- Thesis §3.3.2.1 (page 66, image `cc5c6807-077.png`)

## Background

IIIE.6.10 confirmed at editor default (100k/40 seed-42, ContinentalPlateFraction
= 0.30) that the live remesh blocker is dominated by `MixedMaterial → UnsupportedHeld`.

The IIIE.6.10 step sweep is dominated by strict unique-nearest `MixedMaterial`
records at the 1e-9 km tolerance, with a tiny 0-2 record distance-tie tail at
some swept steps. This makes the nearest-hit amendment the primary behavior
and the IIIE.6.6 fallback composition an actually exercised edge case, not
purely theoretical cleanup.

Per-step empirical counts (from IIIE.6.10 selection rows):

| Step | Mixed-material unsupported | Mixed-material strict unique-nearest | Mixed-material distance ties |
|---|---:|---:|---:|
| 1 | 464 | 464 | 0 |
| 8 | 3,004 | 3,004 | 0 |
| 15 | 4,202 | 4,200 | 2 |
| 16 | 4,301 | 4,301 | 0 |
| 17 | 4,219 | 4,218 | 1 |
| 20 | 4,489 | 4,488 | 1 |
| 24 | 5,142 | 5,141 | 1 |
| 32 | 5,480 | 5,480 | 0 |
| 33 | 5,698 | 5,696 | 2 |
| 40 | 6,292 | 6,292 | 0 |
| 48 | 5,853 | 5,852 | 1 |
| 60 | 8,861 | 8,860 | 1 |

**Strict unique-nearest is the overwhelmingly dominant mixed-material case at
default scale, and the remaining distance-tie tail is small enough to route
through the existing IIIE.6.6 hierarchy rather than inventing a new material
rule.**

The IIIE.6.5 explicit decline of mixed-material:

```cpp
F.Name = TEXT("mixed material - IIIE.6.5 declines, held");
F.ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::MixedMaterial;
F.ExpectedNearestResult = ECarrierLabPhaseIIIE65NearestHitResult::UnsupportedHeld;
```

is itself CarrierLab discipline without paper backing. The paper does NOT
specify "do not resolve mixed-material multi-hits." This decline blocks
paper-spirit nearest-hit resolution for what the empirical data shows are
unique-nearest cases.

The asymmetry surfaced by the research:
- Thesis §3.3.1.3 (convergence): material-aware routing (continental check
  switches subduction → collision)
- Thesis §3.3.2.3 (divergence/remesh): only process-state filter for
  ray-cast, no `x_C` read in source-selection text
- Thesis §3.3.2.1 (ridge formula): silent on neighboring plate material

The convergence section explicitly uses material-aware routing; the
divergence/remesh section is silent on material-aware routing.

## Option comparison

### Option A — Amend nearest-hit to include strict unique-nearest mixed-material

Extend the existing IIIE.6.5 nearest-hit resolver to apply when the multi-hit
bucket is `MixedMaterial` AND the candidates have a strict unique nearest
within 1e-9 km tolerance. Same shape as the existing nearest-hit rule, just
without the material decline.

The IIIE.6.5 fixture `"mixed material - IIIE.6.5 declines, held"` becomes:

```cpp
// Default behavior (with bExtendNearestHitToMixedMaterial = true):
F.Name = TEXT("mixed material - IIIE.6.5 unique nearest applies");
F.ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::MixedMaterial;
F.ExpectedNearestResult = ECarrierLabPhaseIIIE65NearestHitResult::UniqueNearestHit;

// Restored-veto behavior (with bExtendNearestHitToMixedMaterial = false):
// preserves the existing UnsupportedHeld decline as a baseline.
```

**Empirical case (load-bearing):** mixed-material strict-unique-nearest is
the dominant class at default scale across the IIIE.6.10 step sweep. Option A
covers the dominant observed mixed-material holds; Sub-option 1 below covers
the tiny observed distance-tie tail through the already approved IIIE.6.6
fallback hierarchy.

**Disclosure framing:** amendment to existing nearest-hit lab policy
(#6 of seven). Adding mixed-material to the bucket types nearest-hit
applies to. NOT a new lab policy. NOT paper-literal restoration (the paper
is silent on multi-valid hits in general; nearest-hit itself is CarrierLab
lab policy, just being extended).

**Risk:** continental plates may lose to closer oceanic plates at boundary
samples, eroding continental coverage over many cadences. IIIH.1 long-horizon
Auth CAF will surface this if it's material; the follow-up path would be
either Option B as a follow-up slice OR continental-priority at a fallback
layer (see Distance-Tied design decision below).

### Option B — Continental priority for mixed-material multi-hits (rejected)

Eighth IIIE lab policy. Driftworld-corroborated. When candidates differ in
material, continental-fraction-higher plate wins regardless of geometric
proximity.

**Why rejected:** the convergence section uses material-aware routing
explicitly; the divergence/remesh section is silent on material-aware
routing. Importing convergence-side logic to the divergence side would
diverge from the structural pattern of the paper. Continental persistence
is a real concern for IIIH.1 stability, but front-loading B before
empirical evidence (long-horizon Auth CAF drift) would be choosing
Driftworld-ish over paper-spirit-ish without justification.

If IIIH.1 surfaces continental erosion attributable to Option A's
material-blind nearest-hit at boundaries, B becomes the follow-up slice.
Until then, A is the more defensible amendment.

## Recommended rule (Option A)

Primary rule: when `MultiHitBucket == MixedMaterial` AND the candidates
have a strict unique nearest within 1e-9 km, apply the existing
nearest-hit resolution. The `UnsupportedHeld` decline only fires when:

- Mixed-material with distance-tied candidates (no strict unique nearest)
- OR a different bucket (`SameMaterial` etc.) that nearest-hit doesn't
  apply to

Composition with IIIE.6.6: see Distance-Tied design decision below.

## Distance-tied mixed-material — design decision

The IIIE.6.10 sweep shows mixed-material distance ties are rare but present
at default scale (0-2 records at the swept steps). The composition question is
therefore load-bearing, even though the fallback count is tiny relative to the
strict unique-nearest population.

**Two sub-options:**

**Sub-option 1: Amend IIIE.6.6 to accept mixed-material distance ties.** When
mixed-material has no unique nearest, fall through to IIIE.6.6's existing
hierarchy (continental priority → older oceanic age → lower plate ID).
At Layer 1 (continental priority), this would fire when one candidate is
continental-dominant and another isn't — exactly the Option B logic
applied at the fallback layer.

**Sub-option 2: Keep mixed-material distance ties held separately.**
Mixed-material distance ties remain `UnsupportedHeld`, distinct from
IIIE.6.6 (which only handles cross-plate-different distance ties from
IIIE.6.5).

**Recommended sub-option: 1 (amend IIIE.6.6).** Reasoning:

- Continental priority at Layer 1 is acceptable as a *fallback* even
  though we rejected it as a *primary* rule. The justification is
  asymmetric: paper-spirit says don't material-route by default; common
  sense says when geometry can't decide AND material composition differs
  meaningfully, the continental-dominant plate is the more defensible
  pick.
- Empirical scale: mixed-material distance ties are a tiny tail at default
  scale (0-2 records in the swept rows). The disclosure cost is minimal, but
  the gate must verify these records resolve rather than continue to hold.
- Composition consistency: IIIE.6.6 already handles distance-tied cases
  for cross-plate-different. Extending it to mixed-material distance ties
  keeps the chain shape uniform.
- The fallback layer is where geometry has already failed to disambiguate.
  Material priority as a tiebreaker after geometric tiebreaker exhausts
  is a defensible escalation.

Sub-option 1 is therefore an amendment to IIIE.6.6 alongside the IIIE.6.5
amendment. This keeps the named-policy count stable at seven.

## Implementation packet for Codex

### Files to touch

| File | Change |
|---|---|
| `Source/CarrierLab/Private/CarrierLabVisualizationActor.cpp` | In `SelectPhaseIIIE3FilteredRemeshSource` (around the IIIE.6.5 evaluation), remove the `MixedMaterial → UnsupportedHeld` decline. Mixed-material with strict unique-nearest at 1e-9 km tolerance now resolves via the existing nearest-hit code path. |
| `Source/CarrierLab/Public/CarrierLabVisualizationActor.h` | Add opt-out flag `bExtendNearestHitToMixedMaterial` defaulting `true`. Add `bUsedNearestHitOnMixedMaterial` per-record positive flag (sub-classifier of existing `bUsedNearestHitTieBreak`). Add `NearestHitOnMixedMaterialCount` aggregate. |
| `Source/CarrierLab/Private/CarrierLabPhaseIIIE65NearestHitCommandlet.cpp` | Update fixture `"mixed material - IIIE.6.5 declines, held"` to verify the new behavior under default flags. Rename to `"mixed material - IIIE.6.5 unique nearest applies"`. Add restored-veto fixture verifying old behavior under `bExtendNearestHitToMixedMaterial = false`. |
| `Source/CarrierLab/Private/CarrierLabPhaseIIIE66DistanceTieFallbackCommandlet.cpp` | Add fixtures for mixed-material distance-tied cases under sub-option 1. Verify Layer 1 (continental priority) fires correctly. Add `bMixedMaterialDistanceTie` audit field. |
| `Source/CarrierLab/Private/CarrierLabPhaseIIIE610ManualCadenceUnsupportedCommandlet.cpp` | Update gate: `unsupported_held` count drops from observed values (4,301-8,861 across step sweep) to 0 under default behavior. Mixed-material counter now reports as `nearest_hit_on_mixed_material_resolved`. The previous unsupported counts remain reproducible via opt-out flag. |
| `Source/CarrierLab/Private/CarrierLabPhaseIIIE6Commandlet.cpp` | Update default 100k/40 seed-42 land=0.30 gate wording so selection closure and bounded live-apply probes are not overclaimed as a full every-step apply sweep unless `-FullApplySweep` is run. |
| `docs/checkpoints/phase-iii-slice-iiie6-11-mixed-material-resolution.md` | New slice report. |

### Hook point

Existing IIIE.6.5 evaluation code in `SelectPhaseIIIE3FilteredRemeshSource`.
The current early-return that classifies `MixedMaterial` as `UnsupportedHeld`
becomes a guarded branch:

```
if (Bucket == MixedMaterial && bExtendNearestHitToMixedMaterial == false) {
    // Restored-veto / IIIE.6.10 baseline
    return UnsupportedHeld;
}
// Continue with existing nearest-hit evaluation.
```

### New audit fields

**Per-record:**
- `bUsedNearestHitOnMixedMaterial` (bool): true if a record was resolved
  via nearest-hit applied to a `MixedMaterial` bucket. Implies
  `bUsedNearestHitTieBreak == true` already.
- (Sub-option 1) `bUsedDistanceTieFallbackOnMixedMaterial` (bool):
  true if a mixed-material distance-tied record was resolved via
  IIIE.6.6 fallback. Implies `bUsedDistanceTieFallback == true`.

**Aggregate:**
- `NearestHitOnMixedMaterialCount`: total per cadence.
- `DistanceTieFallbackOnMixedMaterialCount`: expected tiny at default scale
  (0-2 across the IIIE.6.10 swept rows), not silently folded into nearest-hit.
- Per-step distribution preserved through the audit hash.

### Acceptance criteria

1. **Default 100k/40 seed-42 land=0.30 selection closes at every swept
   step, with bounded live-apply probes.** Gate: `unsupported == 0` and
   `unresolved == 0` for all step values in {1, 8, 15, 16, 17, 20, 24,
   32, 33, 40, 48, 60}. Live-apply probes at the formerly failing editor
   steps (`16`, `32`, `33`, plus auto-cadence step `32`) must report mode
   `phase_iii_e6_live_apply` and `events 0→1`. A full every-step apply
   sweep is supported via commandlet flag and should use the monitored
   runner because the apply path is still known-expensive.

2. **Auto cadence at step 32 advances.** Gate: same as above.

3. **Mixed-material strict-unique-nearest resolves at 1e-9 km tolerance.**
   Gate: `NearestHitOnMixedMaterialCount` matches the IIIE.6.10 mixed-material
   counts (within motion-step variance). At step 16: ~4,301. At step 32: ~5,480.
   At step 60: ~8,861.

4. **Mixed-material distance ties route through IIIE.6.6.**
   Gate: `DistanceTieFallbackOnMixedMaterialCount` is non-zero only when
   mixed-material distance ties exist. At default scale: expected tiny tail,
   e.g. 2 at steps 15/33 and 1 at steps 17/20/24/48/60 in the IIIE.6.10
   sweep.

5. **IIIE.6.10 baseline reproducible via opt-out.** With
   `bExtendNearestHitToMixedMaterial = false`, the IIIE.6.10 sweep shows the
   same unsupported counts (4,301 at step 16, etc.). Audit trail preserved.

6. **Forbidden-policy counters stay zero.** `bUsedPolicyWinner = 0`,
   `bUsedPriorOwnerFallback = 0`, projection-authority counter zero. The
   amendment does not introduce new forbidden-policy invocations.

7. **Same-seed determinism preserved.** Two replays of the live remesh
   produce byte-identical hashes.

8. **No regression in pre-existing IIIE gates.** All prior IIIE slice
   commandlets pass: IIIE.3, IIIE.5, IIIE.6.3, IIIE.6.5 (with updated
   fixture), IIIE.6.6 (with optional new mixed-material fixtures).

### Stop conditions

- Stop if removing the decline introduces any new forbidden policy
  invocation.
- Stop if observability reveals the resolved mixed-material cases cluster
  at suspicious geometric features (e.g., all 4,301 samples are at exactly
  one boundary edge — would suggest a real bug being papered over).
- Stop if same-seed determinism breaks.
- Stop if any IIIE.6.10 step in the sweep still shows `unsupported > 0`
  after the amendment lands.

## Performance note

Mixed-material strict-unique-nearest resolution doesn't add new
boundary-pair query cost; it reuses the existing nearest-hit evaluation
that's already running on those samples (just changes the outcome from
`UnsupportedHeld` to `UniqueNearestHit`). The 5+ minute boundary-pair
query cost surfaced in IIIE.6.7 is independent of this slice; perf
optimization remains owed to a future IIIE.6.12 (originally tagged
IIIE.6.10 before the manual-cadence sweep slice took that name).

## Disclosure language for IIIE.6.11 slice report

```
IIIE.6.11 amends the existing nearest-hit lab policy (CarrierLab IIIE
lab policy #6) to include mixed-material multi-hit cases when there is
a strict unique nearest hit within 1e-9 km tolerance. The amendment
removes the IIIE.6.5 mixed-material decline introduced as CarrierLab
discipline without paper backing. It does NOT add a new IIIE lab policy.
The IIIE consolidation named-lab-policy count remains at seven.

Empirical justification: IIIE.6.10 diagnosed 4,301-8,861 mixed-material
unsupported holds per cadence at editor default (100k/40 seed-42 land=0.30)
across the manual remesh step sweep. The observed cases are overwhelmingly
strict unique-nearest at the 1e-9 km tolerance, with a tiny 0-2 record
distance-tie tail routed through IIIE.6.6 by this design. Process-mark
correlation 0/0/0 rules out upstream IIIB/IIIC/IIID tracking failure.

Source-text framing: thesis §3.3.1.3 (page 62) explicitly uses material-
aware routing on the convergence side (continental check switches
subduction → collision); thesis §3.3.2.3 (page 68) is silent on
material-aware routing on the divergence/remesh side. The amendment
preserves this asymmetry: nearest-hit at remesh remains material-blind,
just with the previous CarrierLab-discipline mixed-material decline no longer
carved out.

This slice is amendment, NOT paper-literal restoration. The paper is
silent on multi-valid hits in general, and nearest-hit itself remains
CarrierLab lab policy. Distance-tied mixed-material cases (rare but present
at default scale) flow through the existing IIIE.6.6 fallback hierarchy as a
sub-option of this amendment.
```

## Stop conditions for IIIE consolidation

- Stop if IIIE consolidation report claims this amendment was paper-cited.
  It wasn't — both the amendment and the underlying nearest-hit rule are
  CarrierLab lab policy.
- Stop if the named-lab-policy count is reported as eight. The
  amendment extends an existing policy; it doesn't add one.
- Stop if Option B (continental priority) is implemented before IIIH.1
  evidence justifies it. The default path stays material-blind nearest-hit.

## Open questions

1. **Does Option A's material-blind nearest-hit at boundaries cause
   continental erosion over many cadences?** Unknown until IIIH.1.
   The follow-up path if yes is to add Option B as a later slice
   (continental priority for mixed-material multi-hits would become an
   eighth lab policy at that point, with empirical justification from
   long-horizon Auth CAF data).

2. **Is the 1e-9 km tolerance still appropriate for mixed-material
   cases?** Same tolerance as cross-plate-different unique-nearest in
   IIIE.6.5. The empirical IIIE.6.10 data shows a tiny mixed-material
   distance-tie tail at this tolerance. If the tolerance is too tight
   at scale (large mixed-material distance-tie counts surface), a future
   tolerance review can adjust.

3. **Should the disclosure call out that the amendment closes a
   CarrierLab-invented decline (paper-silent on whether to decline)
   rather than removes a paper-violating constraint (which IIIE.6.9
   was)?** Yes. The disclosure language above does this; verify the
   slice report preserves it.

## Recommendation

**Ready for Codex execution as designed.** Small slice — extends an
existing rule's bucket coverage, removes the IIIE.6.5 mixed-material
decline, adds opt-out flag, updates fixtures.

Estimated scope: ~150-250 lines including commandlet fixture updates
and report. After this slice lands at default 100k/40 seed-42 land=0.30:
- Selection closes at every swept step, and the manual remesh button advances
  at the formerly failing editor probes (`16`, `32`, `33`) without the
  mixed-material hold
- Auto cadence at step 32 advances state
- The visual unblock the user has been climbing toward is real
- IIIE.6.12 perf optimization can proceed on settled semantics
- IIIE consolidation can begin

---

**Summary for independent review:** IIIE.6.11 amends the existing
CarrierLab nearest-hit lab policy (#6 of seven IIIE lab policies) to
include strict unique-nearest mixed-material multi-hits at 1e-9 km
tolerance, removing the IIIE.6.5 `UnsupportedHeld` decline introduced
without paper backing. Empirical justification comes from IIIE.6.10's
manual-cadence step sweep at editor default (100k/40 seed-42 land=0.30):
mixed-material unsupported counts are 4,301-8,861 per cadence and
are overwhelmingly strict unique-nearest with a tiny 0-2 record distance-tie tail. The
amendment preserves the thesis's asymmetry (convergence-side material-
aware routing per §3.3.1.3, divergence-side material-blind ray-cast per
§3.3.2.3). Distance-tied mixed-material cases (effectively zero at
default scale but present in the sweep) compose with the existing IIIE.6.6
fallback hierarchy.
The IIIE consolidation lab-policy count remains at seven; this is
amendment, not addition. The amendment is NOT paper-literal restoration
(IIIE.6.9's pattern); it is amendment of an existing CarrierLab lab
policy.
