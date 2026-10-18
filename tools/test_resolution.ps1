# Starts an installation at one output resolution (as the launcher does) and
# captures frames and the window/backbuffer log lines.
#   powershell -ExecutionPolicy Bypass -File tools\test_resolution.ps1 -Install <dir> -Out <dir> -Resolution 2560x1440 [-Fullscreen]
param([Parameter(Mandatory)][string]$Install,[Parameter(Mandatory)][string]$Out,
      [string]$Resolution='2560x1440',[switch]$Fullscreen,[int]$Seconds=22)
$ErrorActionPreference='Stop'
if(Test-Path $Out){Remove-Item -Recurse -Force $Out}
New-Item -ItemType Directory "$Out\userdata" -Force | Out-Null
$w,$h=$Resolution -split 'x'
$env:VF2_GPU_CAPTURE="$Out\captures"
$env:VF2_GPU_CAPTURE_DELAY_MS='12000'
$env:VF2_GPU_CAPTURE_INTERVAL_MS='3000'
$env:VF2_GPU_CAPTURE_COUNT='3'
$mode=if($Fullscreen){'--fullscreen'}else{"--no-fullscreen --window_width $w --window_height $h"}
$arguments="--game_data_root `"$Install\data\game`" --user_data_root `"$Out\userdata`" --input_backend xinput --license_mask 1 --user_language 1 "+
  "--vf2_output_resolution $Resolution --vf2_no_intro=true $mode --log_file `"$Out\game.log`""
try {$process=Start-Process "$Install\data\engine\vf2.exe" -WorkingDirectory "$Install\data\engine" -ArgumentList $arguments -PassThru}
finally {Remove-Item Env:VF2_GPU_CAPTURE,Env:VF2_GPU_CAPTURE_DELAY_MS,Env:VF2_GPU_CAPTURE_INTERVAL_MS,Env:VF2_GPU_CAPTURE_COUNT}
Start-Sleep -Seconds $Seconds
if(!$process.HasExited){Stop-Process -InputObject $process}
Get-Content "$Out\game.log" | Where-Object {$_ -match 'VF2 (render selection|GPU settings|launch|window|native backbuffer|physical display|startup fullscreen)|presenter|swap|surface'} |
  ForEach-Object {$_ -replace '^.*?\] (\[[^\]]*\] ){0,2}',''}
Get-ChildItem "$Out\captures" -Filter *.txt -ErrorAction SilentlyContinue | ForEach-Object {"capture $($_.BaseName): $(Get-Content $_.FullName)"}
