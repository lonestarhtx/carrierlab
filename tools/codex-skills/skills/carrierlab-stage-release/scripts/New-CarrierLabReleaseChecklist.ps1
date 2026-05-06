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
    "   - if code changed: build CarrierLabEditor; in Codex Desktop request escalation on the first real build because UBT writes AppData caches",
    "   - if docs only: git diff --check",
    "   - run targeted commandlets/tests required by the slice; run real UnrealEditor-Cmd commandlets with escalation on the first attempt and verify log/report/output files",
    "",
    "5. Commit",
    "   - git add only intended files",
    "   - if git add fails on .git/index.lock, verify no competing git process, then rerun the same scoped git add with escalation; do not delete the lock as a first response",
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
