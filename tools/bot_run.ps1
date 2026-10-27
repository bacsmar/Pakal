param(
    [string]$Level,                # relative to build-win64\bin, e.g. Assets/zombies/hollywood_s01t01/level.json
    [string]$Out,                  # prefix of the outputs: <Out>.log, <Out>.err, <Out>.trace, <Out>.profile.json
    [int]$TimeoutSeconds = 600,
    [string]$Seed = "2026",
    [hashtable]$ExtraEnv = @{},    # more variables, e.g. @{ ZOMBIES_AUTOPLAY = "idle"; ZOMBIES_SCREENSHOT = "x.tga" }
    [float]$TimeScale = 1,         # game seconds per real second (ZOMBIES_TIMESCALE): the same game, sooner
    [string]$Root = (Split-Path $PSScriptRoot -Parent)
)
# Runs the Zombies bot on one level and waits for LEVEL COMPLETE, a failed mission or the timeout.
# Prints VICTORY, DEFEAT (the player is down), FAILED, EXITED or TIMEOUT. The profile is a throwaway file, so the player's own is never touched.
# <Out>.trace keeps the game's state lines (every 2 s of game time) up to the end: two runs with the same seed give the
# same trace, so a change that keeps the game as it was keeps it too (Compare-Object (gc a.trace) (gc b.trace)).
$bin = Join-Path $Root "build-win64\bin"
$env:ZOMBIES_LEVEL = $Level
$env:ZOMBIES_AUTOPLAY = "1"
$env:ZOMBIES_SEED = $Seed
$env:ZOMBIES_PROFILE = "$Out.profile.json"
$env:PAKAL_FPS_LOG = "1"
$env:ZOMBIES_TIMESCALE = "$TimeScale"
foreach ($k in $ExtraEnv.Keys) { Set-Item "env:$k" $ExtraEnv[$k] }
if (Test-Path $env:ZOMBIES_PROFILE) { Remove-Item $env:ZOMBIES_PROFILE }
$p = Start-Process -FilePath (Join-Path $bin "PakalPlayer.exe") -ArgumentList "ZombiesModule.dll" -WorkingDirectory $bin `
    -RedirectStandardOutput "$Out.log" -RedirectStandardError "$Out.err" -PassThru
$start = Get-Date
$result = "TIMEOUT"
while (((Get-Date) - $start).TotalSeconds -lt $TimeoutSeconds) {
    Start-Sleep -Seconds 2
    if ($p.HasExited) { $result = "EXITED"; break }
    $text = (Get-Content "$Out.log", "$Out.err" -Raw -ErrorAction SilentlyContinue) -join ""
    if ($text -match "LEVEL COMPLETE") { $result = "VICTORY"; break }
    if ($text -match "mission failed") { $result = "FAILED"; break }
    if ($text -match "the player is down") { $result = "DEFEAT"; break }
}
$secs = [int]((Get-Date) - $start).TotalSeconds
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
$trace = @()
foreach ($line in (Get-Content "$Out.log" -ErrorAction SilentlyContinue)) {
    if ($line -match "\[ZombiesLevel\] t |LEVEL COMPLETE|mission failed|the player is down|restarting level") { $trace += $line }
    if ($line -match "LEVEL COMPLETE|mission failed|the player is down") { break }
}
$trace | Set-Content "$Out.trace"
"$result after $secs s"
