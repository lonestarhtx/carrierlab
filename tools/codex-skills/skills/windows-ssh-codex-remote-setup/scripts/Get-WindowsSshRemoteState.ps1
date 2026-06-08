param()

$ErrorActionPreference = 'Stop'

function Test-ExistingPath {
    param([string]$Path)
    try {
        [pscustomobject]@{
            path = $Path
            exists = Test-Path -LiteralPath $Path -ErrorAction Stop
            access_error = $null
        }
    }
    catch {
        [pscustomobject]@{
            path = $Path
            exists = $null
            access_error = $_.Exception.Message
        }
    }
}

function Get-CommandPath {
    param([string]$Name)
    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return $null
}

$identity = [System.Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object System.Security.Principal.WindowsPrincipal($identity)

$ipConfigs = @()
try {
    $ipConfigs = Get-NetIPConfiguration |
        Where-Object { $_.IPv4Address -and $_.NetAdapter.Status -eq 'Up' } |
        ForEach-Object {
            [pscustomobject]@{
                interface = $_.InterfaceAlias
                ipv4 = @($_.IPv4Address | ForEach-Object { $_.IPAddress })
                gateway = @($_.IPv4DefaultGateway | ForEach-Object { $_.NextHop })
            }
        }
}
catch {
    $ipConfigs = @([pscustomobject]@{ error = $_.Exception.Message })
}

$services = foreach ($name in @('sshd', 'ssh-agent')) {
    $svc = Get-Service -Name $name -ErrorAction SilentlyContinue
    if ($svc) {
        [pscustomobject]@{
            name = $svc.Name
            status = $svc.Status.ToString()
            start_type = $svc.StartType.ToString()
        }
    }
    else {
        [pscustomobject]@{
            name = $name
            status = 'missing'
            start_type = $null
        }
    }
}

$firewallRules = @()
try {
    $firewallRules = @(Get-NetFirewallRule -ErrorAction SilentlyContinue |
        Where-Object { $_.DisplayName -like '*OpenSSH*' -or $_.Name -like '*OpenSSH*' } |
        Select-Object DisplayName, Enabled, Direction, Action, Profile)
}
catch {
    $firewallRules = @([pscustomobject]@{ error = $_.Exception.Message })
}

$sshdConfigPath = 'C:\ProgramData\ssh\sshd_config'
$sshdConfigSignals = @()
if (Test-Path -LiteralPath $sshdConfigPath) {
    $sshdConfigSignals = @(Select-String -LiteralPath $sshdConfigPath -Pattern 'AuthorizedKeysFile|Match Group administrators' |
        ForEach-Object {
            [pscustomobject]@{
                line = $_.LineNumber
                text = $_.Line.Trim()
            }
        })
}

[pscustomobject]@{
    hostname = $env:COMPUTERNAME
    user = $identity.Name
    is_administrator = $principal.IsInRole([System.Security.Principal.WindowsBuiltInRole]::Administrator)
    ip_configs = $ipConfigs
    command_paths = [pscustomobject]@{
        ssh = Get-CommandPath 'ssh.exe'
        sshd = Get-CommandPath 'sshd.exe'
        ssh_keygen = Get-CommandPath 'ssh-keygen.exe'
        winget = Get-CommandPath 'winget.exe'
    }
    services = $services
    paths = @(
        Test-ExistingPath 'C:\Windows\System32\OpenSSH\sshd.exe'
        Test-ExistingPath 'C:\ProgramData\ssh\sshd_config'
        Test-ExistingPath 'C:\ProgramData\ssh\administrators_authorized_keys'
        Test-ExistingPath (Join-Path $env:USERPROFILE '.ssh\authorized_keys')
    )
    sshd_config_signals = $sshdConfigSignals
    firewall_rules = $firewallRules
} | ConvertTo-Json -Depth 5
