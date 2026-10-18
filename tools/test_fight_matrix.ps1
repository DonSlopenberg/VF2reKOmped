param([string]$Name='native-matrix',[int]$Width=1920,[int]$Height=1080,[switch]$Short)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$folder=Join-Path $root ('analysis/fight-test-'+$Name)
New-Item -ItemType Directory -Path $folder -Force | Out-Null
if(!(Test-Path "$folder/userdata")){Copy-Item -LiteralPath "$root/analysis/startup-buttons/userdata" -Destination "$folder/userdata" -Recurse}
Set-Content "$folder/command.txt" ''
$env:VF2_CUSTOM_FIGHT_TEST="$folder/command.txt"
$env:VF2_CUSTOM_FIGHT_TEST_REPORT="$folder/state.txt"
$env:VF2_PANORAMA_TRACE="$folder/phase.csv"
$env:VF2_GPU_CAPTURE="$folder/captures"
$env:VF2_GPU_CAPTURE_DELAY_MS='13000'
$env:VF2_GPU_CAPTURE_INTERVAL_MS='5000'
$env:VF2_GPU_CAPTURE_COUNT='40'
$args="--game_data_root `"$root/data/game`" --user_data_root `"$folder/userdata`" --license_mask 1 --video_mode_width $Width --video_mode_height $Height --window_width 960 --window_height 540 --no-fullscreen --log_file `"$folder/runtime.log`""
try {$process=Start-Process "$root/data/engine/vf2_fight_candidate.exe" -WorkingDirectory "$root/data/engine" -ArgumentList $args -WindowStyle Hidden -PassThru}
finally {Remove-Item Env:VF2_CUSTOM_FIGHT_TEST,Env:VF2_CUSTOM_FIGHT_TEST_REPORT,Env:VF2_PANORAMA_TRACE,Env:VF2_GPU_CAPTURE,Env:VF2_GPU_CAPTURE_DELAY_MS,Env:VF2_GPU_CAPTURE_INTERVAL_MS,Env:VF2_GPU_CAPTURE_COUNT}
$process.Id | Set-Content "$folder/pid.txt"
$cases=@('9 0 0 1 1 1 10','0 9 1 0 1 1 10','9 9 1 1 1 1 10','restart','9 9 0 0 1 1 11','2 1 1 1 1 1 0')
if(!$Short){for($arena=0;$arena -lt 16;$arena++){$cases+=('{0} {1} 1 1 1 1 {2}' -f ($arena%11),(($arena+4)%11),$arena)}}
$watch=[Diagnostics.Stopwatch]::StartNew()
try {
  Start-Sleep -Seconds 8
  foreach($case in $cases){
    if($process.HasExited){throw "Test game exited before configuration: $case"}
    Add-Content "$folder/cases.txt" ('{0:F3} {1}' -f $watch.Elapsed.TotalSeconds,$case)
    Set-Content "$folder/command.txt" $case
    Start-Sleep -Seconds 7
  }
  @{alive_at_end=(!$process.HasExited);seconds=$watch.Elapsed.TotalSeconds;cases=$cases} | ConvertTo-Json | Set-Content "$folder/result.json"
} finally {if(!$process.HasExited){Stop-Process -InputObject $process}}
Write-Output $folder
