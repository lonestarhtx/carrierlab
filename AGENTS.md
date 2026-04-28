## Repo Notes

### Current Project Truth

- CarrierLab is a clean-room falsification lab for the Cortial et al. moving plate-local crust carrier model.
- This is not Aurous 2 and must not port Aurous V6, V9, Prototype A/B/C/D/E, exporter, control panel, sidecar infrastructure, or ownership-recovery logic.
- Pre-coding deliverables live in `docs/paper-carrier-extraction.md`, `docs/driftworld-carrier-comparison.md`, `docs/carrier-design.md`, and `docs/pre-mortem.md`.
- No production code should advance a stage without a written checkpoint report and an explicit user go/no-go.

### Push Verification

- After pushing, verify the remote SHA matches local `HEAD` with `git ls-remote origin <branch>` or by checking GitHub.
- Do not trust an agent-reported push result without remote verification.

### Stage Discipline

- Stage 0: cold-start carrier only. No motion, no mutation.
- Stage 1: rigid motion projection only.
- Stage 1.5: named resampling sub-stage. Do not silently blend it into Stage 1.
- At the end of every stage, write a checkpoint report and wait for explicit user go/no-go before advancing.
- Stop conditions in `docs/pre-mortem.md` and `docs/carrier-design.md` are binding.

### Clean-Room Constraints

- Do not use persistent global sample ownership as tectonic authority.
- Do not use ownership persistence, repair, recovery, backfill, retention, hysteresis, or anchoring heuristics.
- Do not implement subduction, collision, rifting, uplift, erosion, slab pull, or terrain beauty until the carrier itself is proven.
- Tests must use independent recomputation, not value passthrough.

