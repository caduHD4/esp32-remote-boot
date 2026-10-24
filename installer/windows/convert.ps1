#Requires -RunAsAdministrator
param([string]$EspAddress,[string]$IpxeFile,[string]$AgentConfig,[string]$ManifestFile,[string]$LoaderFile,[switch]$FullScan,[string]$DiscoveryProfile,[string]$AgentFile,[string]$Rollback)
$ErrorActionPreference='Stop'
$root=Split-Path (Split-Path $PSScriptRoot)
. (Join-Path $root 'agent\windows\Common.ps1')
$base=Join-Path $env:ProgramData 'RemoteBoot'
if(-not $AgentConfig){$AgentConfig=Join-Path $base 'agent.json'}
$binary=Join-Path $base 'remote-boot-agent.exe'
function Get-RbField($Document,[string]$Name) {
    $property=$Document.PSObject.Properties[$Name]
    if($null -eq $property){return $null};return $property.Value
}
function Invoke-RbAgent {
    param([string]$runner,[string[]]$Arguments)
    & $runner @Arguments
    if($LASTEXITCODE -ne 0){throw "Agent command $($Arguments[0]) failed (exit $LASTEXITCODE). Older agents need -AgentFile with a current native binary; do not re-pair."}
}
function Stop-RbTask {
    Stop-ScheduledTask -TaskName RemoteBootAgent
    $deadline=[datetime]::UtcNow.AddSeconds(10)
    while((Get-ScheduledTask -TaskName RemoteBootAgent).State -eq 'Running'){
        if([datetime]::UtcNow -ge $deadline){throw 'Agent task did not stop; refusing file replacement'}
        Start-Sleep -Milliseconds 100
    }
}
function Convert-RbHex([string]$Hex) {
    $bytes=New-Object byte[] ($Hex.Length/2)
    for($i=0;$i -lt $bytes.Length;$i++){$bytes[$i]=[Convert]::ToByte($Hex.Substring(2*$i,2),16)}
    return ,$bytes
}
function Read-RbProfile([string]$Path) {
    if((Get-Item -LiteralPath $Path).Length -gt 16384){throw 'Public profile exceeds 16 KiB'}
    $p=Get-Content -LiteralPath $Path -Raw|ConvertFrom-Json
    $required=@('profile_version','protocol','discovery_version','pc_id','blind_mode','mode_revision','device_id','public_key','fingerprint','last_ipv4','http_port','ws_port','hostname')
    $names=@($p.PSObject.Properties.Name)
    if($names.Count -ne $required.Count -or @($names|Where-Object {$_ -cnotin $required}).Count){throw 'Public profile must contain exactly the documented fields (no secrets).'}
    foreach($name in @('profile_version','protocol','discovery_version','mode_revision','http_port','ws_port')){
        if($p.$name -isnot [int] -and $p.$name -isnot [long] -and $p.$name -isnot [uint32]){throw "Invalid integer profile field: $name"}
    }
    if($p.profile_version -ne 1 -or $p.protocol -ne 2 -or $p.discovery_version -ne 1 -or $p.http_port -ne 80 -or $p.ws_port -ne 81 -or $p.mode_revision -lt 1 -or $p.mode_revision -gt [uint32]::MaxValue -or $p.blind_mode -isnot [bool]){throw 'Invalid public profile version, mode, revision or ports.'}
    foreach($item in @(@('pc_id',32),@('device_id',32),@('public_key',130),@('fingerprint',64))){
        if($p.($item[0]) -isnot [string] -or $p.($item[0]) -cnotmatch ('^[0-9a-f]{'+$item[1]+'}$')){throw "Invalid public profile field: $($item[0])"}
    }
    if($p.device_id -eq ('0'*32) -or -not $p.public_key.StartsWith('04')){throw 'Invalid bound device ID or uncompressed public key'}
    $point=Convert-RbHex $p.public_key
    $hash=[Security.Cryptography.SHA256]::Create()
    try {$fingerprint=([BitConverter]::ToString($hash.ComputeHash($point))).Replace('-','').ToLowerInvariant()} finally {$hash.Dispose()}
    if($fingerprint -cne $p.fingerprint){throw 'Public key fingerprint mismatch'}
    $parameters=New-Object Security.Cryptography.ECParameters
    $parameters.Curve=[Security.Cryptography.ECCurve]::CreateFromFriendlyName("nistP256")
    $q=New-Object Security.Cryptography.ECPoint;$q.X=[byte[]]$point[1..32];$q.Y=[byte[]]$point[33..64];$parameters.Q=$q
    $key=[Security.Cryptography.ECDsa]::Create()
    try {$key.ImportParameters($parameters)} finally {$key.Dispose()}
    $ip=$null
    if($p.last_ipv4 -isnot [string] -or -not [Net.IPAddress]::TryParse($p.last_ipv4,[ref]$ip) -or $ip.AddressFamily -ne [Net.Sockets.AddressFamily]::InterNetwork -or $ip.ToString() -cne $p.last_ipv4 -or $ip.GetAddressBytes()[0] -eq 0 -or $ip.GetAddressBytes()[0] -eq 127 -or $ip.GetAddressBytes()[0] -ge 224){throw 'Profile requires a usable canonical IPv4'}
    if($p.hostname -cne ('remote-boot-'+$p.device_id.Substring(20)+'.local')){throw 'Profile hostname does not match bound identity'}
    return $p
}
function Protect-RbBackup([string]$Path) {
    & icacls.exe $Path /inheritance:r /grant:r '*S-1-5-18:(OI)(CI)F' '*S-1-5-32-544:(OI)(CI)F'|Out-Null
    if($LASTEXITCODE -ne 0){throw 'Cannot protect backup directory'}
}
function Assert-RbSafePath([string]$Path) {
    if(-not [IO.Path]::IsPathRooted($Path) -or '..' -in ($Path -split '[\\/]')){throw 'Recovery paths must be absolute without parent traversal'}
    $current=[IO.Path]::GetFullPath($Path)
    while($current){
        $entry=Get-Item -LiteralPath $current -Force -ErrorAction SilentlyContinue
        if($null -ne $entry -and ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint)){throw "Unsafe reparse destination or ancestor: $current"}
        $parent=[IO.Directory]::GetParent($current)
        $current=if($null -eq $parent){$null}else{$parent.FullName}
    }
}
function Write-RbSnapshotSeal([string]$Backup) {
    $inventory=@{}
    foreach($name in @('state.json','agent.json','agent.bin','config-acl.xml','binary-acl.xml','ipxe.efi','manifest.json')){
        $path=Join-Path $Backup $name;Assert-RbSafePath $path
        $inventory[$name]=if(Test-Path -LiteralPath $path -PathType Leaf){(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}else{$null}
    }
    $inventory|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $Backup 'integrity.json')
}
function Test-RbSnapshot([string]$Backup) {
    Assert-RbSafePath $Backup
    $seal=Join-Path $Backup 'integrity.json';Assert-RbSafePath $seal
    $inventory=Get-Content -LiteralPath $seal -Raw|ConvertFrom-Json
    $names=@('state.json','agent.json','agent.bin','config-acl.xml','binary-acl.xml','ipxe.efi','manifest.json')
    if(@($inventory.PSObject.Properties.Name).Count -ne $names.Count -or @($inventory.PSObject.Properties.Name|Where-Object {$_ -cnotin $names}).Count){throw 'Incomplete backup integrity inventory'}
    foreach($required in @('state.json','agent.json','agent.bin','config-acl.xml','binary-acl.xml')){if(-not (Get-RbField $inventory $required)){throw "Required original missing from inventory: $required"}}
    foreach($name in $names){
        $path=Join-Path $Backup $name;Assert-RbSafePath $path;$expected=Get-RbField $inventory $name
        if($expected){if(-not (Test-Path -LiteralPath $path -PathType Leaf) -or (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne $expected){throw "Backup integrity mismatch: $name"}}
        elseif(Test-Path -LiteralPath $path){throw "Backup presence mismatch: $name"}
    }
}
function Copy-RbRecoveryFile([string]$Source,[string]$Destination,$Acl=$null) {
    Assert-RbSafePath $Source;Assert-RbSafePath $Destination
    $stage=Join-Path (Split-Path $Destination -Parent) ('.rb-restore-'+[guid]::NewGuid().ToString('N'))
    # CreateNew fails rather than following an existing staging file/reparse point.
    try {
        $output=[IO.File]::Open($stage,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
        try {
            $input=[IO.File]::OpenRead($Source)
            try {$input.CopyTo($output);$output.Flush($true)} finally {$input.Dispose()}
        } finally {$output.Dispose()}
        if($null -ne $Acl){Set-Acl -LiteralPath $stage -AclObject $Acl}
        Assert-RbSafePath $Destination
        Move-Item -LiteralPath $stage -Destination $Destination -Force
    } finally {if(Test-Path -LiteralPath $stage){Remove-Item -LiteralPath $stage -Force}}

}
function Restore-RbConfigAcl {
    if($null -ne $script:originalConfigAcl){Assert-RbSafePath $AgentConfig;Set-Acl -LiteralPath $AgentConfig -AclObject $script:originalConfigAcl}
}
function Import-RbAcl([string]$Path) {
    $saved=Import-Clixml -LiteralPath $Path
    if($saved -is [Security.AccessControl.FileSystemSecurity]){return $saved}
    # CLIXML may deserialize FileSecurity as a property bag; rebuild its descriptor.
    $acl=New-Object Security.AccessControl.FileSecurity
    $acl.SetSecurityDescriptorSddlForm([string]$saved.Sddl)
    return $acl
}
function Restore-RbFiles([string]$Backup) {
    Test-RbSnapshot $Backup
    $state=Get-Content -LiteralPath (Join-Path $Backup 'state.json') -Raw|ConvertFrom-Json
    # Validate every destination and deserialize every ACL before restoring any file.
    foreach($path in @($state.config,$state.binary,$script:restoreTarget,(Join-Path $script:restoreTarget 'ipxe.efi'),(Join-Path $script:restoreTarget 'manifest.json'))){Assert-RbSafePath $path}
    $configAcl=Import-RbAcl (Join-Path $Backup 'config-acl.xml')
    $binaryAcl=Import-RbAcl (Join-Path $Backup 'binary-acl.xml')
    Copy-RbRecoveryFile (Join-Path $Backup 'agent.json') $state.config $configAcl
    Copy-RbRecoveryFile (Join-Path $Backup 'agent.bin') $state.binary $binaryAcl
    foreach($name in @('ipxe.efi','manifest.json')){
        $destination=Join-Path $script:restoreTarget $name;$original=Join-Path $Backup $name
        if(Test-Path -LiteralPath $original){Copy-RbRecoveryFile $original $destination}
        elseif(Test-Path -LiteralPath $destination){Remove-Item -LiteralPath $destination -Force}
    }
    $created=Get-RbField $state 'created_id'
    if($created){
        $entry=[Firmware]::Read("Boot$created");$order=[Firmware]::Read('BootOrder');$next=[Firmware]::Read('BootNext')
        $referenced=$false;for($i=0;$i -lt $order.Length;$i+=2){if([BitConverter]::ToUInt16($order,$i) -eq [Convert]::ToUInt16($created,16)){$referenced=$true}}
        if($null -ne $next -and $next.Length -eq 2 -and [BitConverter]::ToUInt16($next,0) -eq [Convert]::ToUInt16($created,16)){$referenced=$true}
        if(-not $referenced -and [Convert]::ToBase64String($entry) -ceq $state.created_bytes){[Firmware]::Write("Boot$created",[byte[]]@())}
        else {Write-Warning "Boot$created changed or is referenced by BootOrder/BootNext; left untouched. Resolve manually."}
    }
}
# Mount exactly the selected GPT ESP, always unmounting our temporary access path.
function Invoke-RbMounted([int]$DiskNumber,[int]$PartitionNumber,[scriptblock]$Action) {
    $letters=@('R','S','T','U','V','W','X','Y','Z');$letter=$letters|Where-Object {-not (Get-PSDrive -Name $_ -ErrorAction SilentlyContinue)}|Select-Object -First 1
    if(-not $letter){throw 'No free drive letter'}
    $access="${letter}:\"
    Add-PartitionAccessPath -DiskNumber $DiskNumber -PartitionNumber $PartitionNumber -AccessPath $access
    try {& $Action $access} finally {Remove-PartitionAccessPath -DiskNumber $DiskNumber -PartitionNumber $PartitionNumber -AccessPath $access}
}
if($Rollback){
    Test-RbSnapshot $Rollback
    $state=Get-Content -LiteralPath (Join-Path $Rollback 'state.json') -Raw|ConvertFrom-Json
    $partition=Get-Partition|Where-Object {$_.DiskNumber -eq $state.disk -and $_.PartitionNumber -eq $state.partition -and [string]$_.Guid -eq $state.partition_guid -and $_.GptType -eq '{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}'}
    if($null -eq $partition){throw 'Original GPT ESP identity unavailable; refusing rollback.'}
    if(-not (Get-RbField $state 'snapshot_complete')){throw 'Incomplete backup; no conversion writes were started. Retain for inspection and rerun.'}
    Write-Host "Restore only this OS agent and selected EFI files from $Rollback. BootOrder/BootNext remain untouched."
    if((Read-Host 'Type RESTORE to roll back') -cne 'RESTORE'){Write-Host 'Rollback cancelled.';return}
    $task=Get-ScheduledTask -TaskName RemoteBootAgent -ErrorAction SilentlyContinue;$running=$null -ne $task -and $task.State -eq 'Running'
    try {
        if($running){Stop-RbTask}
        Invoke-RbMounted $state.disk $state.partition {param($access) $script:restoreTarget=Join-Path $access 'EFI\iPXE';Restore-RbFiles $Rollback}
    } finally {if($running){Start-ScheduledTask -TaskName RemoteBootAgent}}
    Write-Host 'Local files restored. Server preparation may now be stale; set desired mode and rerun conversion to report it. No physical boot tested.'
    return
}
if(-not $DiscoveryProfile -or -not $IpxeFile){throw 'Assisted conversion requires -DiscoveryProfile and -IpxeFile. Agent upgrades also require a public profile; do not re-pair.'}
$profile=Read-RbProfile $DiscoveryProfile
Write-Host "ESP fingerprint: $($profile.fingerprint)"
Write-Host "PC $($profile.pc_id); desired blind_mode=$($profile.blind_mode); mode revision=$($profile.mode_revision); normal IPv4=$($profile.last_ipv4)"
Write-Host 'Compare the fingerprint with a previously trusted source or physical USB. Convert each OS agent separately in dual boot.'
if((Read-Host 'Type TRUST to confirm fingerprint and desired mode') -cne 'TRUST'){Write-Host 'Conversion cancelled; no files changed.';return}
Assert-RbSafePath $AgentConfig;Assert-RbSafePath $binary
if(-not (Test-Path -LiteralPath $AgentConfig -PathType Leaf) -or -not (Test-Path -LiteralPath $binary -PathType Leaf)){throw 'Existing paired agent config and installed binary are required.'}
$identity=Get-Content -LiteralPath $AgentConfig -Raw|ConvertFrom-Json
if($identity.protocol -ne 2 -or $identity.pc_id -cne $profile.pc_id){throw 'Profile belongs to another paired PC'}
foreach($name in @('device_id','public_key','fingerprint')){$pin=Get-RbField $identity $name;if($pin -and $pin -cne $profile.$name){throw 'Profile differs from pinned ESP identity'}}
if($EspAddress -and $EspAddress -cne $profile.last_ipv4){throw 'Requested ESP IPv4 differs from confirmed profile'}
$EspAddress=$profile.last_ipv4
$runner=if($AgentFile){$AgentFile}else{$binary}
Invoke-RbAgent $runner @('--self-test')
$IpxeFile=(Resolve-Path -LiteralPath $IpxeFile).Path
if(-not $ManifestFile){$ManifestFile=Join-Path (Split-Path $IpxeFile) 'manifest.json'}
if(-not $LoaderFile){$LoaderFile=Join-Path (Split-Path $IpxeFile) 'RemoteBoot.efi'}
$manifest=Get-Content -LiteralPath $ManifestFile -Raw|ConvertFrom-Json
foreach($name in @('profile_version','blind_mode','mode_revision','device_id','public_key','fingerprint','discovery_version')){
    $value=Get-RbField $manifest $name
    if($null -eq $value -or $value.GetType() -ne $profile.$name.GetType() -or $value -cne $profile.$name){throw "Manifest differs from profile: $name"}
}
if($manifest.pc_id -cne $profile.pc_id -or $manifest.esp_ipv4 -cne $EspAddress -or $manifest.ipxe_sha256 -cnotmatch '^[0-9a-f]{64}$' -or $manifest.loader_sha256 -cnotmatch '^[0-9a-f]{64}$' -or (Get-FileHash -LiteralPath $IpxeFile -Algorithm SHA256).Hash -ine $manifest.ipxe_sha256 -or (Get-FileHash -LiteralPath $LoaderFile -Algorithm SHA256).Hash -ine $manifest.loader_sha256){throw 'Manifest PC/ESP or actual image hash mismatch'}
$image=[IO.File]::ReadAllBytes($IpxeFile)
if($image.Length -lt 256 -or $image[0] -ne 77 -or $image[1] -ne 90){throw 'Invalid PE image'}
$pe=[BitConverter]::ToInt32($image,60)
if($pe -lt 0 -or $pe+94 -gt $image.Length -or [BitConverter]::ToUInt32($image,$pe) -ne 0x4550 -or [BitConverter]::ToUInt16($image,$pe+4) -ne 0x8664 -or [BitConverter]::ToUInt16($image,$pe+24) -ne 0x20b -or [BitConverter]::ToUInt16($image,$pe+92) -ne 10){throw 'Expected EFI application PE32+ x86_64'}
# Remote Boot create-only entries may deliberately be absent from BootOrder.
$catalog=Get-BootCatalog -FullScan
if($catalog.systems.Count -gt 24){throw 'Catalog exceeds 24 entries'}
Write-Host "Verified iPXE SHA256 $($manifest.ipxe_sha256); loader SHA256 $($manifest.loader_sha256). Build in Linux/WSL; install on native Windows."
if((Read-Host 'Type VERIFIED after checking builder output and hashes') -cne 'VERIFIED'){Write-Host 'Conversion cancelled; no files changed.';return}
$partitions=@(Get-Partition|Where-Object GptType -eq '{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}')
$partitions|Format-Table DiskNumber,PartitionNumber,Guid,Size
$diskNumber=[int](Read-Host 'ESP disk number');$partitionNumber=[int](Read-Host 'ESP partition number')
$partition=$partitions|Where-Object {$_.DiskNumber -eq $diskNumber -and $_.PartitionNumber -eq $partitionNumber}
if($null -eq $partition){throw 'Selected partition is not a GPT ESP'}
$disk=Get-Disk -Number $diskNumber
$entryBytes=[Firmware]::NewOption([uint32]$partitionNumber,[uint64]($partition.Offset/$disk.LogicalSectorSize),[uint64]($partition.Size/$disk.LogicalSectorSize),[guid]$partition.Guid)
$existing=@($catalog.systems|Where-Object name -eq 'Remote Boot iPXE')
if($existing.Count -gt 1){throw 'Multiple Remote Boot entries; resolve duplicates explicitly first'}
$id=$null
if($existing.Count -eq 1){
    $id=$existing[0].id
    if([Convert]::ToBase64String([Firmware]::Read("Boot$id")) -cne [Convert]::ToBase64String($entryBytes)){throw 'Existing Remote Boot entry targets a different ESP; refusing replacement'}
}
Write-Host "Will update agent $AgentConfig and disk $diskNumber partition $partitionNumber EFI\iPXE."
if((Read-Host 'Type INSTALL to write agent configuration and EFI') -cne 'INSTALL'){Write-Host 'Conversion cancelled; no files changed.';return}
if($id -and (Read-Host 'Type REUSE to preserve existing EFI entry') -cne 'REUSE'){Write-Host 'Conversion cancelled; no files changed.';return}
$backup=Join-Path $base ('backup-'+[datetime]::UtcNow.ToString('yyyyMMddTHHmmssfff')+'-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $backup -Force|Out-Null
Protect-RbBackup $backup
Copy-Item -LiteralPath $AgentConfig -Destination (Join-Path $backup 'agent.json')
Copy-Item -LiteralPath $binary -Destination (Join-Path $backup 'agent.bin')
$script:originalConfigAcl=Get-Acl -LiteralPath $AgentConfig
$script:originalConfigAcl|Export-Clixml -LiteralPath (Join-Path $backup 'config-acl.xml')
Get-Acl -LiteralPath $binary|Export-Clixml -LiteralPath (Join-Path $backup 'binary-acl.xml')
Copy-Item -LiteralPath $DiscoveryProfile -Destination (Join-Path $backup 'profile.json')
Copy-Item -LiteralPath $ManifestFile -Destination (Join-Path $backup 'new-manifest.json')
$state=@{config=$AgentConfig;binary=$binary;disk=$diskNumber;partition=$partitionNumber;partition_guid=[string]$partition.Guid}
$state|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $backup 'state.json')
$oldOrder=[Firmware]::Read('BootOrder');[IO.File]::WriteAllBytes((Join-Path $backup 'BootOrder.bin'),$oldOrder)
$task=Get-ScheduledTask -TaskName RemoteBootAgent -ErrorAction SilentlyContinue;$running=$null -ne $task -and $task.State -eq 'Running'
$script:phase='backup EFI files';$script:taskStopped=$false
Write-Host "Backup: $backup"
try {
    Invoke-RbMounted $diskNumber $partitionNumber {
        param($access)
        $target=Join-Path $access 'EFI\iPXE';Assert-RbSafePath $target;New-Item -ItemType Directory -Path $target -Force|Out-Null
        foreach($name in @('ipxe.efi','manifest.json')){if(Test-Path -LiteralPath (Join-Path $target $name)){Copy-Item -LiteralPath (Join-Path $target $name) -Destination (Join-Path $backup $name)}}
        $state.snapshot_complete=$true;$state|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $backup 'state.json');Write-RbSnapshotSeal $backup
        if($running){$script:taskStopped=$true;Stop-RbTask}
        $script:phase='agent configuration'
        try {Invoke-RbAgent $runner @('--apply-profile',$DiscoveryProfile,'--config',$AgentConfig,'--confirm-fingerprint',$profile.fingerprint)} finally {Restore-RbConfigAcl}
        if($AgentFile){
            $script:phase='agent binary upgrade';$acl=Get-Acl -LiteralPath $binary
            Copy-Item -LiteralPath $AgentFile -Destination "$binary.new" -Force;Set-Acl -LiteralPath "$binary.new" -AclObject $acl
            Move-Item -LiteralPath "$binary.new" -Destination $binary -Force
        }
        $script:phase='EFI install'
        $destination=Join-Path $target 'ipxe.efi';$manifestTarget=Join-Path $target 'manifest.json'
        Copy-Item -LiteralPath $IpxeFile -Destination "$destination.new" -Force;Copy-Item -LiteralPath $ManifestFile -Destination "$manifestTarget.new" -Force
        Move-Item -LiteralPath "$destination.new" -Destination $destination -Force;Move-Item -LiteralPath "$manifestTarget.new" -Destination $manifestTarget -Force
        if((Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash -ine $manifest.ipxe_sha256 -or (Get-FileHash -LiteralPath $manifestTarget -Algorithm SHA256).Hash -ine (Get-FileHash -LiteralPath $ManifestFile -Algorithm SHA256).Hash){throw 'Installed EFI or manifest hash mismatch'}
        if(-not $id){
            for($i=0;$i -le 65535;$i++){if($null -eq [Firmware]::Read(('Boot{0:X4}' -f $i))){$id=('{0:X4}' -f $i);break}}
            if(-not $id){throw 'No unused Boot#### entry'}
            $state.created_id=$id;$state.created_bytes=[Convert]::ToBase64String($entryBytes);$state|ConvertTo-Json|Set-Content -LiteralPath (Join-Path $backup 'state.json')
            Write-RbSnapshotSeal $backup
            [Firmware]::Write("Boot$id",$entryBytes)
        }
        if([Convert]::ToBase64String([Firmware]::Read('BootOrder')) -cne [Convert]::ToBase64String($oldOrder)){throw 'Unexpected BootOrder change; retained snapshot, no global restore attempted'}
        if($script:taskStopped){Start-ScheduledTask -TaskName RemoteBootAgent;$script:taskStopped=$false}
        $script:phase='server preparation report'
        try {Invoke-RbAgent $binary @('--report-preparation',$manifestTarget,'--config',$AgentConfig)} finally {Restore-RbConfigAcl}
        Write-Host 'Local preparation reported. This is not proof of physical boot; other OS agents may still need conversion.'
        $script:phase='optional BootNext test'
        if((Read-Host 'Type TEST to set BootNext for a one-time test') -ceq 'TEST'){[Firmware]::Write('BootNext',[BitConverter]::GetBytes([Convert]::ToUInt16($id,16)))}
        Write-Host "Installed Boot$id; BootOrder preserved. No reboot performed. Backup: $backup"
    }
} catch {
    throw "Conversion incomplete at phase $script:phase. Local preparation pending (server may reject stale mode/revision). Backup: $backup. Rerun with a fresh profile or explicitly restore using install.ps1 -Rollback '$backup'. Cause: $_"
} finally {try {Restore-RbConfigAcl} finally {if($script:taskStopped){Start-ScheduledTask -TaskName RemoteBootAgent}}}
