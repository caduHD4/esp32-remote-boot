param([string]$ConfigPath="$env:ProgramData\RemoteBoot\agent.json")
. (Join-Path $PSScriptRoot 'Common.ps1')
$config=Get-Content -LiteralPath $ConfigPath -Raw|ConvertFrom-Json
$script:RBUrl=$config.url;$script:RBToken=$config.token
$generation=-1;$lastSync=[datetime]::MinValue;$ack=''
$ackPath=Join-Path (Split-Path $ConfigPath) 'ack.txt'
if(Test-Path $ackPath){$ack=Get-Content $ackPath -Raw}
while($true) {
    try {
        $os=Get-CimInstance Win32_OperatingSystem
        $current=[Firmware]::Read('BootCurrent');$bootId=''
        if($null -ne $current -and $current.Length -eq 2) { $candidate=('{0:X4}' -f [BitConverter]::ToUInt16($current,0));if(Test-BootTarget $candidate){$bootId=$candidate} }
        $payload=@{hostname=$env:COMPUTERNAME;os=$os.Caption;boot_id=$bootId;uptime=[int](([datetime]::Now-$os.LastBootUpTime).TotalSeconds);ack=$ack;reboot_enabled=$config.allow_reboot}
        $reply=Invoke-RemoteBootApi POST heartbeat $payload
        if($reply.discovery_generation -ne $generation -or ([datetime]::UtcNow-$lastSync).TotalMinutes -ge 5) {
            Invoke-RemoteBootApi POST systems/sync (Get-BootCatalog) | Out-Null
            $generation=$reply.discovery_generation;$lastSync=[datetime]::UtcNow
        }
        if($reply.PSObject.Properties.Name -contains 'command') {
            $command=$reply.command
            if($config.allow_reboot -and $command.action -eq 'reboot' -and $command.id -ne $ack -and (Test-BootTarget $command.boot_id)) {
                [Firmware]::Write('BootNext',[BitConverter]::GetBytes([Convert]::ToUInt16($command.boot_id,16)))
                $ack=$command.id;[IO.File]::WriteAllText($ackPath,$ack);$payload.ack=$ack
                try { Invoke-RemoteBootApi POST heartbeat $payload|Out-Null } catch { [Firmware]::Write('BootNext',[byte[]]@());throw }
                Restart-Computer
            }
        }
    } catch { Write-Warning $_.Exception.Message }
    Start-Sleep -Seconds 12
}
