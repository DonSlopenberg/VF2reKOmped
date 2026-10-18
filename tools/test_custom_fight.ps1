param([string]$Name='native-first',[int]$Width=1920,[int]$Height=1080,[int]$Seconds=65,[switch]$Direct)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$folder=Join-Path $root ('analysis/fight-test-'+$Name)
New-Item -ItemType Directory -Path $folder -Force | Out-Null
if(!(Test-Path "$folder/userdata")){Copy-Item -LiteralPath "$root/analysis/startup-buttons/userdata" -Destination "$folder/userdata" -Recurse}
Set-Content "$folder/input.txt" '0'
Set-Content "$folder/command.txt" ''
$env:VF2_CUSTOM_FIGHT_TEST="$folder/command.txt"
$env:VF2_CUSTOM_FIGHT_TEST_REPORT="$folder/state.txt"
$env:VF2_AUTOMATED_INPUT="$folder/input.txt"
$env:VF2_PANORAMA_TRACE="$folder/phase.csv"
$env:VF2_GPU_CAPTURE="$folder/captures"
$env:VF2_GPU_CAPTURE_DELAY_MS=if($Direct){'8000'}else{'38000'}
$env:VF2_GPU_CAPTURE_INTERVAL_MS='2000'
$env:VF2_GPU_CAPTURE_COUNT='30'
$arguments="--game_data_root `"$root/data/game`" --user_data_root `"$folder/userdata`" --license_mask 1 --video_mode_width $Width --video_mode_height $Height --window_width 960 --window_height 540 --no-fullscreen --log_file `"$folder/runtime.log`""
try {
  $process=Start-Process "$root/data/engine/vf2_fight_candidate.exe" -WorkingDirectory "$root/data/engine" -ArgumentList $arguments -WindowStyle Hidden -PassThru
} finally {
  Remove-Item Env:VF2_CUSTOM_FIGHT_TEST,Env:VF2_CUSTOM_FIGHT_TEST_REPORT,Env:VF2_AUTOMATED_INPUT,Env:VF2_PANORAMA_TRACE,Env:VF2_GPU_CAPTURE,Env:VF2_GPU_CAPTURE_DELAY_MS,Env:VF2_GPU_CAPTURE_INTERVAL_MS,Env:VF2_GPU_CAPTURE_COUNT
}
$process.Id | Set-Content "$folder/pid.txt"
$watch=[Diagnostics.Stopwatch]::StartNew()
$events=@(@(6,'10'),@(9,'1000'),@(12,'10'),@(15,'1000'),@(18,'1000'),@(21,'1000'),@(26,'1000'),@(34,'10'),@(37,'1000'))
try {
  if(!$Direct){foreach($event in $events){
    while($watch.Elapsed.TotalSeconds -lt $event[0] -and !$process.HasExited){Start-Sleep -Milliseconds 100}
    Set-Content "$folder/input.txt" $event[1];Start-Sleep -Milliseconds 220;Set-Content "$folder/input.txt" '0'
  }}
  $startAt=if($Direct){8}else{40}
  while($watch.Elapsed.TotalSeconds -lt $startAt -and !$process.HasExited){Start-Sleep -Milliseconds 100}
  Set-Content "$folder/command.txt" '9 0 0 1 1 1 10'
  while($watch.Elapsed.TotalSeconds -lt $Seconds -and !$process.HasExited){Start-Sleep -Milliseconds 100}
  @{alive_at_end=(!$process.HasExited);seconds=$watch.Elapsed.TotalSeconds} | ConvertTo-Json | Set-Content "$folder/result.json"
} finally {if(!$process.HasExited){Stop-Process -InputObject $process}}
Write-Output $folder
