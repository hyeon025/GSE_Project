param(
    [ValidateRange(1024, 65535)][int]$Port = 8765,
    [string]$Snapshot = (Join-Path $env:LOCALAPPDATA 'GSE_Project/Monitor/snapshot.json'),
    [string]$PythonPath,
    [switch]$Background
)

$ErrorActionPreference = 'Stop'
$server = Join-Path $PSScriptRoot 'monitor_server.py'
$root = Split-Path $PSScriptRoot -Parent
$snapshotPath = [IO.Path]::GetFullPath($Snapshot)
$lastPort = [Math]::Min(65535, $Port + 20)

# Reuse only this project's bridge and source; never terminate an occupied port.
for ($candidate = $Port; $candidate -le $lastPort; $candidate++) {
    $url = "http://127.0.0.1:$candidate"
    try {
        $health = Invoke-RestMethod "$url/api/health" -TimeoutSec 2
        if ($health.bridge.service -eq 'gse-world-monitor' -and
            $health.bridge.projectRoot -eq $root -and $health.bridge.snapshotPath -eq $snapshotPath) {
            Write-Output "Monitor already running: $url/"
            Write-Output "Source status: $($health.status)"
            return
        }
    } catch { }
    $probe = [Net.Sockets.TcpListener]::new([Net.IPAddress]::Loopback, $candidate)
    try {
        $probe.Start()
        $Port = $candidate
        break
    } catch {
        if ($candidate -eq $lastPort) { throw 'No free local monitor port found.' }
    } finally { $probe.Stop() }
}

$python = $null
if ($PythonPath) {
    if (-not (Test-Path -LiteralPath $PythonPath -PathType Leaf)) { throw 'PythonPath must point to an existing Python executable.' }
    $executable = & $PythonPath -c 'import sys; assert sys.version_info >= (3, 9); print(sys.executable)'
    if ($LASTEXITCODE -ne 0) { throw 'PythonPath must provide Python 3.9 or newer.' }
    $python = "$executable"
}
foreach ($name in @('py', 'python', 'python3')) {
    if ($python) { break }
    $command = Get-Command $name -ErrorAction SilentlyContinue
    if (-not $command) { continue }
    try {
        $arguments = @('-c', 'import sys; assert sys.version_info >= (3, 9); print(sys.executable)')
        if ($name -eq 'py') { $arguments = @('-3') + $arguments }
        $executable = & $command.Source @arguments 2>$null
        if ($LASTEXITCODE -eq 0 -and $executable -and (Test-Path -LiteralPath "$executable")) {
            $python = "$executable"
            break
        }
    } catch { }
}
# Modeling workstations may have Blender's bundled Python but only an empty py launcher.
# Reuse that installed runtime; the bridge requires only the standard library.
if (-not $python) {
    $bundledPattern = Join-Path $env:ProgramFiles 'Blender Foundation\Blender *\*\python\bin\python.exe'
    foreach ($runtime in (Get-ChildItem -Path $bundledPattern -File -ErrorAction SilentlyContinue | Sort-Object FullName -Descending)) {
        try {
            $executable = & $runtime.FullName -c 'import sys; assert sys.version_info >= (3, 9); print(sys.executable)' 2>$null
            if ($LASTEXITCODE -eq 0 -and $executable) { $python = "$executable"; break }
        } catch { }
    }
}
if (-not $python) { throw 'Python 3.9+ is required. No working Python runtime was found.' }

if (-not $Background) {
    & $python $server --port $Port --snapshot $snapshotPath
    if ($LASTEXITCODE -ne 0) { throw "Monitor server exited with code $LASTEXITCODE." }
    return
}

$logs = Join-Path $env:LOCALAPPDATA 'GSE_Project/Monitor'
New-Item -ItemType Directory -Path $logs -Force | Out-Null
$stdout = Join-Path $logs "server-$Port.log"
$stderr = Join-Path $logs "server-$Port.error.log"
$process = Start-Process -FilePath $python -WindowStyle Hidden -PassThru -WorkingDirectory $root `
    -ArgumentList @('"' + $server + '"', '--port', "$Port", '--snapshot', '"' + $snapshotPath + '"') `
    -RedirectStandardOutput $stdout -RedirectStandardError $stderr
$url = "http://127.0.0.1:$Port"
for ($attempt = 0; $attempt -lt 20; $attempt++) {
    if ($process.HasExited) { throw "Monitor exited. See $stderr" }
    try {
        $health = Invoke-RestMethod "$url/api/health" -TimeoutSec 1
        if ($health.bridge.service -eq 'gse-world-monitor' -and $health.bridge.snapshotPath -eq $snapshotPath) {
            Write-Output "Monitor: $url/"
            Write-Output "JSON: $url/api/state"
            Write-Output "Source status: $($health.status) | PID: $($process.Id)"
            Write-Output "Log: $stdout"
            return
        }
    } catch { }
    Start-Sleep -Milliseconds 200
}
throw "Monitor startup could not be confirmed. Check $stderr (PID $($process.Id))."
