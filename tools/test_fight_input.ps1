$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$folder=Join-Path $root 'analysis/fight-input-test'
New-Item -ItemType Directory -Path $folder -Force | Out-Null
if(!(Test-Path "$folder/userdata")){Copy-Item -LiteralPath "$root/analysis/startup-buttons/userdata" -Destination "$folder/userdata" -Recurse}
Set-Content "$folder/command.txt" '0 1 0 0 1 1 0'
Set-Content "$folder/input.txt" '0 0'
$env:VF2_CUSTOM_FIGHT_TEST="$folder/command.txt"
$env:VF2_CUSTOM_FIGHT_TEST_REPORT="$folder/state.txt"
$env:VF2_AUTOMATED_INPUT="$folder/input.txt"
try {$process=Start-Process "$root/data/engine/vf2_fight_candidate.exe" -WorkingDirectory "$root/data/engine" -ArgumentList "--game_data_root `"$root/data/game`" --user_data_root `"$folder/userdata`" --license_mask 1 --video_mode_width 1920 --video_mode_height 1080 --window_width 960 --window_height 540 --no-fullscreen --log_file `"$folder/runtime.log`"" -WindowStyle Hidden -PassThru}
finally {Remove-Item Env:VF2_CUSTOM_FIGHT_TEST,Env:VF2_CUSTOM_FIGHT_TEST_REPORT,Env:VF2_AUTOMATED_INPUT}
try {
  Start-Sleep -Seconds 11
  Set-Content "$folder/input.txt" '4 8';Start-Sleep -Seconds 2
  Set-Content "$folder/input.txt" '8 4';Start-Sleep -Seconds 2
  Set-Content "$folder/input.txt" '4000 2000';Start-Sleep -Seconds 2
  Set-Content "$folder/input.txt" '0 0';Start-Sleep -Seconds 1
  @{alive_at_end=(!$process.HasExited)} | ConvertTo-Json | Set-Content "$folder/result.json"
} finally {if(!$process.HasExited){Stop-Process -InputObject $process}}
