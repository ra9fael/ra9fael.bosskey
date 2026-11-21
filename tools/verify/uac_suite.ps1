param(
    [Parameter(Mandatory = $true)][string]$Runtime,
    [string]$Output = 'build/uac-input/validation/high',
    [ValidateSet('guard', 'crash', 'disconnect', 'autohide')][string]$StartAt = 'guard'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Set-Location $root
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this isolated acceptance controller as administrator.'
}
$python = (Get-Command python.exe).Source
$tasks = @('guard', 'crash', 'disconnect', 'autohide')
New-Item -ItemType Directory -Force -Path $Output | Out-Null
$failed = $false
foreach ($task in $tasks[$tasks.IndexOf($StartAt)..($tasks.Length - 1)]) {
    & $python tools/verify/strict_preview.py $task "$Output/$task" --exe "$Runtime/bosskey.exe" --enhanced 2>&1 |
        Tee-Object -FilePath "$Output/$task.log"
    $result = $LASTEXITCODE
    "Controller result: $task exit=$result" | Tee-Object -FilePath "$Output/$task.log" -Append
    if ($result) { $failed = $true }
}
if ($failed) { exit 1 }
