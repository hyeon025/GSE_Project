param(
    [Parameter(Mandatory=$true)][string] $SavePath,
    [ValidateSet('idle','move','combat')][string] $Mode = 'move',
    [ValidatePattern('^[a-zA-Z0-9_-]+$')][string] $Label = 'play-check'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$out = Join-Path $root 'x64\perf'
$data = Join-Path $out "$Label-data"
New-Item -ItemType Directory -Force "$data\GSE_Project", "$out\play-objects" | Out-Null
Copy-Item -LiteralPath $SavePath -Destination "$data\GSE_Project\world_v1.txt" -Force
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'Visual Studio C++ toolchain not found.' }
$developer = Join-Path $installation 'Common7\Tools\VsDevCmd.bat'
$command = Join-Path $out 'build-play-benchmark.cmd'
@"
@echo off
call "$developer" -arch=x64 >nul
if errorlevel 1 exit /b 1
cl /nologo /EHsc /O2 /std:c++14 /MD /Fo:x64\perf\play-objects\ /Fe:"$out\$Label.exe" tests\PlayLoopBenchmark.cpp SimpleGame\ArtAsset.cpp SimpleGame\ArtPresentation.cpp SimpleGame\Renderer.cpp SimpleGame\RendererBatch.cpp SimpleGame\RendererStorage.cpp SimpleGame\RendererText.cpp SimpleGame\RendererProfile.cpp SimpleGame\CharacterMesh.cpp SimpleGame\WorldMonitor.cpp /link /LIBPATH:SimpleGame\Dependencies\x64 freeglut.lib glew32.lib opengl32.lib gdi32.lib user32.lib shell32.lib
"@ | Set-Content -LiteralPath $command -Encoding Default
Push-Location $root
try {
    & cmd /c $command
    if ($LASTEXITCODE -ne 0) { throw 'Play benchmark build failed.' }
    foreach ($name in @('freeglut.dll','glew32.dll')) {
        $dll = @("x64\Release\$name","x64\Debug\$name") | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
        if (-not $dll) { throw "Missing runtime library: $name" }
        Copy-Item -LiteralPath $dll -Destination $out -Force
    }
    & "$out\$Label.exe" $Label $data $Mode
    if ($LASTEXITCODE -ne 0) { throw 'Play benchmark failed.' }
    Get-Content -LiteralPath "$out\$Label.txt"
} finally { Pop-Location }
