# Functional smoke test of an installation (reference or clean-room install):
# starts the game like the launcher does, opens a CPU-vs-CPU custom fight with
# upscaled backgrounds, captures frames and summarises the runtime log.
#   powershell -ExecutionPolicy Bypass -File tools\test_installed_game.ps1 -Install <dir> -Out <dir>
param([Parameter(Mandatory)][string]$Install,[Parameter(Mandatory)][string]$Out,[int]$Seconds=40,
      [string]$Fight='2 5 1 1 0 0 2',[string]$Resolution='1920x1080')
$ErrorActionPreference='Stop'
if(Test-Path $Out){Remove-Item -Recurse -Force $Out}
New-Item -ItemType Directory $Out | Out-Null
$user="$Out\userdata"; New-Item -ItemType Directory $user | Out-Null
Set-Content "$Out\command.txt" ''
$env:VF2_CUSTOM_FIGHT_TEST="$Out\command.txt"
$env:VF2_CUSTOM_FIGHT_TEST_REPORT="$Out\state.txt"
$env:VF2_GPU_CAPTURE="$Out\captures"
$env:VF2_GPU_CAPTURE_DELAY_MS='26000'
$env:VF2_GPU_CAPTURE_INTERVAL_MS='3000'
$env:VF2_GPU_CAPTURE_COUNT='5'
$arguments="--game_data_root `"$Install\data\game`" --user_data_root `"$user`" --input_backend xinput --license_mask 1 --user_language 1 "+
  "--vf2_output_resolution $Resolution --vf2_no_intro=true --vf2_background_replacements=true --no-fullscreen --window_width 960 --window_height 540 --log_file `"$Out\game.log`""
try {$process=Start-Process "$Install\data\engine\vf2.exe" -WorkingDirectory "$Install\data\engine" -ArgumentList $arguments -WindowStyle Hidden -PassThru}
finally {Remove-Item Env:VF2_CUSTOM_FIGHT_TEST,Env:VF2_CUSTOM_FIGHT_TEST_REPORT,Env:VF2_GPU_CAPTURE,Env:VF2_GPU_CAPTURE_DELAY_MS,Env:VF2_GPU_CAPTURE_INTERVAL_MS,Env:VF2_GPU_CAPTURE_COUNT}
$watch=[Diagnostics.Stopwatch]::StartNew()
try {
  # Start the fight only once the Main Menu is up, so every run takes the same path.
  while($watch.Elapsed.TotalSeconds -lt 60 -and !$process.HasExited -and
        !(Select-String -LiteralPath "$Out\game.log" -Pattern 'Main Menu music' -SimpleMatch -Quiet -ErrorAction SilentlyContinue)){Start-Sleep -Milliseconds 200}
  Start-Sleep -Seconds 2
  Set-Content "$Out\command.txt" $Fight
  $Seconds+= [int]$watch.Elapsed.TotalSeconds
  while($watch.Elapsed.TotalSeconds -lt $Seconds -and !$process.HasExited){Start-Sleep -Milliseconds 100}
  $alive=!$process.HasExited
} finally {if(!$process.HasExited){Stop-Process -InputObject $process}}
$log=Get-Content "$Out\game.log"
$pick={param($pattern) @($log | Where-Object {$_ -match $pattern} | ForEach-Object {($_ -replace '^\[[^\]]*\] \[[^\]]*\] \[[^\]]*\] \[[^\]]*\] ','')})}
[ordered]@{
  alive_at_end=$alive
  errors=@($log | Where-Object {$_ -match '\[(error|critical)\]'} | ForEach-Object {($_ -replace '^.*\] ','')} | Group-Object | ForEach-Object {"$($_.Count)x $($_.Name)"})
  milestones=(& $pick 'Push Start flow|Main Menu music|Custom Fight (started|applied|queued)|replacement (loaded|fills)|music')
  state=if(Test-Path "$Out\state.txt"){Get-Content "$Out\state.txt" -Raw}else{''}
  captures=@(Get-ChildItem "$Out\captures" -File -ErrorAction SilentlyContinue | ForEach-Object Name)
} | ConvertTo-Json -Depth 4 | Tee-Object "$Out\summary.json"
