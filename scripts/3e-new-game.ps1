param(
    [Parameter(Mandatory = $true)]
    [string]$Id,

    [Parameter(Mandatory = $true)]
    [string]$Name
)

$ErrorActionPreference = "Stop"

if ($Id -notmatch '^[a-z0-9][a-z0-9_-]*$') {
    throw "Game ID may contain lowercase letters, numbers, _ and - only."
}

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

$Template = Join-Path $Root "games\_template"
$Destination = Join-Path $Root "games\$Id"

if (-not (Test-Path $Template)) {
    throw "Game template not found: $Template"
}

if (Test-Path $Destination) {
    throw "Game already exists: $Destination"
}

Copy-Item $Template $Destination -Recurse

$ManifestPath = Join-Path $Destination "game.json"

$Manifest = Get-Content $ManifestPath -Raw | ConvertFrom-Json

$Manifest.id = $Id
$Manifest.displayName = $Name
$Manifest.enabled = $true
$Manifest.integration.state = "new"

$Manifest |
    ConvertTo-Json -Depth 32 |
    Set-Content $ManifestPath -Encoding utf8

Write-Host ""
Write-Host "[3E] Game created successfully:"
Write-Host "     ID   : $Id"
Write-Host "     Name : $Name"
Write-Host "     Path : $Destination"
Write-Host ""
Write-Host "3E-Studio will auto-discover this game from game.json."
