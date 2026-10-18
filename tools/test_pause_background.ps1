param([int]$Arena=3,[switch]$Color,[string]$Name='arena3-before',[string]$Engine='vf2_pause_test.exe',[int]$Fov=120,[switch]$Fullscreen,[switch]$Motion)
$ErrorActionPreference='Stop'
$vf2Root=(Get-Location).Path
$vf2Folder=Join-Path $vf2Root ('analysis/pause-'+$Name)
New-Item -ItemType Directory -Force $vf2Folder | Out-Null
if(!(Test-Path "$vf2Folder/userdata")){Copy-Item "$vf2Root/analysis/startup-buttons/userdata" "$vf2Folder/userdata" -Recurse}
Set-Content "$vf2Folder/command.txt" '';Set-Content "$vf2Folder/input.txt" '0 0'
$env:VF2_CUSTOM_FIGHT_TEST="$vf2Folder/command.txt"
$env:VF2_CUSTOM_FIGHT_TEST_REPORT="$vf2Folder/state.txt"
$env:VF2_AUTOMATED_INPUT="$vf2Folder/input.txt"
$env:VF2_PANORAMA_TRACE="$vf2Folder/phase.csv"
$env:VF2_GPU_CAPTURE="$vf2Folder/captures"
$env:VF2_GPU_CAPTURE_DELAY_MS='19000';$env:VF2_GPU_CAPTURE_INTERVAL_MS='17';$env:VF2_GPU_CAPTURE_COUNT='20'
$env:VF2_CAPTURE_FAST='1';if($Color){$env:VF2_PANORAMA_COLOR_TEST='1'}
$vf2WindowMode=if($Fullscreen){'--fullscreen'}else{'--no-fullscreen'}
try{$vf2P=Start-Process "$vf2Root/data/engine/$Engine" -WorkingDirectory "$vf2Root/data/engine" -WindowStyle Hidden -PassThru -ArgumentList "--game_data_root `"$vf2Root/data/game`" --user_data_root `"$vf2Folder/userdata`" --license_mask 1 --vf2_output_resolution 3840x2160 --vf2_no_intro=true --vf2_background_fov=$Fov $vf2WindowMode --window_width 1280 --window_height 720 --log_file `"$vf2Folder/runtime.log`""}
finally{Remove-Item Env:VF2_CUSTOM_FIGHT_TEST,Env:VF2_CUSTOM_FIGHT_TEST_REPORT,Env:VF2_AUTOMATED_INPUT,Env:VF2_PANORAMA_TRACE,Env:VF2_GPU_CAPTURE,Env:VF2_GPU_CAPTURE_DELAY_MS,Env:VF2_GPU_CAPTURE_INTERVAL_MS,Env:VF2_GPU_CAPTURE_COUNT;Remove-Item Env:VF2_CAPTURE_FAST,Env:VF2_PANORAMA_COLOR_TEST -ErrorAction SilentlyContinue}
try{
Start-Sleep -Seconds 10;Set-Content "$vf2Folder/command.txt" "0 1 0 0 1 1 $Arena"
if($Motion){Start-Sleep -Seconds 4;Set-Content "$vf2Folder/input.txt" '0 8';Start-Sleep -Seconds 3}else{Start-Sleep -Seconds 7};Set-Content "$vf2Folder/input.txt" '10 0'
Start-Sleep -Milliseconds 300;Set-Content "$vf2Folder/input.txt" '0 0'
Start-Sleep -Seconds 10
}finally{if(!$vf2P.HasExited){Stop-Process -InputObject $vf2P}}


