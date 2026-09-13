$ErrorActionPreference = "Stop"

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

Write-Host ""
Write-Host "=== 3E-Engine Foundation Verification ==="
Write-Host ""

$Required = @(
    "config\studio.json",

    "config\ui\default\main_menu.json",
    "config\ui\default\toolbar.json",
    "config\ui\default\panels.json",
    "config\ui\default\shortcuts.json",
    "config\ui\default\layout.json",

    "config\commands\core.json",

    "games\_template\game.json",
    "games\jackie\game.json",

    "games\jackie\ui\main_menu.json",
    "games\jackie\ui\toolbar.json",
    "games\jackie\ui\panels.json",
    "games\jackie\ui\shortcuts.json",
    "games\jackie\ui\layout.json"
)

foreach ($Relative in $Required) {

    $Full = Join-Path $Root $Relative

    if (-not (Test-Path $Full)) {
        throw "[FAIL] Missing: $Relative"
    }

    Write-Host "[OK] $Relative"
}

Write-Host ""
Write-Host "Checking JSON..."
Write-Host ""

$JsonRoots = @(
    (Join-Path $Root "config"),
    (Join-Path $Root "games")
)

$JsonFiles =
    Get-ChildItem $JsonRoots -Recurse -Filter "*.json"

foreach ($File in $JsonFiles) {

    try {
        Get-Content $File.FullName -Raw |
            ConvertFrom-Json |
            Out-Null

        Write-Host "[JSON OK] $($File.FullName.Substring($Root.Length + 1))"
    }
    catch {
        throw "[JSON FAIL] $($File.FullName): $($_.Exception.Message)"
    }
}

Write-Host ""
Write-Host "Checking game manifests..."
Write-Host ""

$Ids = @{}

$GameDirs =
    Get-ChildItem (Join-Path $Root "games") -Directory |
    Where-Object { $_.Name -ne "_template" }

foreach ($GameDir in $GameDirs) {

    $ManifestPath =
        Join-Path $GameDir.FullName "game.json"

    if (-not (Test-Path $ManifestPath)) {
        throw "[FAIL] Missing game.json in $($GameDir.Name)"
    }

    $Manifest =
        Get-Content $ManifestPath -Raw |
        ConvertFrom-Json

    if ([string]::IsNullOrWhiteSpace($Manifest.id)) {
        throw "[FAIL] Game has no ID: $($GameDir.Name)"
    }

    if ($Ids.ContainsKey($Manifest.id)) {
        throw "[FAIL] Duplicate Game ID: $($Manifest.id)"
    }

    $Ids[$Manifest.id] = $true

    Write-Host "[GAME OK] $($Manifest.id) - $($Manifest.displayName)"
}

Write-Host ""
Write-Host "Running git diff check..."
Write-Host ""

Push-Location $Root

git diff --check

if ($LASTEXITCODE -ne 0) {
    Pop-Location
    throw "[FAIL] git diff --check failed."
}

Pop-Location

Write-Host ""
Write-Host "=========================================="
Write-Host "3E-Engine Foundation: PASS"
Write-Host "=========================================="
Write-Host ""
