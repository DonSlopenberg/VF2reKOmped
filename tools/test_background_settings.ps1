$ErrorActionPreference='Stop'
$vf2Root=Split-Path $PSScriptRoot -Parent
$vf2Base=Join-Path $vf2Root 'analysis/background-settings-roundtrip'
foreach($vf2Stage in @('save','load')) {
  $vf2Out=Join-Path $vf2Base $vf2Stage
  New-Item -ItemType Directory -Force -Path $vf2Out | Out-Null
  if(!(Test-Path -LiteralPath (Join-Path $vf2Out 'userdata'))) {
    Copy-Item -LiteralPath (Join-Path $vf2Root 'analysis/startup-buttons/userdata') -Destination (Join-Path $vf2Out 'userdata') -Recurse
  }
  $env:VF2_SETTINGS_TEST='1'
  if($vf2Stage -eq 'load') {$env:VF2_SETTINGS_CONFIG_TEST=Join-Path $vf2Base 'save/userdata/settings-test.toml'}
  $vf2Arguments='--game_data_root "'+(Join-Path $vf2Root 'data/game')+'" --user_data_root "'+(Join-Path $vf2Out 'userdata')+'" --video_mode_width 1920 --video_mode_height 1080 --no-fullscreen --license_mask 1 --vf2_no_intro=true --log_file "'+(Join-Path $vf2Out 'runtime.log')+'"'
  if($vf2Stage -eq 'save') {$vf2Arguments+=' --vf2_background_fov 90 --vf2_background_horizon 64'}
  try {
    $vf2Process=Start-Process -FilePath (Join-Path $vf2Root 'data/engine/vf2_panorama_candidate.exe') -WorkingDirectory (Join-Path $vf2Root 'data/engine') -WindowStyle Hidden -ArgumentList $vf2Arguments -PassThru
  } finally {Remove-Item Env:VF2_SETTINGS_TEST;Remove-Item Env:VF2_SETTINGS_CONFIG_TEST -ErrorAction SilentlyContinue}
  try {
    Start-Sleep -Seconds 8
    $vf2Process.Refresh()
    if($vf2Process.HasExited){throw "Settings test exited in stage $vf2Stage"}
    $vf2Log=Get-Content (Join-Path $vf2Out 'runtime.log') -Raw
    if($vf2Log -notmatch 'horizon slider drawn: value=64.*independent FOV=90') {throw "Settings did not round-trip in stage $vf2Stage"}
  } finally {if(!$vf2Process.HasExited){Stop-Process -InputObject $vf2Process -ErrorAction Stop}}
}
Write-Output 'PASS: FOV 90 and horizon 64 saved and loaded independently in a fresh process.'
