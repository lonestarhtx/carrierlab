---
name: carrierlab-claim-audit
description: Audit CarrierLab docs and checkpoint reports for stale or over-strong claims. Use when reports, planning packets, checkpoint docs, or skills mention Stage 1.5, paper-faithful behavior, validated foundations, pass/prove language, signature tokens, remesh policy, maps, or visual evidence.
---

# CarrierLab Claim Audit

## Quick Scan

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<skill-dir>\scripts\Invoke-CarrierLabClaimAudit.ps1'
```

Scan only changed docs:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<skill-dir>\scripts\Invoke-CarrierLabClaimAudit.ps1' -ChangedOnly
```

Include changed `SKILL.md` files when the skill text itself is the review
target:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<skill-dir>\scripts\Invoke-CarrierLabClaimAudit.ps1' -ChangedOnly -IncludeSkills
```

## Review Rules

Flag wording that says more than the code/report proves.

High-risk phrases:

- "Stage 1.5 works" without the new foundation-characterization caveat.
- "paper-faithful" attached to standalone Stage 1.5 remesh.
- "proves" where the gate is diagnostic, sampled, or fixture-limited.
- "validated carrier foundation" if it hides the Stage 1.5 limitation.
- "independent signature gate" unless code compares computed vs expected.
- map/PNG language that treats visuals as proof.

Preferred replacement shape:

- "demonstrates in fixture X"
- "foundation characterization"
- "paper-faithful only after IIIB/IIIC/IIID process-state input"
- "diagnostic/control policy"
- "comparison artifact, not authority"

## Output Discipline

List findings with file/line, quote the claim briefly, and suggest replacement
wording. Do not rewrite history by deleting old evidence; add supersession notes
when old reports are intentionally preserved.
