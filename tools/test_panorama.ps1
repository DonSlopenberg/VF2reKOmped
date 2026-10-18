param([int]$Width=1920,[int]$Height=1080,[string]$Name='16x9-final',
      [int]$Seconds=110,[switch]$NoSmooth,[switch]$Sweep,
      [double]$BackgroundFov=120,[switch]$SettingsOverlay,
      [double]$Horizon=0,[double]$GuestRefresh=60)
$ErrorActionPreference='Stop'
function Write-PanoramaInput([string]$Path,[string]$Value) {
  for($vf2Attempt=0;$vf2Attempt -lt 40;$vf2Attempt++) {
    try {[IO.File]::WriteAllText($Path,$Value);return}
    catch [IO.IOException] {Start-Sleep -Milliseconds 5}
  }
  throw 'Controller test input remained locked.'
}
$vf2Root=Split-Path $PSScriptRoot -Parent
$vf2Output=Join-Path $vf2Root ('analysis/panorama-test-'+$Name)
New-Item -ItemType Directory -Path $vf2Output -Force | Out-Null
$vf2User=Join-Path $vf2Output 'userdata'
if(!(Test-Path -LiteralPath $vf2User)) {
  Copy-Item -LiteralPath (Join-Path $vf2Root 'data/userdata') -Destination $vf2User -Recurse
}
$vf2Input=Join-Path $vf2Output 'input.txt'
Set-Content -LiteralPath $vf2Input -Value '0'
$env:VF2_GPU_CAPTURE=Join-Path $vf2Output 'captures'
$env:VF2_AUTOMATED_INPUT=$vf2Input
$env:VF2_PANORAMA_TRACE=Join-Path $vf2Output 'phase.csv'
if($Sweep){
  $env:VF2_PANORAMA_SWEEP='1'
  $env:VF2_GPU_CAPTURE_DELAY_MS='38000'
  $env:VF2_GPU_CAPTURE_INTERVAL_MS='100'
  $env:VF2_GPU_CAPTURE_COUNT='80'
}
$vf2Arguments='--game_data_root "'+(Join-Path $vf2Root 'data/game')+'" --user_data_root "'+$vf2User+'" --license_mask 1 --user_language 1 --video_mode_width '+$Width+' --video_mode_height '+$Height+' --window_width 960 --window_height '+[int](960*$Height/$Width)+' --no-fullscreen --log_file "'+(Join-Path $vf2Output 'runtime.log')+'"'
if($NoSmooth){$vf2Arguments+=' --vf2_background_smooth_scroll=false'}
$vf2Arguments+=' --vf2_background_fov '+$BackgroundFov.ToString([Globalization.CultureInfo]::InvariantCulture)
$vf2Arguments+=' --vf2_background_horizon '+$Horizon.ToString([Globalization.CultureInfo]::InvariantCulture)
$vf2Arguments+=' --video_mode_refresh_rate '+$GuestRefresh.ToString([Globalization.CultureInfo]::InvariantCulture)
if($SettingsOverlay){$env:VF2_SETTINGS_TEST='1'}
try {
  $vf2Process=Start-Process -FilePath (Join-Path $vf2Root 'data/engine/vf2_panorama_candidate.exe') -WorkingDirectory (Join-Path $vf2Root 'data/engine') -ArgumentList $vf2Arguments -WindowStyle Hidden -PassThru
} finally {
  Remove-Item Env:VF2_GPU_CAPTURE,Env:VF2_AUTOMATED_INPUT,Env:VF2_PANORAMA_TRACE
  if($SettingsOverlay){Remove-Item Env:VF2_SETTINGS_TEST}
  if($Sweep){Remove-Item Env:VF2_PANORAMA_SWEEP,Env:VF2_GPU_CAPTURE_DELAY_MS,Env:VF2_GPU_CAPTURE_INTERVAL_MS,Env:VF2_GPU_CAPTURE_COUNT}
}
@{pid=$vf2Process.Id;width=$Width;height=$Height;arguments=$vf2Arguments} | ConvertTo-Json | Set-Content (Join-Path $vf2Output 'launch.json')
$vf2Watch=[Diagnostics.Stopwatch]::StartNew()
$vf2Events=@(@(6,'10'),@(9,'1000'),@(12,'10'),@(15,'1000'),@(18,'1000'),@(21,'1000'),@(26,'1000'),@(34,'10'),@(37,'1000'))
try {
  foreach($vf2Event in $vf2Events){
    while($vf2Watch.Elapsed.TotalSeconds -lt $vf2Event[0] -and !$vf2Process.HasExited){Start-Sleep -Milliseconds 100}
    if($vf2Process.HasExited){throw 'Test game exited before navigation completed.'}
    Write-PanoramaInput $vf2Input $vf2Event[1]
    Start-Sleep -Milliseconds 220
    Write-PanoramaInput $vf2Input '0'
  }
  while($vf2Watch.Elapsed.TotalSeconds -lt $Seconds -and !$vf2Process.HasExited){Start-Sleep -Milliseconds 250}
  @{alive_at_end=(!$vf2Process.HasExited);elapsed_seconds=$vf2Watch.Elapsed.TotalSeconds} | ConvertTo-Json | Set-Content (Join-Path $vf2Output 'result.json')
} finally {
  if(!$vf2Process.HasExited){Stop-Process -InputObject $vf2Process; $vf2Process.WaitForExit()}
}
Write-Output $vf2Output
