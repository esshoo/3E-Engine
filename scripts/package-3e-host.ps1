param(
    [string]$Root = (Resolve-Path "$PSScriptRoot\.."),
    [string]$Output = ".artifact\3E-Studio"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

Set-Location (Resolve-Path $Root)

$OutputPath =
    [System.IO.Path]::GetFullPath(
        (Join-Path (Get-Location) $Output)
    )

Remove-Item $OutputPath -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $OutputPath | Out-Null

$Executables = @(
    ".\bin\3E-Studio.exe",
    ".\bin\3E-Player.exe"
)

foreach ($Executable in $Executables) {
    if (-not (Test-Path $Executable)) {
        throw "Missing host executable: $Executable"
    }

    Copy-Item `
        $Executable `
        (Join-Path $OutputPath ([System.IO.Path]::GetFileName($Executable))) `
        -Force
}

foreach ($Dir in @(
    "config",
    "games",
    "schemas",
    "scripts",
    "pipelines"
)) {
    if (Test-Path ".\$Dir") {
        Copy-Item `
            ".\$Dir" `
            (Join-Path $OutputPath $Dir) `
            -Recurse `
            -Force
    }
}

@"
3E-Engine portable host package.

Executables:
  3E-Studio.exe
  3E-Player.exe

Data-driven content:
  config/
  games/
  schemas/
  scripts/
  pipelines/

3E-Player is universal. It resolves games from:
  games/<game-id>/game.json

External game/project source files are not copied into this package.
"@ | Set-Content `
    (Join-Path $OutputPath "BUILD-INFO.txt") `
    -Encoding UTF8

Write-Host "Portable package ready: $OutputPath" -ForegroundColor Green