function Invoke-ShutdownCommand {
    param($Command, [hashtable]$Payload, [bool]$Allowed, [string]$SessionId, [string]$AckPath, [ref]$Ack)
    if(-not $Allowed -or $Command.action -ne 'shutdown') { return }
    if($Command.id -notmatch '^[0-9a-f]{32}$' -or
       -not ($Command.PSObject.Properties.Name -contains 'session_id') -or
       $Command.session_id -ne $SessionId -or $Command.id -eq $Ack.Value) { return }
    [IO.File]::WriteAllText($AckPath,$Command.id)
    $Ack.Value=$Command.id; $Payload.ack=$Command.id
    $confirmation=Invoke-RemoteBootApi POST heartbeat $Payload
    if(($confirmation.PSObject.Properties.Name -contains 'ack_accepted') -and $confirmation.ack_accepted -eq $true) {
        Stop-Computer -ErrorAction Stop
    } else { Write-Warning 'Shutdown canceled: acknowledgment rejected' }
}
