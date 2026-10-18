# Single-steps the simulation during a CPU-vs-CPU custom fight and captures
# every tick (GPU readback only): exact consecutive frames for measuring how
# often an element of the scene updates.
#   powershell -ExecutionPolicy Bypass -File tools\test_step_capture.ps1 -Install <dir> -Out <dir> [-Steps 24]
param([Parameter(Mandatory)][string]$Install,[Parameter(Mandatory)][string]$Out,[string]$Fight='2 5 1 1 0 0 2',
      [string]$Resolution='1920x1080',[int]$Steps=24,[int]$FreezeAfterFight=6,[int]$StepMs=700,[string]$Extra='')
$ErrorActionPreference='Stop'
if(Test-Path $Out){Remove-Item -Recurse -Force $Out}
New-Item -ItemType Directory "$Out\userdata" -Force | Out-Null
Set-Content "$Out\command.txt" ''
Set-Content "$Out\step.txt" ''
$env:VF2_CUSTOM_FIGHT_TEST="$Out\command.txt"; $env:VF2_STEP_TEST="$Out\step.txt"
$arguments="--game_data_root `"$Install\data\game`" --user_data_root `"$Out\userdata`" --input_backend xinput --license_mask 1 --user_language 1 "+
  "--vf2_output_resolution $Resolution --vf2_no_intro=true --no-fullscreen --window_width 960 --window_height 540 --log_file `"$Out\game.log`" $Extra"
# Captures start when the script creates the capture trigger below; the
# capture thread's delay is fixed at launch, so use a long delay and count.
$env:VF2_GPU_CAPTURE="$Out\captures"; $env:VF2_CAPTURE_FAST='1'
$env:VF2_GPU_CAPTURE_INTERVAL_MS="$($StepMs/2)"; $env:VF2_GPU_CAPTURE_COUNT='200'
try {$process=Start-Process "$Install\data\engine\vf2.exe" -WorkingDirectory "$Install\data\engine" -ArgumentList $arguments -WindowStyle Hidden -PassThru}
finally {Remove-Item Env:VF2_CUSTOM_FIGHT_TEST,Env:VF2_STEP_TEST,Env:VF2_GPU_CAPTURE,Env:VF2_CAPTURE_FAST,Env:VF2_GPU_CAPTURE_INTERVAL_MS,Env:VF2_GPU_CAPTURE_COUNT}
$watch=[Diagnostics.Stopwatch]::StartNew()
try {
  while($watch.Elapsed.TotalSeconds -lt 60 -and !$process.HasExited -and
        !(Select-String -LiteralPath "$Out\game.log" -Pattern 'Main Menu music' -SimpleMatch -Quiet -ErrorAction SilentlyContinue)){Start-Sleep -Milliseconds 200}
  Start-Sleep -Seconds 2
  Set-Content "$Out\command.txt" $Fight
  while($watch.Elapsed.TotalSeconds -lt 90 -and !$process.HasExited -and
        !(Select-String -LiteralPath "$Out\game.log" -Pattern 'custom fight initialized' -SimpleMatch -Quiet)){Start-Sleep -Milliseconds 100}
  $freeze=$watch.Elapsed.TotalSeconds+$FreezeAfterFight
  while($watch.Elapsed.TotalSeconds -lt $freeze -and !$process.HasExited){Start-Sleep -Milliseconds 20}
  "frozen at $([int]$watch.Elapsed.TotalSeconds) s; steps from capture index of $([int]$watch.Elapsed.TotalMilliseconds)"
  for($k=0;$k -le $Steps;$k++) {
    Set-Content "$Out\step.txt" "$k"
    $until=$freeze+($k+1)*$StepMs/1000
    while($watch.Elapsed.TotalSeconds -lt $until -and !$process.HasExited){Start-Sleep -Milliseconds 20}
  }
  Start-Sleep -Seconds 2
} finally {if(!$process.HasExited){Stop-Process -InputObject $process}}
Get-Content "$Out\game.log" | Where-Object {$_ -match 'step test'} | ForEach-Object {$_ -replace '^.*?\] VF2','VF2'}
"captures: $((Get-ChildItem "$Out\captures" -Filter *.rgba -ErrorAction SilentlyContinue).Count)"
