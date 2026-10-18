param([int]$Refresh=60,[string]$Name='vertical-60',[switch]$NoSmooth,
      [switch]$Demo,[double]$Fov=120,[double]$Horizon=0,[int]$Seconds=65)
$ErrorActionPreference='Stop'
$vf2Root=Split-Path $PSScriptRoot -Parent
$vf2Folder=Join-Path $vf2Root ('analysis/'+$Name)
New-Item -ItemType Directory -Force $vf2Folder | Out-Null
if(!(Test-Path "$vf2Folder/userdata")) {
  Copy-Item -LiteralPath "$vf2Root/analysis/startup-buttons/userdata" -Destination "$vf2Folder/userdata" -Recurse
}
$vf2OriginalMode=(& "$vf2Root/analysis/test_refresh.exe").Trim()
$vf2OriginalRefresh=[int]($vf2OriginalMode.Split(' ')[2])
$vf2Process=$null
try {
  $vf2TestMode=if($Refresh){& "$vf2Root/analysis/test_refresh.exe" $Refresh}else{$vf2OriginalMode}
  if($LASTEXITCODE -ne 0){throw 'Unable to select the physical monitor refresh.'}
  $vf2TestMode=$vf2TestMode.Trim()
  $vf2ActualRefresh=[int]($vf2TestMode.Split(' ')[2])
  Set-Content "$vf2Folder/monitor.txt" "$vf2OriginalMode -> $vf2TestMode"
  [IO.File]::WriteAllText("$vf2Folder/input.txt",'0')
  [IO.File]::WriteAllText("$vf2Folder/command.txt",'')
  $env:VF2_AUTOMATED_INPUT="$vf2Folder/input.txt"
  $env:VF2_CUSTOM_FIGHT_TEST="$vf2Folder/command.txt"
  $env:VF2_CUSTOM_FIGHT_TEST_REPORT="$vf2Folder/state.txt"
  $env:VF2_PANORAMA_TRACE="$vf2Folder/phase.csv"
  $env:VF2_GPU_CAPTURE="$vf2Folder/captures"
  $env:VF2_GPU_CAPTURE_DELAY_MS='12000'
  $env:VF2_GPU_CAPTURE_INTERVAL_MS='1000'
  $env:VF2_GPU_CAPTURE_COUNT='40'
  $vf2Arguments="--game_data_root `"$vf2Root/data/game`" --user_data_root `"$vf2Folder/userdata`" --license_mask 1 --vf2_output_resolution 3840x2160 --vf2_no_intro=true --no-fullscreen --window_width 960 --window_height 540 --log_file `"$vf2Folder/runtime.log`""
  if($NoSmooth){$vf2Arguments+=' --vf2_background_smooth_scroll=false'}
  $vf2Arguments+=' --vf2_background_fov '+$Fov.ToString([Globalization.CultureInfo]::InvariantCulture)
  $vf2Arguments+=' --vf2_background_horizon '+$Horizon.ToString([Globalization.CultureInfo]::InvariantCulture)
  try {
    $vf2Process=Start-Process "$vf2Root/data/engine/vf2_panorama_candidate.exe" -WorkingDirectory "$vf2Root/data/engine" -WindowStyle Hidden -ArgumentList $vf2Arguments -PassThru
  } finally {
    Remove-Item Env:VF2_AUTOMATED_INPUT,Env:VF2_CUSTOM_FIGHT_TEST,Env:VF2_CUSTOM_FIGHT_TEST_REPORT,Env:VF2_PANORAMA_TRACE,Env:VF2_GPU_CAPTURE,Env:VF2_GPU_CAPTURE_DELAY_MS,Env:VF2_GPU_CAPTURE_INTERVAL_MS,Env:VF2_GPU_CAPTURE_COUNT
  }
  # Return from Main Menu to title/attract. The second phase uses the native
  # match initializer, with CPU control and a saved profile confined here.
  $vf2Watch=[Diagnostics.Stopwatch]::StartNew()
  if(!$Demo){foreach($vf2Event in @(@(6,'2000'),@(9,'2000'))) {
    while($vf2Watch.Elapsed.TotalSeconds -lt $vf2Event[0] -and !$vf2Process.HasExited){Start-Sleep -Milliseconds 100}
    [IO.File]::WriteAllText("$vf2Folder/input.txt",$vf2Event[1]);Start-Sleep -Milliseconds 160
    [IO.File]::WriteAllText("$vf2Folder/input.txt",'0')
  }}
  if($Demo) {
    while($vf2Watch.Elapsed.TotalSeconds -lt 45 -and !$vf2Process.HasExited) {
      if((Test-Path "$vf2Folder/runtime.log") -and
         ((Get-Content "$vf2Folder/runtime.log" -Raw) -match 'opening native Main Menu')) {break}
      Start-Sleep -Milliseconds 200
    }
    Start-Sleep -Seconds 2
    # Main Menu -> Arcade options -> confirmation -> native attract loop.
    # Do not press Start in the Model 2 game: leave its demo cameras running.
    for($vf2Confirm=0;$vf2Confirm -lt 4;$vf2Confirm++) {
      [IO.File]::WriteAllText("$vf2Folder/input.txt",'1000');Start-Sleep -Milliseconds 200
      [IO.File]::WriteAllText("$vf2Folder/input.txt",'0');Start-Sleep -Seconds 2
    }
  }
  while($vf2Watch.Elapsed.TotalSeconds -lt 28 -and !$vf2Process.HasExited){Start-Sleep -Milliseconds 100}
  if(!$Demo){[IO.File]::WriteAllText("$vf2Folder/command.txt",'2 3 1 1 1 1 1')}
  while($vf2Watch.Elapsed.TotalSeconds -lt $Seconds -and !$vf2Process.HasExited){Start-Sleep -Milliseconds 100}
  @{alive_at_end=(!$vf2Process.HasExited);physical_refresh=$vf2ActualRefresh;
    seconds=$vf2Watch.Elapsed.TotalSeconds;smooth_scroll=(!$NoSmooth.IsPresent)} | ConvertTo-Json | Set-Content "$vf2Folder/result.json"
} finally {
  if($vf2Process -and !$vf2Process.HasExited){Stop-Process -InputObject $vf2Process;$vf2Process.WaitForExit()}
  $vf2CurrentMode=(& "$vf2Root/analysis/test_refresh.exe").Trim()
  if([int]($vf2CurrentMode.Split(' ')[2]) -ne $vf2OriginalRefresh) {
    & "$vf2Root/analysis/test_refresh.exe" $vf2OriginalRefresh
    if($LASTEXITCODE -ne 0){throw 'Could not restore the previous monitor refresh.'}
  }
}
Write-Output $vf2Folder
