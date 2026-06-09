# Milestone 3 Fable Follow-Up Review Prompt

Paste this into Fable after the revised M3 design doc is available.

```text
You previously reviewed CarrierLab Milestone 3 pre-implementation design at
local HEAD 5e46db9 and returned GO WITH CONDITIONS. I revised
docs/checkpoints/milestone-3-design-pass.md to address your findings.

Please perform a follow-up review focused on whether the design blockers were
actually closed, not a fresh broad review unless you see a new blocker.

Repository state:

- Branch: codex/v2-0-carrier
- Design docs are still pre-implementation and may be untracked locally.
- Milestone 3 implementation has not begun.

Files to review:

- docs/checkpoints/milestone-3-design-pass.md
- docs/checkpoints/milestone-3-entry-packet.md
- docs/checkpoints/milestone-2-closeout-report.md
- docs/paper-resampling-extraction.md
- docs/phase-ii-subduction-design.md
- docs/phase-ii-pre-mortem.md
- Source/CarrierLab/Public/CarrierLabV2Core.h
- Source/CarrierLab/Private/CarrierLabV2Core.cpp

What changed in response to your review:

1. The zero-raw-hit rule now has a q1/q2 divergence predicate. In filters-on
   mode, q1/q2 generation requires a q1/q2 pair from two plates, qGamma, and a
   positive current opening rate at qGamma above a named tolerance. Otherwise
   the sample becomes deferred-nondivergent-gap and increments
   q1q2_divergence_rejected_count.
2. The filters-off baseline mechanism no longer claims an M3 config can match
   M2 config-entangled hashes. The design now requires rerunning literal M2
   fixture configs through FCarrierV2Milestone2::RunFixtureWithReplay and
   comparing against pinned compile-time constants from M2-FX-001..006 plus
   SCALE-50K-M2.
3. Contact evidence is now specified as plate-local boundary geometry, not
   global sample overlap hit sets. The design adds a resolution-invariant
   plate-local contact/label hash gate.
4. Zero-valid-after-filter q1/q2 suppression is reclassified as a deliberate
   CarrierLab lab policy / thesis deviation until later collision/terrane
   material prerequisites exist.
5. Previous-blocked sample lineage is now measure-only. It lives in fixture or
   metrics structures and no write/assignment path may read it.
6. The fixture ladder now includes filters-on inert, O/O ambiguous, C/C
   collision-candidate, divergence-no-labels, resolution-invariant labels, and
   same-pair mixed-signal.
7. The no-hole-growth hard gate is scoped to resolvable O/C fronts; ambiguous
   O/O and C/C deferred growth is characterization with an M4 dependency.
8. Numeric performance caps are pre-registered for 250k: full resample cycle
   <= 3580 ms and contact+label+filter process lane <= 260 ms.

Please answer:

1. Are the two original design blockers now closed?
2. Is the q1/q2 divergence predicate stateless and clean-room safe, or does it
   still risk hidden ownership or a disguised resolver?
3. Is the pinned literal-M2 baseline gate implementable against the existing M2
   code and strong enough as a regression check?
4. Does boundary-triangle-derived contact evidence plus the resolution
   invariance gate sufficiently avoid process authority depending on substrate
   sample resolution?
5. Is the zero-valid-after-filter lab policy now honestly classified against
   the thesis, with restoration preconditions clear enough?
6. Is previous-blocked lineage now safely measure-only?
7. Are the added fixtures and stop conditions sufficient before implementation?
8. Are the performance caps reasonable and pre-registered enough?
9. What, if anything, must still change before implementation begins?

Please return findings first, ordered by severity. Classify each as:

- blocker before implementation
- implementation watch item
- wording/reporting cleanup
- optional improvement

End with GO / GO WITH CONDITIONS / NO-GO for beginning Milestone 3
implementation after any final doc edits.
```
