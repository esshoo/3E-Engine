param(
    [string]$Root = "D:\Essam\game\3E-Engine"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

Set-Location (Resolve-Path $Root)

Write-Host "=== 3E Foundation Freeze Verification ===" -ForegroundColor Cyan
Write-Host "No compilation will be performed."

$Required = @(
    ".\apps\studio\main.cpp",
    ".\apps\studio\studio_app.cpp",
    ".\apps\player\main.cpp",
    ".\apps\player\player_app.cpp",
    ".\engine\data\json_value.cpp",
    ".\engine\runtime\runtime_root.cpp",
    ".\engine\scripting\lua_runtime.cpp",
    ".\vendor\lua\src\lua.h",
    ".\config\studio.json",
    ".\config\ui\default\main_menu.json",
    ".\config\ui\default\panels.json",
    ".\config\ui\default\shortcuts.json",
    ".\games\jackie\game.json",
    ".\games\jackie\commands\jackie.json",
    ".\games\jackie\schemas\asset_rules.json",
    ".\scripts\package-3e-host.ps1",
    ".\.github\workflows\build-3e-studio-on-demand.yml"
)

foreach ($Path in $Required) {
    if (-not (Test-Path $Path)) {
        throw "Missing Foundation file: $Path"
    }
}

Write-Host "[OK] Required foundation files" -ForegroundColor Green

$JsonFiles = @(
    Get-ChildItem ".\config" -Recurse -File -Filter "*.json"
    Get-ChildItem ".\games" -Recurse -File -Filter "*.json"
)

foreach ($File in $JsonFiles) {
    try {
        Get-Content $File.FullName -Raw |
            ConvertFrom-Json |
            Out-Null
    }
    catch {
        throw "Invalid JSON: $($File.FullName)`n$($_.Exception.Message)"
    }
}

Write-Host "[OK] JSON syntax: $($JsonFiles.Count) files" -ForegroundColor Green

$Premake = Get-Content ".\premake5.lua" -Raw
$LuaPremake = Get-Content ".\vendor\lua\premake5.lua" -Raw

foreach ($Project in @(
    'project "3E-Studio"',
    'project "3E-Player"'
)) {
    if ($Premake -notmatch [regex]::Escape($Project)) {
        throw "Premake project missing from root premake5.lua: $Project"
    }
}

if ($Premake -notmatch 'include\s+"vendor/lua"') {
    throw 'Root premake5.lua is missing include "vendor/lua".'
}

if ($LuaPremake -notmatch 'project\s+"lua"') {
    throw 'vendor/lua/premake5.lua is missing project "lua".'
}

Write-Host "[OK] Premake host boundaries" -ForegroundColor Green
Write-Host "[OK] Lua Premake include/project boundary" -ForegroundColor Green

$StudioMain = Get-Content ".\apps\studio\main.cpp" -Raw
$StudioApp = Get-Content ".\apps\studio\studio_app.cpp" -Raw
$PlayerMain = Get-Content ".\apps\player\main.cpp" -Raw
$Workflow = Get-Content ".\.github\workflows\build-3e-studio-on-demand.yml" -Raw

if ($StudioMain -notmatch 'threee::runtime::FindRuntimeRoot') {
    throw "Studio is not using the generic runtime-root service."
}

if ($StudioApp -match 'std::filesystem::path FindRuntimeRoot') {
    throw "Duplicate Studio runtime-root implementation remains."
}

if ($StudioApp -notmatch 'ResolvePlayerExecutable' -or
    $StudioApp -notmatch 'CreateProcessW' -or
    $StudioApp -notmatch 'TerminateProcess') {
    throw "Studio Player lifecycle is incomplete."
}

if ($StudioApp -notmatch 'm_builtinHandlers\["studio\.exit"\]') {
    throw "Studio Exit command is not implemented."
}

if ($PlayerMain -notmatch '--game' -or
    $PlayerMain -notmatch '--project' -or
    $PlayerMain -notmatch '--verify-runtime') {
    throw "3E-Player CLI is incomplete."
}

if ($Workflow -notmatch '3E-Studio\.vcxproj' -or
    $Workflow -notmatch '3E-Player\.vcxproj' -or
    $Workflow -notmatch 'package-3e-host\.ps1') {
    throw "GitHub Actions host workflow is incomplete."
}

Write-Host "[OK] Runtime host wiring" -ForegroundColor Green
Write-Host "[OK] Manual one-run GitHub build workflow" -ForegroundColor Green

git diff --check

if ($LASTEXITCODE -ne 0) {
    throw "git diff --check failed."
}

Write-Host "[OK] git diff --check" -ForegroundColor Green

Write-Host ""
Write-Host "FOUNDATION FREEZE STATIC VERIFICATION: PASS" -ForegroundColor Green
Write-Host "Native compilation has NOT been performed." -ForegroundColor Green