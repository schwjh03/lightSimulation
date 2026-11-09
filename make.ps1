# PowerShell entry point for MSYS2's native GNU Make (named mingw32-make).
# Usage: .\make.ps1 [run|build|test|help] [ARGS=--prism]
$ErrorActionPreference = 'Stop'
$makeCommand = Get-Command mingw32-make.exe, make.exe -ErrorAction SilentlyContinue | Select-Object -First 1
$makePath = if ($makeCommand) { $makeCommand.Source } else { 'C:\msys64\ucrt64\bin\mingw32-make.exe' }
if (-not (Test-Path -LiteralPath $makePath)) {
    throw 'GNU Make is missing. In MSYS2 UCRT64 run: pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make'
}
Push-Location $PSScriptRoot
try {
    & $makePath @args
    if ($LASTEXITCODE -ne 0) { throw "Make failed with exit code $LASTEXITCODE" }
} finally {
    Pop-Location
}
