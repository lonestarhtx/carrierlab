param(
    [string]$SliceName = 'CarrierLab slice',
    [string]$Branch = 'master'
)

$lines = @(
    "# CarrierLab Release Checklist: $SliceName",
    "",
    "1. Confirm repo state",
    "   - git status --short",
    "   - git log -1 --oneline",
    "   - read AGENTS.md and current stage/slice docs",
    "",
    "2. Scope check",
    "   - git diff --stat",
    "   - verify no Aurous/V6/V9/Prototype code port",
    "   - verify no persistent global sample ownership authority",
    "",
    "3. Guardrails",
    "   - rg forbidden terms in Source/docs",
    "   - confirm docs/checkpoint report exists if closing a stage or slice",
    "",
    "4. Validation",
    "   - if code changed: build CarrierLabEditor",
    "   - if docs only: git diff --check",
    "   - run targeted commandlets/tests required by the slice",
    "",
    "5. Commit",
    "   - git add only intended files",
    "   - git diff --cached --check",
    "   - git commit -m ""<narrow message>""",
    "",
    "6. Push and verify",
    "   - git push origin $Branch",
    "   - local: git rev-parse HEAD",
    "   - remote: git ls-remote origin $Branch",
    "   - local SHA must equal remote SHA"
)

$lines -join "`n"
