---
name: windows-ssh-codex-remote-setup
description: Diagnose or set up SSH from a MacBook or other client into Michael's Windows Codex machine for remote Codex access. Use when Windows `sshd` is missing or will not start, OpenSSH Server installation fails, Mac-to-Windows SSH reports no route/password prompts, aliases or identity files do not match, or the user wants phone-to-Mac-to-Windows Codex access.
---

# Windows SSH Codex Remote Setup

## Goal

Get to one verified outcome: the Mac/client can SSH into the Windows Codex host without a password, and the Windows host shows `sshd` running with the expected key path.

Keep three layers separate:

- Codex product support: if the user asks what Codex officially supports today, use official OpenAI docs before answering.
- Windows server setup: install/start/configure OpenSSH Server and firewall.
- Client auth: verify the Mac alias, IP route, identity file, and authorized key path are correct.

## Quick Inventory

Run the read-only helper first from the repo or skill root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File '<skill-dir>\scripts\Get-WindowsSshRemoteState.ps1'
```

Use it to avoid guessing the hostname, active LAN IPs, `sshd` service state, OpenSSH paths, firewall rule, and admin-key routing.

## Windows Server Workflow

1. Confirm scope:
   - The target is inbound SSH to Windows, not GitHub SSH auth.
   - Capture hostname, Windows username, active LAN IP, and whether the Windows user is in Administrators.
   - Do not assume previous values like `JARVIS` or `192.168.0.16` are still current; verify them.

2. If `Start-Service sshd` fails:
   - Check `Test-Path C:\Windows\System32\OpenSSH\sshd.exe`.
   - Check `Get-Service sshd,ssh-agent -ErrorAction SilentlyContinue`.
   - If `sshd.exe` and the `sshd` service are missing, the server payload is not installed.

3. Install OpenSSH Server only with user/admin approval:
   - The capability name has four tildes: `OpenSSH.Server~~~~0.0.1.0`.
   - `Get-WindowsCapability -Online` and `Add-WindowsCapability` require an elevated admin shell.
   - If `Add-WindowsCapability` fails with `0x800f0950`, pivot to DISM/component-store repair or package discovery instead of looping on `Start-Service`.
   - On this machine, a prior working fallback used `winget search OpenSSH` and found `Microsoft.OpenSSH.Preview`; re-discover the package ID before using it.

4. Start and expose the service:
   - `Set-Service sshd -StartupType Automatic`
   - `Start-Service sshd`
   - Enable the Windows firewall rule for OpenSSH on the intended network profile, usually Private.

## Key Placement

Use the Mac/client public key only. Never ask for or store a private key.

If the Windows user is in the local Administrators group, inspect `C:\ProgramData\ssh\sshd_config` for `Match Group administrators`. Windows OpenSSH commonly routes admin users to:

```text
C:\ProgramData\ssh\administrators_authorized_keys
```

Putting the key only in `C:\Users\Michael\.ssh\authorized_keys` may not work for admin users. After editing key files, restart `sshd` and test again.

## Mac-Side Diagnosis

Ask the user to run one command at a time and paste only the command output, not the `PS ...>` or shell prompt prefix.

Useful Mac probes:

```bash
ipconfig getifaddr en0
route get <windows-ip>
nc -vz <windows-ip> 22
ssh -G <alias>
ssh -i ~/.ssh/<identity-file> michael@<windows-ip>
```

Rules:

- Avoid leading zeros in IPv4 literals; `192.168.0.016` can be parsed as a different address.
- Use `nc -vz <ip> 22` to separate network reachability from SSH auth.
- Use `ssh -G <alias>` to verify the alias resolves to the intended hostname, user, port, and identity file.
- If old aliases conflict, create a fresh alias such as `jarvis-codex` instead of fighting stale config.

## Stop Conditions

- Do not claim setup is done until direct or alias SSH works without a password and Windows shows `sshd` running.
- Stop and ask for an elevated admin shell when installation, service creation, or firewall changes require elevation.
- Stop if the user pasted prompt text or error blocks back into PowerShell; tell them to paste only the raw command body, one line at a time.
- Stop if the network profile is Public or unknown and the firewall change would expose SSH more broadly than intended.

## Answer Shape

```text
Current layer: Windows server / key placement / Mac alias / network route
What is verified: <hostname, IP, service, firewall, key path, alias>
What is failing: <exact command and error>
Next command: <one command only>
Done when: <passwordless SSH plus running sshd>
```
