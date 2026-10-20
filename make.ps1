# PowerShell entry point for MSYS2's native GNU Make (named mingw32-make).
# Usage: .\make.ps1 [run|test|clean|full] [-j4]
$ErrorActionPreference = 'Stop'
$makeCommand = Get-Command mingw32-make.exe, make.exe -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $makeCommand) {
    throw 'GNU Make is missing. In MSYS2 UCRT64 run: pacman -S --needed mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-sfml'
}
Push-Location $PSScriptRoot
try {
    & $makeCommand.Source @args
    if ($LASTEXITCODE -ne 0) { throw "Make failed with exit code $LASTEXITCODE" }
} finally {
    Pop-Location
}
