param(
    [string]$SliceId = 'IIIC4',
    [string]$ClassSuffix = 'PhaseIIIC4',
    [string]$ReportPath = ''
)

if (-not $ReportPath) {
    $ReportPath = "docs/checkpoints/phase-iii-slice-$($SliceId.ToLower())-report.md"
}

$header = "Source/CarrierLab/Public/CarrierLab$ClassSuffix`Commandlet.h"
$source = "Source/CarrierLab/Private/CarrierLab$ClassSuffix`Commandlet.cpp"

$lines = @(
    "# CarrierLab Commandlet Scaffold: $SliceId",
    "",
    "Files:",
    "- $header",
    "- $source",
    "- $ReportPath",
    "",
    "Minimum gates:",
    "- bypass/off hash gate when mutation is opt-in",
    "- IIIB independent-signature regression for IIIC+ convergence-dependent slices: 4df40569f5e51e1a",
    "- primary positive fixture with independent oracle",
    "- zero-motion negative",
    "- single-plate negative when plate contacts are involved",
    "- same-seed replay hash equality",
    "- report-claim scope note",
    "",
    "Implementation reminders:",
    "- Main returns 0 only when all gates pass",
    "- write JSONL metrics under Saved/CarrierLab/<phase>/<slice>/<timestamp>/metrics.jsonl",
    "- write checkpoint report under docs/checkpoints",
    "- do not stage Saved artifacts by default"
)

$lines -join "`n"
