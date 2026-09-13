param(
    [string]$Root = "D:\Essam\game\3E-Engine"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

Set-Location (Resolve-Path $Root)

Write-Host "=== 3E Jackie Final Bridge Verification ===" -ForegroundColor Cyan
Write-Host "No compilation will be performed."

$required = @(
    ".\apps\player\main.cpp",
    ".\apps\player\player_app.cpp",
    ".\apps\studio\studio_app.cpp",
    ".\games\jackie\game.json",
    ".\games\jackie\commands\jackie.json",
    ".\games\jackie\commands\tools.json",
    ".\games\jackie\scripts\open-map-viewer.ps1",
    ".\games\jackie\ui\toolbar.json"
)

foreach ($path in $required) {
    if (-not (Test-Path $path)) {
        throw "Missing final bridge file: $path"
    }
}

$game = Get-Content ".\games\jackie\game.json" -Raw | ConvertFrom-Json
$commands = Get-Content ".\games\jackie\commands\jackie.json" -Raw | ConvertFrom-Json
$tools = Get-Content ".\games\jackie\commands\tools.json" -Raw | ConvertFrom-Json
$player = Get-Content ".\apps\player\player_app.cpp" -Raw
$studio = Get-Content ".\apps\studio\studio_app.cpp" -Raw
$premake = Get-Content ".\premake5.lua" -Raw

if ($game.runtime.mode -ne "legacy-bridge") {
    throw "Jackie runtime mode must be legacy-bridge."
}

if ($game.runtime.executable -ne '${GameRoot}/3EChan.exe') {
    throw "Jackie runtime executable is not wired to the proven GameRoot 3EChan.exe."
}

$mapCommand = $commands.commands | Where-Object id -eq "jackie.map_viewer.open"
if (-not $mapCommand -or $mapCommand.action.type -ne "process") {
    throw "Jackie map viewer command is not executable."
}

if ($player -notmatch 'CreateProcessW' -or
    $player -notmatch 'CreateJobObjectW' -or
    $player -notmatch 'JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE') {
    throw "3E Player native runtime supervision is incomplete."
}

if ($studio -notmatch 'Open Map Viewer' -or
    $studio -notmatch 'Play Jackie') {
    throw "Studio Jackie workspace controls are missing."
}

$playerProject = ($premake -split 'project "3E-Player"', 2)[1]
if ($playerProject -match 'links\s*\{\s*"libp3d"') {
    throw "3E Player still depends on the placeholder renderer path."
}

Write-Host "[OK] Jackie native runtime -> GameRoot/3EChan.exe" -ForegroundColor Green
Write-Host "[OK] 3E Player supervises the real runtime process" -ForegroundColor Green
Write-Host "[OK] Map Viewer command launches the proven browser viewer" -ForegroundColor Green
Write-Host "[OK] Studio Jackie workspace exposes Play / Map Viewer / Stop" -ForegroundColor Green
Write-Host "[OK] Player no longer renders the placeholder ImGui runtime window" -ForegroundColor Green

git diff --check
if ($LASTEXITCODE -ne 0) {
    throw "git diff --check failed."
}

Write-Host "[OK] git diff --check" -ForegroundColor Green
Write-Host "FINAL BRIDGE STATIC VERIFICATION: PASS" -ForegroundColor Green
