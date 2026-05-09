---
name: carrierlab-page-image-research
description: Produce CarrierLab paper/thesis research checkpoints from local page images with explicit/implicit/silent classification, verbatim citations, French keyword sweeps, and a strict no-code/no-design boundary.
---

# CarrierLab Page-Image Research

## Use When

- A CarrierLab slice depends on what the Cortial thesis or CGF paper actually says.
- The user asks for research-only, page-image authority, or source silence.
- A behavior may be paper-faithful, lab policy, or an accidental CarrierLab rule.

## Source Order

1. Read the relevant local checkpoint/report to understand the symptom.
2. Search extracted text only to find candidate pages.
3. Inspect thesis/CGF page images directly for final citations.
4. Use Driftworld or external references only as tertiary/non-authoritative context.

## Citation Discipline

- Quote verbatim where possible.
- Cite image filename and section/page, for example `cc5c6807-079.png`, §3.3.2.3.
- If the page is silent, write `source silent`; do not paraphrase a rule into existence.
- Separate thesis, CGF, and tertiary evidence.

## Evidence Categories

Use these categories in the checkpoint:

- `explicit`: source states the rule.
- `implicit`: source strongly points at the rule but does not specify mechanics.
- `silent`: inspected source does not answer.
- `contradicted`: source text points away from the proposed rule.
- `tertiary`: useful comparison, not authority.

## French Keyword Sweeps

For boundary/remesh work, search likely French terms:

- `divergence`, `dorsale`, `rift`, `frontière`, `plaque`
- `intersection`, `rééchantillonnage`, `triangulation`, `TDS`
- `vitesse relative`, `mouvement`, `éloignement`, `ouverture`
- `glissement`, `coulissage`, `cisaillement`, `décrochement`, `transformante`
- `subduction`, `collision`, `obduction`

## Checkpoint Shape

```markdown
## Status
recommendation-ready / needs-user-decision / blocked-on-source

## Source 0 Result
Implementation symptom only; not paper authority.

## Thesis Re-Read
Quotes, image citations, explicit/implicit/silent classification.

## CGF Re-Read
Condensed paper evidence.

## Tertiary Comparison
Driftworld or other source, clearly non-authoritative.

## Synthesis
Table: question | thesis | CGF | tertiary | consequence.

## Recommendation
State whether next slice is thesis-cited implementation, lab-policy design, or stop.
```

## Guardrails

- Research only means no code edits and no implementation design.
- Do not turn "physically plausible" into "paper says."
- Do not call a visual/map artifact proof.
- Do not hide source conflict; name it.
