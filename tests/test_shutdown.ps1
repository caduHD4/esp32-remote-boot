$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot '../agent/windows/Power.ps1')
$path=Join-Path ([IO.Path]::GetTempPath()) ([guid]::NewGuid().ToString()+'.ack')
$script:calls=0; $script:accepted=$true; $script:fail=$false
function Stop-Computer { param($ErrorAction) $script:calls++ }
function Invoke-RemoteBootApi {
    param($Method,$Endpoint,$Payload)
    if($script:fail) { throw 'Network failure' }
    return [pscustomobject]@{ack_accepted=$script:accepted}
}
function Assert($condition) { if(-not $condition) { throw 'Shutdown dispatch assertion failed' } }
try {
    $command=[pscustomobject]@{id=('a'*32);action='shutdown';session_id='session-one'}
    $ack=''; $payload=@{}
    Invoke-ShutdownCommand $command $payload $true 'session-one' $path ([ref]$ack)
    Assert ($script:calls -eq 1)
    Invoke-ShutdownCommand $command $payload $true 'session-one' $path ([ref]$ack)
    Assert ($script:calls -eq 1)
    $ack=''
    Invoke-ShutdownCommand $command $payload $false 'session-one' $path ([ref]$ack)
    Invoke-ShutdownCommand $command $payload $true 'new-session' $path ([ref]$ack)
    Assert ($script:calls -eq 1 -and $ack -eq '')
    $script:accepted=$false
    Invoke-ShutdownCommand $command $payload $true 'session-one' $path ([ref]$ack)
    Assert ($script:calls -eq 1)
    $ack=''; $script:fail=$true
    try { Invoke-ShutdownCommand $command $payload $true 'session-one' $path ([ref]$ack) } catch { }
    Assert ($script:calls -eq 1)
    Write-Output 'Windows shutdown dispatch: passed (Stop-Computer mocked)'
} finally { Remove-Item $path -ErrorAction SilentlyContinue }
