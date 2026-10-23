# Real PowerShell installer flow; OS, EFI and network calls are in-memory fixtures.
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot
foreach($file in @('convert.ps1','install.ps1')){
 $parseErrors=$null;[Management.Automation.Language.Parser]::ParseFile((Join-Path $repo ('installer/windows/'+$file)),[ref]$null,[ref]$parseErrors)|Out-Null
 if($parseErrors){throw ($parseErrors|Out-String)}
}
$source=Get-Content (Join-Path $repo 'installer/windows/convert.ps1') -Raw
$entry=(Get-Content (Join-Path $repo 'installer/windows/install.ps1') -Raw).Replace('#Requires -RunAsAdministrator','').Replace("& (Join-Path `$PSScriptRoot 'convert.ps1') @PSBoundParameters",'& ([scriptblock]::Create($script:conversionCode)) @PSBoundParameters')
function Invoke-FakeAcl {$global:LASTEXITCODE=0}
function Get-Acl {return @{fixture='private'}}
function Set-Acl {}
function Get-ScheduledTask {return [pscustomobject]@{State=$(if($script:scenario -eq 'inactive' -or $script:stopped){'Ready'}else{'Running'})}}
function Stop-ScheduledTask {$script:stopped=$true}
function Start-ScheduledTask {$script:started=$true;$script:stopped=$false}
function Get-BootCatalog {param([switch]$FullScan) if(-not $FullScan){return @{systems=@()}};return @{systems=@(@{id='0002';name='Remote Boot iPXE'})}}
function Get-Partition {return [pscustomobject]@{GptType='{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}';DiskNumber=0;PartitionNumber=1;Size=4096;Offset=512;Guid='aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaaa'}}
function Get-Disk {return @{LogicalSectorSize=512}}
function Add-PartitionAccessPath {}
function Remove-PartitionAccessPath {}
function Move-Item {
 param([string]$LiteralPath,[string]$Destination,[switch]$Force)
 if($script:scenario -eq 'efi-fail' -and $LiteralPath -like '*ipxe.efi.new'){throw 'Fixture EFI write failure'}
 Microsoft.PowerShell.Management\Move-Item -LiteralPath $LiteralPath -Destination $Destination -Force:$Force
}
function Read-Host {
    param($Prompt)
    if($Prompt -like '*TRUST*'){if($script:scenario -eq 'cancel-trust'){return 'NO'};return 'TRUST'}
    if($Prompt -like '*VERIFIED*'){return 'VERIFIED'}
    if($Prompt -like '*INSTALL*'){if($script:scenario -eq 'cancel-install'){return 'NO'};return 'INSTALL'}
    if($Prompt -like '*REUSE*'){return 'REUSE'}
    if($Prompt -like '*RESTORE*'){return 'RESTORE'}
    if($Prompt -like '*disk number*'){return '0'}
    if($Prompt -like '*partition number*'){return '1'}
    return ''
}
function Invoke-FakeAgent {
    $global:LASTEXITCODE=0
    if($args -contains '--pair'){throw 'Conversion must never pair'}
    if($args -contains '--apply-profile'){
        $script:applied=$true
        if($script:scenario -eq 'apply-fail'){$global:LASTEXITCODE=4;return}
        $p=Get-Content $args[[array]::IndexOf($args,'--apply-profile')+1] -Raw|ConvertFrom-Json
        $path=$args[[array]::IndexOf($args,'--config')+1]
        $c=Get-Content $path -Raw|ConvertFrom-Json
        foreach($k in @('blind_mode','mode_revision','device_id','public_key','fingerprint','discovery_version')){$c|Add-Member -NotePropertyName $k -NotePropertyValue $p.$k -Force}
        $c.url='http://'+$p.last_ipv4;$c|ConvertTo-Json|Set-Content $path
    }
    if($args -contains '--report-preparation'){$script:reported=$true;if($script:scenario -eq 'stale'){$global:LASTEXITCODE=5}}
}
Add-Type @'
using System;
public static class FixtureFirmware {
 public static byte[] Order=new byte[]{1,0,2,0};
 public static byte[] Read(string name) {return name=="BootOrder"?Order:name=="Boot0002"?new byte[]{42}:null;}
 public static byte[] NewOption(uint p,ulong start,ulong size,Guid g) {return new byte[]{42};}
 public static void Write(string n,byte[] b) {throw new Exception("Unexpected firmware write: "+n);}
}
'@
$key='04'+'6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296'+'4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5'
$fingerprint=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Convert]::FromHexString($key))).ToLowerInvariant()
foreach($scenario in @('success','normal','rerun','inactive','wrong-pc','wrong-mode','wrong-revision','wrong-device','wrong-pin','bad-hash','cancel-trust','cancel-install','apply-fail','efi-fail','stale')) {
 $box=Join-Path ([IO.Path]::GetTempPath()) ('rb-convert-'+[guid]::NewGuid().ToString('N'))
 New-Item -ItemType Directory $box|Out-Null
 try {
  $script:scenario=$scenario;$script:stopped=$false;$script:started=$false;$script:applied=$false;$script:reported=$false
  $base=Join-Path $box 'RemoteBoot';New-Item -ItemType Directory $base|Out-Null
  $config=Join-Path $base 'agent.json';$binary=Join-Path $base 'remote-boot-agent.exe'
  $old='{"protocol":2,"pc_id":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","agent_id":"cccccccccccccccccccccccccccccccc","token":"fixture-not-real","url":"http://192.168.1.20","allow_reboot":true,"allow_shutdown":false}'
  if($scenario -eq 'normal'){$doc=$old|ConvertFrom-Json;$doc|Add-Member -NotePropertyName blind_mode -NotePropertyValue $true;$doc|Add-Member -NotePropertyName mode_revision -NotePropertyValue 2;$doc|Add-Member -NotePropertyName device_id -NotePropertyValue ('b'*32);$doc|Add-Member -NotePropertyName public_key -NotePropertyValue $key;$doc|Add-Member -NotePropertyName fingerprint -NotePropertyValue $fingerprint;$old=$doc|ConvertTo-Json}
  if($scenario -eq 'wrong-pin'){$doc=$old|ConvertFrom-Json;$doc|Add-Member -NotePropertyName device_id -NotePropertyValue ('e'*32);$old=$doc|ConvertTo-Json}
  [IO.File]::WriteAllText($config,$old);[IO.File]::WriteAllText($binary,'old binary')
  $candidate=Join-Path $box 'new.exe';Set-Content $candidate 'new binary'
  $profile=@{profile_version=1;protocol=2;discovery_version=1;pc_id=('a'*32);blind_mode=$true;mode_revision=2;device_id=('b'*32);public_key=$key;fingerprint=$fingerprint;last_ipv4='192.168.1.30';http_port=80;ws_port=81;hostname='remote-boot-bbbbbbbbbbbb.local'}
  if($scenario -eq 'normal'){$profile.blind_mode=$false;$profile.mode_revision=3}
  if($scenario -eq 'wrong-pc'){$profile.pc_id='d'*32}
  $profilePath=Join-Path $box 'profile.json';$profile|ConvertTo-Json|Set-Content $profilePath
  # Minimal PE32+ EFI fixture checked by the real installer.
  $image=New-Object byte[] 256;$image[0]=77;$image[1]=90;$image[60]=64;$image[64]=80;$image[65]=69;$image[68]=100;$image[69]=134;$image[88]=11;$image[89]=2;$image[156]=10
  $ipxe=Join-Path $box 'ipxe.efi';[IO.File]::WriteAllBytes($ipxe,$image)
  $loader=Join-Path $box 'RemoteBoot.efi';[IO.File]::WriteAllBytes($loader,$image)
  $hash=(Get-FileHash $ipxe -Algorithm SHA256).Hash.ToLowerInvariant()
  $manifest=@{};foreach($k in @('profile_version','blind_mode','mode_revision','device_id','public_key','fingerprint','discovery_version')){$manifest[$k]=$profile[$k]}
  $manifest.esp_ipv4=$profile.last_ipv4;$manifest.pc_id=$profile.pc_id;$manifest.ipxe_sha256=$hash;$manifest.loader_sha256=$hash
  if($scenario -eq 'wrong-mode'){$manifest.blind_mode=$false};if($scenario -eq 'wrong-revision'){$manifest.mode_revision=55};if($scenario -eq 'wrong-device'){$manifest.device_id='e'*32};if($scenario -eq 'bad-hash'){$manifest.ipxe_sha256='0'*64}
  $manifestPath=Join-Path $box 'manifest.json';$manifest|ConvertTo-Json|Set-Content $manifestPath
  $script:fixtureAccess=Join-Path $box 'esp';$efi=Join-Path $script:fixtureAccess 'EFI/iPXE';New-Item -ItemType Directory $efi -Force|Out-Null
  [IO.File]::WriteAllText((Join-Path $efi 'ipxe.efi'),'old EFI');[IO.File]::WriteAllText((Join-Path $efi 'manifest.json'),'old manifest')
  $code=$source.Replace('#Requires -RunAsAdministrator','').Replace(". (Join-Path `$root 'agent\windows\Common.ps1')",'').Replace('[Firmware]','[FixtureFirmware]')
  $code=$code.Replace('$root=Split-Path (Split-Path $PSScriptRoot)',('$root='''+$repo+''''))
  $code=$code.Replace('Join-Path $env:ProgramData ''RemoteBoot''',(''''+$base+''''))
  $code=$code.Replace('& icacls.exe','Invoke-FakeAcl').Replace('& $runner @Arguments','Invoke-FakeAgent @Arguments')
  $code=$code.Replace('$access="${letter}:\"','$access=$script:fixtureAccess')
  $script:conversionCode=$code
  $failed=$false
  try {& ([scriptblock]::Create($entry)) -DiscoveryProfile $profilePath -IpxeFile $ipxe -AgentConfig $config -AgentFile $candidate -ManifestFile $manifestPath -LoaderFile $loader} catch {$failed=$true;Write-Host "Expected/observed $scenario error: $_"}
  $safe=$scenario -like 'wrong-*' -or $scenario -in @('bad-hash','cancel-trust','cancel-install')
  if($safe){
   if([IO.File]::ReadAllText($config) -cne $old -or [IO.File]::ReadAllText($binary) -cne 'old binary' -or [IO.File]::ReadAllText((Join-Path $efi 'ipxe.efi')) -cne 'old EFI' -or $script:applied -or $script:stopped){throw "Unsafe pre-confirmation change: $scenario"}
   if($failed -ne ($scenario -notlike 'cancel-*')){throw "Wrong validation result $scenario"}
  } else {
   if($failed -ne ($scenario -in @('apply-fail','efi-fail','stale'))){throw "Unexpected outcome $scenario"}
   $backup=(Get-ChildItem $base -Directory -Filter 'backup-*'|Select-Object -First 1).FullName
   if(-not $backup -or [IO.File]::ReadAllText((Join-Path $backup 'agent.json')) -cne $old){throw 'No credential backup'}
   if($scenario -notin @('apply-fail','efi-fail')){
    $c=Get-Content $config -Raw|ConvertFrom-Json
    if($c.token -cne 'fixture-not-real' -or $c.agent_id -cne ('c'*32) -or -not $c.allow_reboot -or $c.allow_shutdown -or $c.blind_mode -ne $profile.blind_mode -or -not $script:reported){throw 'Credentials, permission or report mismatch'}
    if((Get-FileHash (Join-Path $efi 'ipxe.efi')).Hash -ine $hash){throw 'EFI was not installed'}
   }
   if($scenario -eq 'rerun'){& ([scriptblock]::Create($entry)) -DiscoveryProfile $profilePath -IpxeFile $ipxe -AgentConfig $config -AgentFile $candidate -ManifestFile $manifestPath -LoaderFile $loader}
   if($scenario -eq 'inactive' -and ($script:started -or $script:stopped)){throw 'Inactive task was changed'}
   if($scenario -eq 'success'){
    $statePath=Join-Path $backup 'state.json';$saved=Get-Content $statePath -Raw;$state=$saved|ConvertFrom-Json;$state.partition_guid='ffffffff-ffff-ffff-ffff-ffffffffffff';$state|ConvertTo-Json|Set-Content $statePath
    $rejected=$false;try {& ([scriptblock]::Create($entry)) -Rollback $backup} catch {$rejected=$true}
    if(-not $rejected -or (Get-FileHash (Join-Path $efi 'ipxe.efi')).Hash -ine $hash){throw 'Rollback wrote to wrong partition'}
    Set-Content $statePath $saved
   }
   & ([scriptblock]::Create($entry)) -Rollback $backup
   if([IO.File]::ReadAllText($config) -cne $old -or [IO.File]::ReadAllText($binary) -cne 'old binary' -or [IO.File]::ReadAllText((Join-Path $efi 'ipxe.efi')) -cne 'old EFI' -or [IO.File]::ReadAllText((Join-Path $efi 'manifest.json')) -cne 'old manifest'){throw 'Rollback did not restore original files'}
  }
  Write-Host "PASS Windows conversion: $scenario"
 } finally {Remove-Item $box -Recurse -Force}
}
