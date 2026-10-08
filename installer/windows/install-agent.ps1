#Requires -RunAsAdministrator
param([Parameter(Mandatory)][string]$EspAddress, [string]$AgentFile)
$ErrorActionPreference='Stop'
$root=Split-Path (Split-Path $PSScriptRoot);$destination=Join-Path $env:ProgramData 'RemoteBoot'
if(-not $AgentFile){$AgentFile=Join-Path $root 'build\agent\win-x64\remote-boot-agent.exe'}
if(-not (Test-Path -LiteralPath $AgentFile -PathType Leaf)){throw 'Build or download the win-x64 NativeAOT agent first; pass -AgentFile PATH'}
& $AgentFile --self-test
if($LASTEXITCODE -ne 0){throw 'Native agent self-test failed'}
New-Item -ItemType Directory -Path $destination -Force|Out-Null
& icacls.exe $destination /reset|Out-Null
if($LASTEXITCODE -ne 0){throw 'Cannot reset agent directory ACL'}
& icacls.exe $destination /inheritance:r /grant:r '*S-1-5-18:(OI)(CI)F' '*S-1-5-32-544:(OI)(CI)F'|Out-Null
if($LASTEXITCODE -ne 0){throw 'Cannot protect agent configuration'}
$allow=(Read-Host 'Type REBOOT to allow dashboard-confirmed reboot commands') -eq 'REBOOT'
$allowShutdown=(Read-Host 'Type SHUTDOWN to allow dashboard/Sinric shutdown commands') -eq 'SHUTDOWN'
$pending=Join-Path $destination 'agent.pending.json'
$stagedBinary=Join-Path $destination 'remote-boot-agent.pending.exe'
try {
    Copy-Item -LiteralPath $AgentFile -Destination $stagedBinary -Force
    if(Get-ScheduledTask -TaskName RemoteBootAgent -ErrorAction SilentlyContinue) { Stop-ScheduledTask -TaskName RemoteBootAgent }
    $pairArguments=@('--pair','--url',"http://$EspAddress",'--config',$pending)
    if($allow){$pairArguments+='--allow-reboot'}
    if($allowShutdown){$pairArguments+='--allow-shutdown'}
    Write-Host 'Open the dashboard pairing window, then approve the short code for the intended PC.'
    & $stagedBinary @pairArguments
    if($LASTEXITCODE -ne 0){throw 'Pairing or hello verification failed; startup task was not enabled'}
    Move-Item -LiteralPath $pending -Destination (Join-Path $destination 'agent.json') -Force
    Move-Item -LiteralPath $stagedBinary -Destination (Join-Path $destination 'remote-boot-agent.exe') -Force
    $arguments='--config "'+(Join-Path $destination 'agent.json')+'"'
    $action=New-ScheduledTaskAction -Execute (Join-Path $destination 'remote-boot-agent.exe') -Argument $arguments -WorkingDirectory $destination
    $trigger=New-ScheduledTaskTrigger -AtStartup
    $principal=New-ScheduledTaskPrincipal -UserId 'SYSTEM' -LogonType ServiceAccount -RunLevel Highest
    $settings=New-ScheduledTaskSettingsSet -ExecutionTimeLimit ([TimeSpan]::Zero) -RestartCount 10 -RestartInterval (New-TimeSpan -Minutes 1) -MultipleInstances IgnoreNew
    Register-ScheduledTask -TaskName RemoteBootAgent -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Force|Out-Null
    Start-ScheduledTask RemoteBootAgent
} finally {
    if(Test-Path -LiteralPath $pending){Remove-Item -LiteralPath $pending -Force}
    if(Test-Path -LiteralPath $stagedBinary){Remove-Item -LiteralPath $stagedBinary -Force}
}
