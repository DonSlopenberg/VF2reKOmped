param([string]$Name='check',[switch]$Skip,[int]$CharacterSteps=5,[string]$Exe='vf2_fight_candidate.exe')
$ErrorActionPreference='Stop'
$vf2Root=Split-Path $PSScriptRoot -Parent
$vf2Folder=Join-Path $vf2Root ('analysis/arcade-colors-'+$Name)
New-Item -ItemType Directory -Force $vf2Folder | Out-Null
Copy-Item "$vf2Root/analysis/startup-buttons/userdata" "$vf2Folder/userdata" -Recurse
[IO.File]::WriteAllText("$vf2Folder/input.txt",'0')
$env:VF2_AUTOMATED_INPUT="$vf2Folder/input.txt"
$env:VF2_GPU_CAPTURE="$vf2Folder/captures"
$env:VF2_GPU_CAPTURE_DELAY_MS='14000'
$env:VF2_GPU_CAPTURE_INTERVAL_MS='2000'
$env:VF2_GPU_CAPTURE_COUNT='22'
$env:VF2_CUSTOM_FIGHT_TEST_REPORT="$vf2Folder/state.txt"
$vf2SkipValue=if($Skip){'true'}else{'false'}
try {$vf2Process=Start-Process "$vf2Root/data/engine/$Exe" -WorkingDirectory "$vf2Root/data/engine" -WindowStyle Hidden -PassThru -ArgumentList "--game_data_root `"$vf2Root/data/game`" --user_data_root `"$vf2Folder/userdata`" --license_mask 1 --vf2_output_resolution 1280x720 --vf2_no_intro=$vf2SkipValue --no-fullscreen --window_width 640 --window_height 360 --log_file `"$vf2Folder/runtime.log`""}
finally {Remove-Item Env:VF2_AUTOMATED_INPUT,Env:VF2_GPU_CAPTURE,Env:VF2_GPU_CAPTURE_DELAY_MS,Env:VF2_GPU_CAPTURE_INTERVAL_MS,Env:VF2_GPU_CAPTURE_COUNT,Env:VF2_CUSTOM_FIGHT_TEST_REPORT}
$vf2Watch=[Diagnostics.Stopwatch]::StartNew()
try {
 foreach($vf2Event in @(@(14,'1000'),@(17,'10'),@(20,'1000'),@(23,'1000'),@(26,'1000'),@(29,'1000'),@(32,'10'))) {
  while($vf2Watch.Elapsed.TotalSeconds -lt $vf2Event[0]){Start-Sleep -Milliseconds 50}
  [IO.File]::WriteAllText("$vf2Folder/input.txt",$vf2Event[1]);Start-Sleep -Milliseconds 100
  [IO.File]::WriteAllText("$vf2Folder/input.txt",'0')
 }
 while($vf2Watch.Elapsed.TotalSeconds -lt 34){Start-Sleep -Milliseconds 50}
 for($vf2Step=0;$vf2Step -lt $CharacterSteps;$vf2Step++) {
  [IO.File]::WriteAllText("$vf2Folder/input.txt",'8');Start-Sleep -Milliseconds 120
  [IO.File]::WriteAllText("$vf2Folder/input.txt",'0');Start-Sleep -Milliseconds 180
 }
 [IO.File]::WriteAllText("$vf2Folder/input.txt",'4000');Start-Sleep -Milliseconds 200
 [IO.File]::WriteAllText("$vf2Folder/input.txt",'0')
 while($vf2Watch.Elapsed.TotalSeconds -lt 55){Start-Sleep -Milliseconds 100}
} finally {if(!$vf2Process.HasExited){Stop-Process -InputObject $vf2Process;$vf2Process.WaitForExit()}}
