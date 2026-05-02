param(
    [string]$SliceName = 'CarrierLab slice',
    [string]$Commandlet = '',
    [string]$Branch = 'master'
)

$commandletLine = if ($Commandlet) {
    "   - run UnrealEditor-Cmd -run=$Commandlet and inspect the generated checkpoint"
} else {
    "   - run the targeted commandlet/test required by the slice"
}

$lines = @(
    "# CarrierLab Slice Release Checklist: $SliceName",
    "",
    "1. Repo and scope",
    "   - git status --short",
    "   - git log -1 --oneline",
    "   - confirm the user-approved slice boundary",
    "",
    "2. Diff hygiene",
    "   - git diff --check",
    "   - git diff --stat",
    "   - scan touched Source/docs for forbidden authority-drift terms",
    "",
    "3. Validation",
    "   - if C++ changed: run carrierlab-build",
    $commandletLine,
    "   - for IIIC+ convergence-dependent slices: checkpoint includes IIIB independent signature 4df40569f5e51e1a",
    "   - read docs/checkpoints/<slice-report>.md before claiming pass",
    "",
    "4. Stage and commit",
    "   - git add -- <explicit intended paths>",
    "   - git diff --cached --check",
    "   - git diff --cached --name-only",
    "   - git commit -m ""<narrow message>""",
    "",
    "5. Push and verify",
    "   - git push origin $Branch",
    "   - powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<git-push-verify>/scripts/Test-GitPushVerified.ps1'",
    "   - local HEAD must equal remote HEAD"
)

$lines -join "`n"
