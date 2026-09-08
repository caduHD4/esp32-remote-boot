$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
Get-ChildItem $root -Filter *.ps1 -Recurse|ForEach-Object {
    $tokens=$null;$errors=$null
    [Management.Automation.Language.Parser]::ParseFile($_.FullName,[ref]$tokens,[ref]$errors)|Out-Null
    if($errors.Count){throw ($errors|Out-String)}
}
Add-Type -Path (Join-Path $root 'agent/windows/Firmware.cs')
$b=[Firmware]::NewOption(1,2048,4096,[guid]'00000000-0000-0000-0000-000000000001')
$entry=[Firmware]::Parse(4,$b)
if(-not $entry.blocked -or $entry.id -ne '0004'){throw 'Firmware parser failed'}
try{[Firmware]::Parse(4,[byte[]]@(1,2,3));throw 'Accepted truncation'}catch{if($_.Exception.Message -eq 'Accepted truncation'){throw}}
Write-Host 'PASS: PowerShell syntax and C# EFI serialization/parser'
