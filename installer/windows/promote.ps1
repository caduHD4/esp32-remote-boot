#Requires -RunAsAdministrator
param([Parameter(Mandatory)][ValidatePattern('^[0-9A-Fa-f]{4}$')][string]$BootId)
$root=Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $root 'agent\windows\Common.ps1')
$id=[Convert]::ToUInt16($BootId,16)
$entry=[Firmware]::Parse($id,[Firmware]::Read("Boot$($BootId.ToUpperInvariant())"))
if($entry.name -ne 'Remote Boot iPXE'){throw 'Not a Remote Boot entry'}
if((Read-Host 'After a successful BootNext test, type PROMOTE to put Remote Boot first') -ne 'PROMOTE'){return}
$old=[Firmware]::Read('BootOrder');$backup=Join-Path $env:ProgramData ('RemoteBoot\BootOrder-before-promote-'+[datetime]::UtcNow.Ticks+'.bin')
[IO.File]::WriteAllBytes($backup,$old)
$order=@($id)+@([Firmware]::Order()|Where-Object {$_ -ne $id})
$bytes=[byte[]]::new($order.Count*2)
for($i=0;$i -lt $order.Count;$i++){[Array]::Copy([BitConverter]::GetBytes([uint16]$order[$i]),0,$bytes,2*$i,2)}
[Firmware]::Write('BootOrder',$bytes)
