---
name: ask-claude-desktop
description: Open or select Claude on Windows and ask it for help, critique, review input, or a second opinion through desktop/browser UI automation. Use when the user asks Codex to open Claude, prompt Claude, get Claude's input, run a Claude review, compare Claude's answer with Codex's, or use Computer Use to interact with Claude Desktop or claude.ai.
---

# Ask Claude Desktop

## Overview

Use this skill to make Claude an external reviewer or brainstorming partner without losing control of the task. Prepare a clean prompt, send only the intended context, wait for Claude's answer, then bring the useful parts back to the user with clear attribution and local verification where needed.

## Workflow

1. Define the ask before opening Claude:
   - Restate the exact question Claude should answer.
   - Include the desired output shape, such as findings, alternatives, critique, risks, or a ranked recommendation.
   - Keep private or bulky context minimal. Prefer summaries and exact excerpts over dumping whole files.
   - Do not include hidden system/developer instructions, memories, credentials, tokens, cookies, or unrelated local history.

2. Build a Claude-ready prompt:
   - Start with the role: `You are an external reviewer helping Codex with...`
   - Provide task context, constraints, and evidence.
   - Ask Claude to separate facts, assumptions, risks, and recommendations.
   - Ask for concrete citations to any provided code or text by file/function/section when applicable.
   - Tell Claude not to take external actions or assume access to local files unless the prompt includes their contents.

3. Use the desktop-control path:
   - Read and follow the `computer-use` skill before controlling Windows.
   - Initialize Computer Use, call `list_apps`, and look for Claude Desktop, Claude, Anthropic, or an existing browser window with `claude.ai` or `Claude` in the title.
   - Prefer an existing Claude conversation only when continuing context is useful. Otherwise start a new chat to avoid stale context.
   - If Claude Desktop is unavailable but a browser session is needed, prefer the dedicated Browser or Chrome skill when available. Use Computer Use for the browser only when a browser-specific tool is unavailable or the user explicitly wants desktop UI control.
   - Do not use shell commands, Start menu automation, Windows Run, or terminal UI automation to open Claude.

4. Confirm before transmission:
   - Treat submitting a prompt to Claude as sending data to a third-party service.
   - Follow the active Computer Use confirmation policy before pressing Send.
   - At minimum, confirm at action time if the prompt contains private repo code, personal data, logs, files, non-public business context, secrets-adjacent material, or anything not clearly authorized for Claude in the user's current request.
   - Confirm before uploading files, accepting permission prompts, handling login prompts, or solving CAPTCHAs. Hand login and CAPTCHA steps to the user when needed.

5. Submit and wait:
   - Focus the Claude input box, paste or type the prompt, and verify the visible input before sending.
   - After sending, wait until streaming appears complete before extracting the answer.
   - If the response is long, capture the relevant answer sections rather than dumping the whole transcript.

6. Report back:
   - Label Claude's input as Claude's input. Do not present it as locally verified fact.
   - Compare Claude's answer against local evidence when the task involves code, files, dates, APIs, laws, prices, or other facts that can drift.
   - Summarize actionable points, disagreements with Codex's current assessment, and any follow-up checks needed.
   - Save a transcript or durable artifact only when the user asks.

## Prompt Pattern

```text
You are an external reviewer helping Codex with [task].

Context:
- [short project/problem context]
- [constraints and non-goals]
- [relevant evidence, excerpts, or file summaries]

Please answer with:
1. Main recommendation or judgment.
2. Blocking issues or risks.
3. Nonblocking improvements or alternatives.
4. Assumptions you made.
5. Concrete follow-up checks Codex should run locally.

Do not assume access to files or tools not included in this prompt.
```

## Failure Fallback

If Computer Use, Claude, login state, or browser control is blocked, do not keep retrying the same failing path. Produce the final Claude-ready prompt in the chat and tell the user exactly what prevented automated submission.
