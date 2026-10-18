param([string]$Name='menu',[int]$Seconds=25,[switch]$NoIntro)
$ErrorActionPreference='Stop'
$vf2Root=Split-Path $PSScriptRoot -Parent
$vf2Folder=Join-Path $vf2Root ('analysis/startup-menu-test-'+$Name)
New-Item -ItemType Directory -Force $vf2Folder | Out-Null
Copy-Item -LiteralPath "$vf2Root/analysis/startup-buttons/userdata" -Destination "$vf2Folder/userdata" -Recurse
$env:VF2_CUSTOM_FIGHT_TEST_REPORT="$vf2Folder/state.txt"
$env:VF2_GPU_CAPTURE="$vf2Folder/captures"
$env:VF2_GPU_CAPTURE_DELAY_MS='8000'
$env:VF2_GPU_CAPTURE_INTERVAL_MS='2000'
$env:VF2_GPU_CAPTURE_COUNT='8'
$vf2Args="--game_data_root `"$vf2Root/data/game`" --user_data_root `"$vf2Folder/userdata`" --license_mask 1 --video_mode_width 1920 --video_mode_height 1080 --window_width 960 --window_height 540 --no-fullscreen --log_file `"$vf2Folder/runtime.log`""
if($NoIntro){$vf2Args+=' --vf2_no_intro=true'}
try {$vf2Process=Start-Process "$vf2Root/data/engine/vf2_fight_candidate.exe" -WorkingDirectory "$vf2Root/data/engine" -ArgumentList $vf2Args -WindowStyle Hidden -PassThru}
finally {Remove-Item Env:VF2_CUSTOM_FIGHT_TEST_REPORT,Env:VF2_GPU_CAPTURE,Env:VF2_GPU_CAPTURE_DELAY_MS,Env:VF2_GPU_CAPTURE_INTERVAL_MS,Env:VF2_GPU_CAPTURE_COUNT}
try {Start-Sleep -Seconds $Seconds; @{alive_at_end=(!$vf2Process.HasExited)} | ConvertTo-Json | Set-Content "$vf2Folder/result.json"}
finally {if(!$vf2Process.HasExited){Stop-Process -InputObject $vf2Process}}
Write-Output $vf2Folder
