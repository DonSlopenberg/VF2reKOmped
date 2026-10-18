# Opens the native pause menu during a CPU-vs-CPU custom fight and captures
# frames (GPU readback only), e.g. to check menu placement per resolution.
#   powershell -ExecutionPolicy Bypass -File tools\test_pause_menu.ps1 -Install <dir> -Out <dir> -Resolution 2560x1080
param([Parameter(Mandatory)][string]$Install,[Parameter(Mandatory)][string]$Out,[string]$Resolution='1920x1080',
      [string]$Fight='2 5 1 1 0 0 2')
$ErrorActionPreference='Stop'
if(Test-Path $Out){Remove-Item -Recurse -Force $Out}
New-Item -ItemType Directory "$Out\userdata" -Force | Out-Null
Set-Content "$Out\command.txt" ''
Set-Content "$Out\input.txt" '0'
$env:VF2_CUSTOM_FIGHT_TEST="$Out\command.txt"
$env:VF2_AUTOMATED_INPUT="$Out\input.txt"
$arguments="--game_data_root `"$Install\data\game`" --user_data_root `"$Out\userdata`" --input_backend xinput --license_mask 1 --user_language 1 "+
  "--vf2_output_resolution $Resolution --vf2_no_intro=true --no-fullscreen --window_width 960 --window_height 540 --log_file `"$Out\game.log`""
# Capture timing is fixed at launch: frames from 30 s on, every 2 s.
$env:VF2_GPU_CAPTURE="$Out\captures"; $env:VF2_GPU_CAPTURE_DELAY_MS='30000'
$env:VF2_GPU_CAPTURE_INTERVAL_MS='2000'; $env:VF2_GPU_CAPTURE_COUNT='4'
try {$process=Start-Process "$Install\data\engine\vf2.exe" -WorkingDirectory "$Install\data\engine" -ArgumentList $arguments -WindowStyle Hidden -PassThru}
finally {Remove-Item Env:VF2_CUSTOM_FIGHT_TEST,Env:VF2_AUTOMATED_INPUT,Env:VF2_GPU_CAPTURE,Env:VF2_GPU_CAPTURE_DELAY_MS,Env:VF2_GPU_CAPTURE_INTERVAL_MS,Env:VF2_GPU_CAPTURE_COUNT}
$watch=[Diagnostics.Stopwatch]::StartNew()
try {
  while($watch.Elapsed.TotalSeconds -lt 60 -and !$process.HasExited -and
        !(Select-String -LiteralPath "$Out\game.log" -Pattern 'Main Menu music' -SimpleMatch -Quiet -ErrorAction SilentlyContinue)){Start-Sleep -Milliseconds 200}
  Start-Sleep -Seconds 2
  Set-Content "$Out\command.txt" $Fight
  # Press Start once the round is running (before the first capture at 30 s).
  while($watch.Elapsed.TotalSeconds -lt 26 -and !$process.HasExited){Start-Sleep -Milliseconds 100}
  Set-Content "$Out\input.txt" '10'; Start-Sleep -Milliseconds 300; Set-Content "$Out\input.txt" '0'
  while($watch.Elapsed.TotalSeconds -lt 40 -and !$process.HasExited){Start-Sleep -Milliseconds 100}
} finally {if(!$process.HasExited){Stop-Process -InputObject $process}}
Get-Content "$Out\game.log" | Where-Object {$_ -match 'VF2 (render selection|keyboard routing|synthetic)'} | ForEach-Object {$_ -replace '^.*?\] VF2','VF2'}
