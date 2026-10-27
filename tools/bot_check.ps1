param(
    [string]$OutDir = (Join-Path $env:TEMP "pakal_bot_check"),
    [string]$Root = (Split-Path $PSScriptRoot -Parent),
    [float]$TimeScale = 8          # the bot plays this many times faster: the steps are the same, so is the trace
)
# Plays every case of tests/baseline (<level>_seed<n>.trace) with the bot and compares its trace with the stored one.
# A change that keeps the game as it was passes every case; one that changes the game on purpose regenerates them
# (copy the new traces over the stored ones, in the same commit). Prints one line per case and exits 1 on a difference.
$baseline = Join-Path $Root "tests\baseline"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$failed = 0
foreach ($stored in Get-ChildItem $baseline -Filter "*.trace") {
    if ($stored.BaseName -notmatch "^(.+)_seed(\d+)$") { continue }
    $level = $Matches[1]; $seed = $Matches[2]
    $out = Join-Path $OutDir $stored.BaseName
    $result = & (Join-Path $PSScriptRoot "bot_run.ps1") -Level "Assets/zombies/$level/level.json" -Out $out -Seed $seed -TimeoutSeconds 300 -Root $Root -TimeScale $TimeScale
    $expected = @(Get-Content $stored.FullName)
    $actual = @(Get-Content "$out.trace")
    $differences = @(Compare-Object $expected $actual -SyncWindow 0)
    if ($differences.Count -eq 0) {
        "SAME     $($stored.BaseName): $result"
    } else {
        $failed++
        $first = 0
        while ($first -lt [Math]::Min($expected.Count, $actual.Count) -and $expected[$first] -eq $actual[$first]) { $first++ }
        "DIFFERS  $($stored.BaseName): $result; first difference at line $($first + 1) (see $out.trace)"
    }
}
if ($failed -gt 0) { exit 1 }
