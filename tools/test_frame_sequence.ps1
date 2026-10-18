# Captures a dense GPU frame sequence (GPU readback only) during a CPU-vs-CPU
# custom fight, e.g. to measure how often an arena element updates.
#   powershell -ExecutionPolicy Bypass -File tools\test_frame_sequence.ps1 -Install <dir> -Out <dir> [-Fight '2 5 1 1 0 0 2']
param([Parameter(Mandatory)][string]$Install,[Parameter(Mandatory)][string]$Out,[string]$Fight='2 5 1 1 0 0 2',
      [string]$Resolution='1920x1080',[int]$DelayMs=30000,[int]$IntervalMs=8,[int]$Count=60)
$ErrorActionPreference='Stop'
if(Test-Path $Out){Remove-Item -Recurse -Force $Out}
New-Item -ItemType Directory "$Out\userdata" -Force | Out-Null
Set-Content "$Out\command.txt" ''
$env:VF2_CUSTOM_FIGHT_TEST="$Out\command.txt"
$env:VF2_GPU_CAPTURE="$Out\captures"; $env:VF2_CAPTURE_FAST='1'
$env:VF2_GPU_CAPTURE_DELAY_MS="$DelayMs"; $env:VF2_GPU_CAPTURE_INTERVAL_MS="$IntervalMs"; $env:VF2_GPU_CAPTURE_COUNT="$Count"
$arguments="--game_data_root `"$Install\data\game`" --user_data_root `"$Out\userdata`" --input_backend xinput --license_mask 1 --user_language 1 "+
  "--vf2_output_resolution $Resolution --vf2_no_intro=true --no-fullscreen --window_width 960 --window_height 540 --log_file `"$Out\game.log`""
try {$process=Start-Process "$Install\data\engine\vf2.exe" -WorkingDirectory "$Install\data\engine" -ArgumentList $arguments -WindowStyle Hidden -PassThru}
finally {Remove-Item Env:VF2_CUSTOM_FIGHT_TEST,Env:VF2_GPU_CAPTURE,Env:VF2_CAPTURE_FAST,Env:VF2_GPU_CAPTURE_DELAY_MS,Env:VF2_GPU_CAPTURE_INTERVAL_MS,Env:VF2_GPU_CAPTURE_COUNT}
$watch=[Diagnostics.Stopwatch]::StartNew()
try {
  while($watch.Elapsed.TotalSeconds -lt 60 -and !$process.HasExited -and
        !(Select-String -LiteralPath "$Out\game.log" -Pattern 'Main Menu music' -SimpleMatch -Quiet -ErrorAction SilentlyContinue)){Start-Sleep -Milliseconds 200}
  Start-Sleep -Seconds 2
  Set-Content "$Out\command.txt" $Fight
  $end=$DelayMs/1000+$Count*($IntervalMs+40)/1000+10
  while($watch.Elapsed.TotalSeconds -lt $end -and !$process.HasExited){Start-Sleep -Milliseconds 200}
} finally {if(!$process.HasExited){Stop-Process -InputObject $process}}
Get-ChildItem "$Out\captures" -Filter *.rgba -ErrorAction SilentlyContinue | Measure-Object | ForEach-Object {"captures: $($_.Count)"}
