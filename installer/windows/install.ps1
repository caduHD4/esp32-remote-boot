#Requires -RunAsAdministrator
param([Parameter(Mandatory)][string]$EspAddress,[Parameter(Mandatory)][string]$IpxeFile,[switch]$FullScan)
$ErrorActionPreference='Stop'
$root=Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $root 'agent\windows\Common.ps1')
$address=$null
if(-not [Net.IPAddress]::TryParse($EspAddress,[ref]$address) -or $address.AddressFamily -ne [Net.Sockets.AddressFamily]::InterNetwork){throw 'Expected IPv4'}
$script:RBUrl="http://$EspAddress"
$secret=Read-Host 'Administrative token' -AsSecureString
$script:RBToken=[Net.NetworkCredential]::new('',$secret).Password
Invoke-RemoteBootApi GET status|Out-Null
$catalog=Get-BootCatalog -FullScan:$FullScan
if($catalog.systems.Count -gt 24){throw 'Catalog exceeds 24 entries. Remove obsolete firmware entries first.'}
$catalog.systems|Format-Table id,name,hidden,blocked
Invoke-RemoteBootApi POST systems/sync $catalog|Out-Null
$IpxeFile=(Resolve-Path -LiteralPath $IpxeFile).Path
$image=[IO.File]::ReadAllBytes($IpxeFile)
if($image.Length -lt 256 -or $image[0] -ne 77 -or $image[1] -ne 90){throw 'Invalid PE image'}
$pe=[BitConverter]::ToInt32($image,60)
if($pe -lt 0 -or $pe+94 -gt $image.Length -or [BitConverter]::ToUInt32($image,$pe) -ne 0x4550 -or [BitConverter]::ToUInt16($image,$pe+4) -ne 0x8664 -or [BitConverter]::ToUInt16($image,$pe+24) -ne 0x20b -or [BitConverter]::ToUInt16($image,$pe+92) -ne 10){throw 'Expected EFI application PE32+ x86_64'}
Get-FileHash -LiteralPath $IpxeFile -Algorithm SHA256|Format-List
Write-Host 'This image must have been built for the selected ESP address with RemoteBoot.efi embedded.'
if((Read-Host 'Type VERIFIED after checking the builder output and SHA256SUMS') -ne 'VERIFIED'){return}
$partitions=@(Get-Partition|Where-Object GptType -eq '{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}')
if(-not $partitions.Count){throw 'No GPT EFI System Partition'}
$partitions|Format-Table DiskNumber,PartitionNumber,Size,Guid
$diskNumber=[int](Read-Host 'ESP disk number');$partitionNumber=[int](Read-Host 'ESP partition number')
$partition=$partitions|Where-Object {$_.DiskNumber -eq $diskNumber -and $_.PartitionNumber -eq $partitionNumber}
if($null -eq $partition){throw 'Selected partition is not an ESP'}
$disk=Get-Disk -Number $diskNumber
$base=Join-Path $env:ProgramData 'RemoteBoot';$backup=Join-Path $base ('backup-'+[datetime]::UtcNow.ToString('yyyyMMddTHHmmssfff'))
New-Item -ItemType Directory -Path $backup -Force|Out-Null
& icacls.exe $base /inheritance:r /grant:r '*S-1-5-18:(OI)(CI)F' '*S-1-5-32-544:(OI)(CI)F'|Out-Null
if($LASTEXITCODE -ne 0){throw 'Cannot protect backup directory'}
$oldOrder=[Firmware]::Read('BootOrder');[IO.File]::WriteAllBytes((Join-Path $backup 'BootOrder.bin'),$oldOrder)
$oldNext=[Firmware]::Read('BootNext');if($null -ne $oldNext){[IO.File]::WriteAllBytes((Join-Path $backup 'BootNext.bin'),$oldNext)}
foreach($entry in $catalog.systems){[IO.File]::WriteAllBytes((Join-Path $backup ('Boot'+$entry.id+'.bin')),[Firmware]::Read('Boot'+$entry.id))}
$catalog|ConvertTo-Json -Depth 8|Set-Content (Join-Path $backup 'catalog.json')
& bcdedit.exe /export (Join-Path $backup 'BCD-backup')
if($LASTEXITCODE -ne 0){throw 'BCD export failed'}
if((Read-Host 'Type INSTALL to mount ESP, write EFI file, and create/reuse Remote Boot entry') -ne 'INSTALL'){return}
$letters=@('R','S','T','U','V','W','X','Y','Z');$letter=$letters|Where-Object {-not (Get-PSDrive -Name $_ -ErrorAction SilentlyContinue)}|Select-Object -First 1
if(-not $letter){throw 'No free drive letter'}
$access="${letter}:\"
Add-PartitionAccessPath -DiskNumber $diskNumber -PartitionNumber $partitionNumber -AccessPath $access
try {
    $target=Join-Path $access 'EFI\iPXE\ipxe.efi';$folder=Split-Path $target
    New-Item -ItemType Directory -Path $folder -Force|Out-Null
    if(Test-Path $target){Copy-Item $target (Join-Path $backup 'ipxe.efi')}
    $entryBytes=[Firmware]::NewOption([uint32]$partitionNumber,[uint64]($partition.Offset/$disk.LogicalSectorSize),[uint64]($partition.Size/$disk.LogicalSectorSize),[guid]$partition.Guid)
    $existing=@($catalog.systems|Where-Object name -eq 'Remote Boot iPXE')
    if($existing.Count -gt 1){throw 'Multiple Remote Boot entries. Resolve duplicates before installation.'}
    if($existing.Count -eq 1){
        $id=$existing[0].id
        $old=[Firmware]::Read("Boot$id")
        if([Convert]::ToBase64String($old) -ne [Convert]::ToBase64String($entryBytes)){throw 'Existing entry targets another ESP or has different data. Refusing replacement.'}
    } else {
        $id=$null
        for($i=0;$i -le 65535;$i++){if($null -eq [Firmware]::Read(('Boot{0:X4}' -f $i))){$id=('{0:X4}' -f $i);break}}
        if($null -eq $id){throw 'No unused Boot#### variable'}
    }
    Copy-Item -LiteralPath $IpxeFile -Destination "$target.new" -Force
    Move-Item -LiteralPath "$target.new" -Destination $target -Force
    if(-not $existing.Count){[Firmware]::Write("Boot$id",$entryBytes)}
    if([Convert]::ToBase64String([Firmware]::Read('BootOrder')) -ne [Convert]::ToBase64String($oldOrder)){throw 'Unexpected BootOrder change. See backup.'}
    Set-Content (Join-Path $backup 'RemoteBootId.txt') $id
    Invoke-RemoteBootApi POST systems/sync (Get-BootCatalog -FullScan)|Out-Null
    if((Read-Host 'Type TEST to set BootNext for one-time test') -eq 'TEST'){[Firmware]::Write('BootNext',[BitConverter]::GetBytes([Convert]::ToUInt16($id,16)))}
    Write-Host "Installed Boot$id. BootOrder preserved. Backup: $backup"
} finally { Remove-PartitionAccessPath -DiskNumber $diskNumber -PartitionNumber $partitionNumber -AccessPath $access }
Write-Host 'Run install-agent.ps1 in each OS needing heartbeat. After successful test, use promote.ps1.'
