param([ValidatePattern('^[a-zA-Z0-9_-]+$')][string] $Label = 'optimized')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$out = Join-Path $root 'x64\perf'
$objects = Join-Path $out 'benchmark-objects'
$data = Join-Path $out "$Label-data"
New-Item -ItemType Directory -Force $out, $objects, $data | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'Visual Studio C++ toolchain not found.' }
$developer = Join-Path $installation 'Common7\Tools\VsDevCmd.bat'
$command = Join-Path $out 'build-render-benchmark.cmd'
@"
@echo off
call "$developer" -arch=x64 >nul
if errorlevel 1 exit /b 1
cl /nologo /EHsc /O2 /std:c++14 /MD /Fo:x64\perf\benchmark-objects\ /Fe:"$out\$Label.exe" tests\ArtRenderBenchmark.cpp SimpleGame\ArtAsset.cpp SimpleGame\ArtPresentation.cpp SimpleGame\Renderer.cpp SimpleGame\RendererBatch.cpp SimpleGame\RendererStorage.cpp SimpleGame\RendererText.cpp SimpleGame\RendererProfile.cpp /link /LIBPATH:SimpleGame\Dependencies\x64 freeglut.lib glew32.lib opengl32.lib gdi32.lib user32.lib
"@ | Set-Content -LiteralPath $command -Encoding Default
Push-Location $root
try {
    & cmd /c $command
    if ($LASTEXITCODE -ne 0) { throw 'Benchmark build failed.' }
    foreach ($name in @('freeglut.dll', 'glew32.dll')) {
        $dll = @("x64\Release\$name", "x64\Debug\$name") | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
        if (-not $dll) { throw "Missing runtime library: $name" }
        Copy-Item -LiteralPath $dll -Destination $out -Force
    }
    & "$out\$Label.exe" $Label $data
    if ($LASTEXITCODE -ne 0) { throw 'Benchmark execution failed.' }
    Get-Content -LiteralPath "$out\$Label.txt"
} finally { Pop-Location }
