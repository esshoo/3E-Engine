param(
    [Parameter(Mandatory = $true)]
    [string]$GameRoot
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$GameRoot = [System.IO.Path]::GetFullPath($GameRoot)

$candidates = @(
    "3EChan-Level-Viewer-Pro-v3",
    "3EChan-Level-Viewer-Pro-v2",
    "3EChan-Level-Viewer-Pro-v1"
)

foreach ($name in $candidates) {
    $viewerRoot = Join-Path $GameRoot "ExportedAssets\$name"

    if (-not (Test-Path $viewerRoot)) {
        continue
    }

    $ps1 = Join-Path $viewerRoot "Start-Viewer.ps1"
    if (Test-Path $ps1) {
        $arguments =
            "-NoProfile -ExecutionPolicy Bypass -File `"$ps1`""

        Start-Process `
            -FilePath "powershell.exe" `
            -ArgumentList $arguments `
            -WorkingDirectory $viewerRoot

        Write-Host "Opened Jackie map viewer: $name"
        exit 0
    }

    $cmd = Join-Path $viewerRoot "Start-Viewer.cmd"
    if (Test-Path $cmd) {
        $arguments =
            "/c `"$cmd`""

        Start-Process `
            -FilePath "cmd.exe" `
            -ArgumentList $arguments `
            -WorkingDirectory $viewerRoot

        Write-Host "Opened Jackie map viewer: $name"
        exit 0
    }

    $html = Join-Path $viewerRoot "index.html"
    if (Test-Path $html) {
        Start-Process $html
        Write-Host "Opened Jackie map viewer HTML: $name"
        exit 0
    }
}

throw "No Jackie map viewer was found under: $GameRoot\ExportedAssets"
