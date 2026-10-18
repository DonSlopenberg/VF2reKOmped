param([string]$Name='baseline',[switch]$Skip,[int]$Seconds=12,[switch]$UhdMonitor)
$ErrorActionPreference='Stop'
$vf2Root=Split-Path $PSScriptRoot -Parent
$vf2Folder=Join-Path $vf2Root ('analysis/startup-logos-'+$Name)
New-Item -ItemType Directory -Force $vf2Folder | Out-Null
Copy-Item -LiteralPath "$vf2Root/analysis/startup-buttons/userdata" -Destination "$vf2Folder/userdata" -Recurse
$env:VF2_GPU_CAPTURE="$vf2Folder/captures"
$env:VF2_GPU_CAPTURE_DELAY_MS='0'
$env:VF2_GPU_CAPTURE_INTERVAL_MS='500'
$env:VF2_GPU_CAPTURE_COUNT='20'
$vf2SkipValue=if($Skip){'true'}else{'false'}
 $vf2Mode=if($UhdMonitor){'--vf2_output_resolution 3840x2160 --fullscreen --vf2_display_device "\\.\DISPLAY1"'}else{'--vf2_output_resolution 1280x720 --no-fullscreen --window_width 640 --window_height 360'}
try {$vf2Process=Start-Process "$vf2Root/data/engine/vf2_fight_candidate.exe" -WorkingDirectory "$vf2Root/data/engine" -WindowStyle Hidden -PassThru -ArgumentList "--game_data_root `"$vf2Root/data/game`" --user_data_root `"$vf2Folder/userdata`" --license_mask 1 $vf2Mode --vf2_no_intro=$vf2SkipValue --log_file `"$vf2Folder/runtime.log`""}
finally {Remove-Item Env:VF2_GPU_CAPTURE,Env:VF2_GPU_CAPTURE_DELAY_MS,Env:VF2_GPU_CAPTURE_INTERVAL_MS,Env:VF2_GPU_CAPTURE_COUNT}
try {Start-Sleep -Seconds $Seconds; if($vf2Process.HasExited){throw 'Game exited during startup'}}
finally {if(!$vf2Process.HasExited){Stop-Process -InputObject $vf2Process;$vf2Process.WaitForExit()}}
Write-Output $vf2Folder
