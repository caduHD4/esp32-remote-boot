$ErrorActionPreference='Stop'
$repo=Split-Path (Split-Path (Split-Path $PSScriptRoot))
$installer=Join-Path $repo 'installer/windows/install-agent.ps1'
$source=Get-Content -LiteralPath $installer -Raw
$errors=$null
[System.Management.Automation.Language.Parser]::ParseFile($installer,[ref]$null,[ref]$errors)|Out-Null
if($errors){throw 'Installer syntax errors'}
function Get-ScheduledTask {return $null}
function Stop-ScheduledTask {}
function New-ScheduledTaskAction {return @{}}
function New-ScheduledTaskTrigger {return @{}}
function New-ScheduledTaskPrincipal {return @{}}
function New-ScheduledTaskSettingsSet {return @{}}
function Register-ScheduledTask {$script:registered=$true}
function Start-ScheduledTask {if(-not $script:checked){throw 'Startup before hello verification'};$script:started=$true}
function Invoke-FakeAcl {$script:protected=$true;$global:LASTEXITCODE=0}
function Invoke-FakeAgent {
    $global:LASTEXITCODE=0
    if($args -contains '--pair') {
        if(-not $script:protected){throw 'Pairing before ACL protection'}
        if($script:scenario -eq 'cancel'){$global:LASTEXITCODE=1;return}
        $config=$args[[Array]::IndexOf($args,'--config')+1]
        Set-Content -LiteralPath $config -Value '{"fixture":true}'
        $script:paired=$true
        if($script:scenario -eq 'check-failure'){$global:LASTEXITCODE=1;return}
        $script:checked=$true
    }
    if($args -contains '--check') {
        if(-not $script:paired){throw 'Verification before pairing'}
        if($script:scenario -eq 'check-failure'){$global:LASTEXITCODE=1;return}
        $script:checked=$true
    }
}
function Read-Host {return ''}
foreach($scenario in @('approved','cancel','check-failure')) {
    $sandbox=Join-Path ([IO.Path]::GetTempPath()) ('rb-installer-'+[Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $sandbox|Out-Null
    try {
        $script:scenario=$scenario;$script:registered=$false;$script:started=$false;$script:checked=$false;$script:paired=$false;$script:protected=$false
        $binary=Join-Path $sandbox 'source.exe';Set-Content -LiteralPath $binary -Value 'fixture'
        $script:fixtureDestination=Join-Path $sandbox 'protected'
        $code=$source.Replace('#Requires -RunAsAdministrator','')
        $code=$code.Replace('$root=Split-Path (Split-Path $PSScriptRoot)','$root="fixture"')
        $code=$code.Replace("Join-Path `$env:ProgramData 'RemoteBoot'",'$script:fixtureDestination')
        $code=$code.Replace('& icacls.exe','Invoke-FakeAcl').Replace('& $AgentFile --self-test','Invoke-FakeAgent --self-test').Replace('& $stagedBinary @pairArguments','Invoke-FakeAgent @pairArguments').Replace('& $stagedBinary --config $pending --check','Invoke-FakeAgent --config $pending --check')
        $failed=$false
        try {& ([ScriptBlock]::Create($code)) -EspAddress '127.0.0.1' -AgentFile $binary} catch {$failed=$true;Write-Host $_}
        if($scenario -eq 'approved') {
            if($failed -or -not $script:registered -or -not $script:started -or -not (Test-Path (Join-Path $script:fixtureDestination 'agent.json'))){throw 'Approved installer fixture failed'}
        } elseif(-not $failed -or $script:registered -or $script:started){throw "Unsafe installer failure: $scenario"}
        if(Test-Path (Join-Path $script:fixtureDestination 'agent.pending.json')){throw 'Staging credential remained'}
        Write-Host "PASS: Windows installer fixture $scenario"
    } finally {Remove-Item -LiteralPath $sandbox -Recurse -Force}
}
$global:LASTEXITCODE=0
exit 0
