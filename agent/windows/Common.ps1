Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if (-not ('Firmware' -as [type])) { Add-Type -Path (Join-Path $PSScriptRoot 'Firmware.cs') }
[Firmware]::Enable()
function Get-BootCatalog {
    param([switch]$FullScan)
    $order=@([Firmware]::Order()); $ids=[Collections.Generic.List[uint16]]::new()
    foreach($id in $order) { if(-not $ids.Contains($id)) { $ids.Add($id) } }
    if($FullScan) { for($i=0;$i -le 65535;$i++) { if(-not $ids.Contains([uint16]$i)) { $ids.Add([uint16]$i) } } }
    $entries=@(foreach($id in $ids) {
        $bytes=[Firmware]::Read(('Boot{0:X4}' -f $id))
        if($null -ne $bytes) { try { [Firmware]::Parse($id,$bytes) } catch { Write-Warning ('Skipping malformed Boot{0:X4}' -f $id) } }
    })
    return @{systems=$entries}
}
function Invoke-RemoteBootApi {
    param([string]$Method,[string]$Path,$Body=@{})
    if($script:RBUrl -notmatch '^http://[0-9.]+$' -or $script:RBToken -notmatch '^[A-Za-z0-9_-]{24,128}$') { throw 'Invalid ESP URL or token format' }
    $parameters=@{Uri="$script:RBUrl/api/v1/$Path";Method=$Method;Headers=@{Authorization="Bearer $script:RBToken"};TimeoutSec=8;ContentType='application/json'}
    if($Method -ne 'GET') { $parameters.Body=($Body|ConvertTo-Json -Depth 12 -Compress) }
    Invoke-RestMethod @parameters
}
function Test-BootTarget {
    param([string]$Id)
    if($Id -notmatch '^[0-9A-Fa-f]{4}$') { return $false }
    $number=[Convert]::ToUInt16($Id,16);$raw=[Firmware]::Read("Boot$($Id.ToUpperInvariant())")
    if($null -eq $raw) { return $false }
    try { return -not [Firmware]::Parse($number,$raw).blocked } catch { return $false }
}
