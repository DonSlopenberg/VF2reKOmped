param([int]$Attempts = 6, [int]$StartIndex = 1)
$ErrorActionPreference = 'Stop'
$vf2Root = Split-Path -Parent $PSScriptRoot
$vf2Exe = Join-Path $vf2Root 'native\out\build\win-amd64-relwithdebinfo\vf2.exe'
$vf2Assets = Join-Path $vf2Root 'data\game'
$vf2UserData = Join-Path $vf2Root 'native\userdata'
$vf2Hints = Join-Path $vf2Root 'native\vf2_analysis.toml'
$lastProbeIndex = $StartIndex + $Attempts - 1
for ($probeIndex = $StartIndex; $probeIndex -le $lastProbeIndex; $probeIndex++) {
    $vf2Log = Join-Path $vf2Root "analysis\runtime-probe-$probeIndex.log"
    if (Test-Path -LiteralPath $vf2Log) { throw "Log already exists: $vf2Log" }
    $vf2Args = '--game_data_root "' + $vf2Assets + '" --user_data_root "' + $vf2UserData + '" --log_file "' + $vf2Log + '" --input_backend xinput --resolution 1080p'
    $probeProcess = Start-Process -FilePath $vf2Exe -ArgumentList $vf2Args -WorkingDirectory (Split-Path -Parent $vf2Exe) -WindowStyle Hidden -PassThru
    $probeProcess.Id | Set-Content -LiteralPath (Join-Path $vf2Root 'analysis\vf2-runtime.pid')
    [void]$probeProcess.WaitForExit(15000)
    $probeText = if (Test-Path -LiteralPath $vf2Log) { Get-Content -Raw -LiteralPath $vf2Log } else { '' }
    $probeMatch = [regex]::Match($probeText, 'invalid or unregistered function at guest address (0x[0-9A-Fa-f]{8})')
    if (-not $probeMatch.Success) {
        Write-Output "Attempt $probeIndex : no missing function reported; exited=$($probeProcess.HasExited); PID=$($probeProcess.Id)"
        Write-Output ($probeText -split "`n" | Select-Object -Last 12)
        exit 0
    }
    if (-not $probeProcess.HasExited) { $probeProcess.Kill(); $probeProcess.WaitForExit() }
    $probeAddress = $probeMatch.Groups[1].Value.ToUpper().Replace('0X','0x')
    Write-Output "Attempt $probeIndex : runtime discovered $probeAddress"
    if ($probeIndex -eq $lastProbeIndex) { Write-Output 'Bound reached; last address not changed.'; exit 2 }
    $hintText = Get-Content -Raw -LiteralPath $vf2Hints
    if ($hintText.Contains('"' + $probeAddress + '"')) { throw "Address already seeded: $probeAddress" }
    $newHint = "`n# Runtime evidence: runtime-probe-$probeIndex.log`n" + '"' + $probeAddress + '" = { size = 0 }' + "`n"
    [IO.File]::AppendAllText($vf2Hints, $newHint)
    $probeBuildLog = Join-Path $vf2Root "analysis\build-probe-$probeIndex.log"
    & cmd.exe /d /c (Join-Path $vf2Root 'native\build.cmd') *> $probeBuildLog
    if ($LASTEXITCODE -ne 0) { Get-Content -LiteralPath $probeBuildLog -Tail 15; exit $LASTEXITCODE }
    Write-Output "Attempt $probeIndex : regenerated and linked successfully."
}
