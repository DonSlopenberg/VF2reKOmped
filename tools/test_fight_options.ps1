param([string]$Name='native-options',[int]$Seconds=110)
$ErrorActionPreference='Stop'
$vf2Root=Split-Path $PSScriptRoot -Parent
$vf2Folder=Join-Path $vf2Root ('analysis/fight-options-test-'+$Name)
New-Item -ItemType Directory -Force $vf2Folder | Out-Null
Copy-Item -LiteralPath "$vf2Root/analysis/startup-buttons/userdata" -Destination "$vf2Folder/userdata" -Recurse
function Write-OptionCommand([string]$Value) {
 for($vf2Attempt=0;$vf2Attempt -lt 40;$vf2Attempt++) {
  try {[IO.File]::WriteAllText("$vf2Folder/command.txt",$Value);return}
  catch [IO.IOException] {Start-Sleep -Milliseconds 5}
 }
 throw 'Fight option command file remained locked.'
}
Write-OptionCommand ''
$env:VF2_CUSTOM_FIGHT_TEST="$vf2Folder/command.txt"
$env:VF2_CUSTOM_FIGHT_TEST_REPORT="$vf2Folder/state.txt"
$env:VF2_GPU_CAPTURE="$vf2Folder/captures"
$env:VF2_GPU_CAPTURE_DELAY_MS='8000'
$env:VF2_GPU_CAPTURE_INTERVAL_MS='10000'
$env:VF2_GPU_CAPTURE_COUNT='10'
try {$vf2Process=Start-Process "$vf2Root/data/engine/vf2_fight_candidate.exe" -WorkingDirectory "$vf2Root/data/engine" -WindowStyle Hidden -PassThru -ArgumentList "--game_data_root `"$vf2Root/data/game`" --user_data_root `"$vf2Folder/userdata`" --license_mask 1 --vf2_output_resolution 3840x2160 --window_width 960 --window_height 540 --no-fullscreen --log_file `"$vf2Folder/runtime.log`""}
finally {Remove-Item Env:VF2_CUSTOM_FIGHT_TEST,Env:VF2_CUSTOM_FIGHT_TEST_REPORT,Env:VF2_GPU_CAPTURE,Env:VF2_GPU_CAPTURE_DELAY_MS,Env:VF2_GPU_CAPTURE_INTERVAL_MS,Env:VF2_GPU_CAPTURE_COUNT}
$vf2Events=@(@(14,'0 1 1 1 1 1 0 0 2 15'),@(20,'0 1 1 1 1 1 0 1 3 30'),@(26,'0 1 1 1 1 1 0 2 4 45'),@(32,'0 1 1 1 1 1 0 3 5 60'),@(38,'restart'),@(44,'0 1 1 1 1 1 0 -1 -1 -1'),@(50,'9 9 1 1 1 1 10 3 2 15'))
$vf2Watch=[Diagnostics.Stopwatch]::StartNew()
try {
 foreach($vf2Event in $vf2Events){
  while($vf2Watch.Elapsed.TotalSeconds -lt $vf2Event[0] -and !$vf2Process.HasExited){Start-Sleep -Milliseconds 50}
  if($vf2Process.HasExited){throw 'Game exited during options test'}
  Write-OptionCommand $vf2Event[1]
 }
 while($vf2Watch.Elapsed.TotalSeconds -lt $Seconds -and !$vf2Process.HasExited){Start-Sleep -Milliseconds 250}
 @{alive_at_end=(!$vf2Process.HasExited);seconds=$vf2Watch.Elapsed.TotalSeconds} | ConvertTo-Json | Set-Content "$vf2Folder/result.json"
} finally {if(!$vf2Process.HasExited){Stop-Process -InputObject $vf2Process}}
Write-Output $vf2Folder

