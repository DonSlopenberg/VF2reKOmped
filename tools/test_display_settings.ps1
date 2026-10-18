param([string]$Name='final',[int]$Seconds=20)
$ErrorActionPreference='Stop'
$vf2Root=Split-Path $PSScriptRoot -Parent
$vf2Folder=Join-Path $vf2Root ('analysis/display-settings-test-'+$Name)
New-Item -ItemType Directory -Force $vf2Folder | Out-Null
Copy-Item -LiteralPath "$vf2Root/analysis/startup-buttons/userdata" -Destination "$vf2Folder/userdata" -Recurse
[IO.File]::WriteAllText("$vf2Folder/userdata/custom-fight.cfg","VF2_CUSTOM_FIGHT 1`n9 10 1 0 10 3 5 60`n")
$env:VF2_SETTINGS_TEST='1'
$env:VF2_FIGHT_LAUNCHER_UI_TEST='1'
$env:VF2_SETTINGS_CONFIG_TEST="$vf2Folder/config.toml"
$env:VF2_GPU_CAPTURE="$vf2Folder/captures"
$env:VF2_GPU_CAPTURE_DELAY_MS='16000'
$env:VF2_GPU_CAPTURE_COUNT='1'
try {$vf2Process=Start-Process "$vf2Root/data/engine/vf2_fight_candidate.exe" -WorkingDirectory "$vf2Root/data/engine" -WindowStyle Hidden -PassThru -ArgumentList "--game_data_root `"$vf2Root/data/game`" --user_data_root `"$vf2Folder/userdata`" --license_mask 1 --vf2_output_resolution 3840x2160 --fullscreen --log_file `"$vf2Folder/runtime.log`""}
finally {Remove-Item Env:VF2_SETTINGS_TEST,Env:VF2_FIGHT_LAUNCHER_UI_TEST,Env:VF2_SETTINGS_CONFIG_TEST,Env:VF2_GPU_CAPTURE,Env:VF2_GPU_CAPTURE_DELAY_MS,Env:VF2_GPU_CAPTURE_COUNT}
try {Start-Sleep -Seconds $Seconds; @{alive_at_end=(!$vf2Process.HasExited)} | ConvertTo-Json | Set-Content "$vf2Folder/result.json"}
finally {if(!$vf2Process.HasExited){Stop-Process -InputObject $vf2Process;$vf2Process.WaitForExit()}}
Write-Output $vf2Folder
