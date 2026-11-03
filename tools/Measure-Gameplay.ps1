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
msbuild SimpleGame\Dependencies\GLFW\GLFW.vcxproj /nologo /v:minimal /p:Configuration=Release /p:Platform=x64
if errorlevel 1 exit /b 1
cl /nologo /DGLEW_STATIC /D_CRT_SECURE_NO_WARNINGS /I SimpleGame\Dependencies\GLEW\glew-2.2.0\include /EHsc /O2 /std:c++14 /MD /Fo:x64\perf\play-objects\ /Fe:"$out\$Label.exe" tests\PlayLoopBenchmark.cpp SimpleGame\ArtAsset.cpp SimpleGame\ArtPresentation.cpp SimpleGame\Renderer.cpp SimpleGame\RendererBatch.cpp SimpleGame\RendererStorage.cpp SimpleGame\RendererText.cpp SimpleGame\RendererProfile.cpp SimpleGame\CharacterMesh.cpp SimpleGame\WorldMonitor.cpp SimpleGame\Dependencies\GLEW\glew-2.2.0\src\glew.c /link SimpleGame\Dependencies\GLFW\build\x64\Release\glfw3.lib opengl32.lib gdi32.lib user32.lib shell32.lib
"@ | Set-Content -LiteralPath $command -Encoding Default
Push-Location $root
try {
    & cmd /c $command
    if ($LASTEXITCODE -ne 0) { throw 'Play benchmark build failed.' }
    & "$out\$Label.exe" $Label $data $Mode
    if ($LASTEXITCODE -ne 0) { throw 'Play benchmark failed.' }
    Get-Content -LiteralPath "$out\$Label.txt"
} finally { Pop-Location }
