param(
    [Parameter(Mandatory = $true)]
    [string[]] $Path,
    [double] $SkipSeconds = 0
)

$ErrorActionPreference = 'Stop'
$culture = [Globalization.CultureInfo]::InvariantCulture
$frames = 0L
$totals = @{}
$cpuTimes = New-Object 'System.Collections.Generic.List[double]'
$intervals = New-Object 'System.Collections.Generic.List[double]'
$drawCounts = New-Object 'System.Collections.Generic.List[double]'
$columns = @('draw_calls', 'draw_instanced', 'world_draws', 'post_draws', 'ui_draws',
    'instances', 'useful_vertices', 'submitted_vertices', 'upload_bytes', 'texture_upload_bytes',
    'mesh_hits', 'mesh_misses', 'mesh_builds', 'disk_hits', 'disk_misses', 'disk_rejected',
    'disk_writes', 'disk_write_errors', 'mesh_evictions', 'text_hits', 'text_misses',
    'flush_explicit', 'flush_pass', 'flush_texture', 'flush_capacity', 'flush_padding', 'flush_cache',
    'mesh_build_us', 'cache_io_us', 'previous_log_ms', 'frame_interval_ms')
foreach ($column in $columns) { $totals[$column] = 0.0 }

$files = @(Get-Item -Path $Path | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName -Unique)
if ($files.Count -eq 0) { throw 'No frame CSV files matched.' }
foreach ($file in $files) {
    $checked = $false
    Get-Content -LiteralPath $file.FullName |
        Where-Object { $_.Length -gt 0 -and -not $_.StartsWith('#') } |
        ConvertFrom-Csv |
        ForEach-Object {
            $row = $_
            if (-not $checked) {
                foreach ($column in ($columns + @('elapsed_ms', 'cpu_render_ms'))) {
                    if (-not $row.PSObject.Properties[$column]) { throw "Missing column $column in $($file.Name)" }
                }
                $checked = $true
            }
            $elapsed = [double]::Parse($row.elapsed_ms, $culture)
            if ($elapsed -ge $SkipSeconds * 1000) {
                $frames++
                foreach ($column in $columns) {
                    $totals[$column] += [double]::Parse($row.$column, $culture)
                }
                $cpuTimes.Add([double]::Parse($row.cpu_render_ms, $culture))
                $drawCounts.Add([double]::Parse($row.draw_calls, $culture))
                $interval = [double]::Parse($row.frame_interval_ms, $culture)
                if ($interval -gt 0) { $intervals.Add($interval) }
            }
        }
}
if ($frames -eq 0) { throw 'No frames remain after filtering.' }
$cpuTimes.Sort()
$intervals.Sort()
$drawCounts.Sort()
function Percentile($values, [double] $percent) {
    if ($values.Count -eq 0) { return $null }
    $index = [Math]::Max(0, [int][Math]::Ceiling($values.Count * $percent) - 1)
    return [Math]::Round($values[$index], 3)
}
$averages = [ordered]@{}
foreach ($column in $columns) {
    $averages[$column] = [Math]::Round($totals[$column] / $frames, 3)
}
[ordered]@{
    schema = 2
    files = @($files.FullName)
    frames = $frames
    skip_seconds = $SkipSeconds
    fps = $(if ($totals.frame_interval_ms -gt 0) {
        [Math]::Round($intervals.Count * 1000 / $totals.frame_interval_ms, 3)
    } else { $null })
    draw_calls_p95 = Percentile $drawCounts .95
    draw_calls_max = $drawCounts[$drawCounts.Count - 1]
    cpu_render_ms_p50 = Percentile $cpuTimes .50
    cpu_render_ms_p95 = Percentile $cpuTimes .95
    frame_interval_ms_p95 = Percentile $intervals .95
    vertex_padding_ratio = $(if ($totals.useful_vertices -gt 0) {
        [Math]::Round($totals.submitted_vertices / $totals.useful_vertices, 3)
    } else { $null })
    averages_per_frame = $averages
    totals = $totals
} | ConvertTo-Json -Depth 4
