param([switch]$Test, [switch]$NoPublish)
$ErrorActionPreference = 'Stop'
$originalPath = $env:PATH
Push-Location $PSScriptRoot
try {
    $compiler = (Get-Command g++.exe -ErrorAction SilentlyContinue).Source
    if (-not $compiler -and (Test-Path 'C:\msys64\ucrt64\bin\g++.exe')) {
        $compiler = 'C:\msys64\ucrt64\bin\g++.exe'
    }
    if (-not $compiler) { throw 'A 64-bit MinGW-w64 C++17 compiler is required.' }
    $env:PATH = (Split-Path -Parent $compiler) + ';' + $env:PATH
    New-Item -ItemType Directory -Force -Path 'build' | Out-Null
    $flags = @('-std=c++17','-O2','-Wall','-Wextra','-Wpedantic','-static','-static-libgcc','-static-libstdc++','-Isrc','-isystem','vendor/raylib/include')
    $libraries = @('vendor/raylib/lib/libraylib.a','-lopengl32','-lgdi32','-lwinmm','-ldwmapi','-lcomdlg32')
    $sources = @(Get-ChildItem -LiteralPath 'src' -Filter '*.cpp' | Sort-Object Name | Select-Object -ExpandProperty FullName)
    & $compiler @flags '-mwindows' @sources 'tests/ui_checks.cpp' 'tests/detector_window_checks.cpp' @libraries '-o' 'build/LightLab2D.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Simulator build failed.' }
    if ($Test) {
        & $compiler @flags 'tests/optics_tests.cpp' 'src/scene.cpp' 'src/optics.cpp' @libraries '-o' 'build/optics_tests.exe'
        if ($LASTEXITCODE -ne 0) { throw 'Test build failed.' }
        & '.\build\optics_tests.exe'
        if ($LASTEXITCODE -ne 0) { throw 'Optical tests failed.' }
        & $compiler @flags 'tests/scene_library_tests.cpp' 'src/scene_library.cpp' 'src/scene.cpp' @libraries '-o' 'build/scene_library_tests.exe'
        if ($LASTEXITCODE -ne 0) { throw 'Scene library test build failed.' }
        & '.\build\scene_library_tests.exe'
        if ($LASTEXITCODE -ne 0) { throw 'Scene library tests failed.' }
        $testRun = Start-Process -FilePath (Join-Path $PWD 'build\LightLab2D.exe') -ArgumentList '--smoke-test' -WorkingDirectory (Join-Path $PWD 'build') -WindowStyle Hidden -Wait -PassThru
        Get-Content -LiteralPath 'build\ui-checks.txt'
        if (Test-Path 'build/detector-window-checks.txt') { Get-Content -LiteralPath 'build/detector-window-checks.txt' }
        if ($testRun.ExitCode -ne 0) { throw 'Interface test failed.' }
    }
    if (-not $NoPublish) {
        Copy-Item -LiteralPath 'build/LightLab2D.exe' -Destination 'LightLab2D.exe' -Force
        Write-Output 'Built LightLab2D.exe (also in build/LightLab2D.exe)'
    } else { Write-Output 'Built build/LightLab2D.exe; running application left open.' }
} finally {
    $env:PATH = $originalPath
    Pop-Location
}
