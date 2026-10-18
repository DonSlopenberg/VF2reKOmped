param([string]$Name='flow',[int]$Seconds=105,[switch]$ExitRestart)
$ErrorActionPreference='Stop'
$vf2Root=Split-Path $PSScriptRoot -Parent
$vf2Folder=Join-Path $vf2Root ('analysis/fight-flow-test-'+$Name)
New-Item -ItemType Directory -Force $vf2Folder | Out-Null
Copy-Item -LiteralPath "$vf2Root/analysis/startup-buttons/userdata" -Destination "$vf2Folder/userdata" -Recurse
Set-Content "$vf2Folder/command.txt" ''
Set-Content "$vf2Folder/input.txt" '0 0'
$env:VF2_CUSTOM_FIGHT_TEST="$vf2Folder/command.txt"
$env:VF2_CUSTOM_FIGHT_TEST_REPORT="$vf2Folder/state.txt"
$env:VF2_AUTOMATED_INPUT="$vf2Folder/input.txt"
$env:VF2_GPU_CAPTURE="$vf2Folder/captures"
$env:VF2_GPU_CAPTURE_DELAY_MS='6000'
$env:VF2_GPU_CAPTURE_INTERVAL_MS='2000'
$env:VF2_GPU_CAPTURE_COUNT='48'
try {
  $vf2Process=Start-Process "$vf2Root/data/engine/vf2_fight_candidate.exe" -WorkingDirectory "$vf2Root/data/engine" -WindowStyle Hidden -PassThru -ArgumentList "--game_data_root `"$vf2Root/data/game`" --user_data_root `"$vf2Folder/userdata`" --license_mask 1 --video_mode_width 1920 --video_mode_height 1080 --window_width 960 --window_height 540 --no-fullscreen --log_file `"$vf2Folder/runtime.log`""
} finally {Remove-Item Env:VF2_CUSTOM_FIGHT_TEST,Env:VF2_CUSTOM_FIGHT_TEST_REPORT,Env:VF2_AUTOMATED_INPUT,Env:VF2_GPU_CAPTURE,Env:VF2_GPU_CAPTURE_DELAY_MS,Env:VF2_GPU_CAPTURE_INTERVAL_MS,Env:VF2_GPU_CAPTURE_COUNT}
$vf2Events=@(@(10,'command','0 1 0 0 1 1 0'),@(17,'input','10 0'),@(17.3,'input','0 0'),@(21,'input','1000 0'),@(21.3,'input','0 0'),@(25,'command','settings-open'),@(29,'command','settings-close'),@(33,'input','0 10'),@(33.3,'input','0 0'),@(34,'command','settings-open'),@(36,'command','settings-close'),@(37,'input','0 1000'),@(37.3,'input','0 0'),@(42,'command','9 9 1 1 1 1 10'),@(48,'input','10 0'),@(48.3,'input','0 0'),@(52,'input','1000 0'),@(52.3,'input','0 0'))
if($ExitRestart){
 $vf2Events=@(@(8,'command','0 1 0 0 1 1 0'),@(14,'input','10 0'),@(14.3,'input','0 0'),@(17,'command','restart'),@(23,'input','10 0'),@(23.3,'input','0 0'))
 for($vf2Down=0;$vf2Down -lt 5;$vf2Down++){
   $vf2Events+=,@((25+$vf2Down*0.5),'input','2 0')
   $vf2Events+=,@((25.2+$vf2Down*0.5),'input','0 0')
 }
 $vf2Events+=@(@(28,'input','1000 0'),@(28.3,'input','0 0'),@(31,'input','1 0'),@(31.3,'input','0 0'),@(32,'input','1000 0'),@(32.3,'input','0 0'),@(36,'command','0 1 0 0 1 1 0'),@(42,'input','0 4'),@(44,'input','0 0'))
}
$vf2Watch=[Diagnostics.Stopwatch]::StartNew()
try {
 foreach($vf2Event in $vf2Events){
   while($vf2Watch.Elapsed.TotalSeconds -lt $vf2Event[0] -and !$vf2Process.HasExited){Start-Sleep -Milliseconds 50}
   if($vf2Process.HasExited){throw 'Game exited during flow test'}
   Set-Content "$vf2Folder/$($vf2Event[1]).txt" $vf2Event[2]
 }
 while($vf2Watch.Elapsed.TotalSeconds -lt $Seconds -and !$vf2Process.HasExited){Start-Sleep -Milliseconds 250}
 @{alive_at_end=(!$vf2Process.HasExited);seconds=$vf2Watch.Elapsed.TotalSeconds} | ConvertTo-Json | Set-Content "$vf2Folder/result.json"
} finally {if(!$vf2Process.HasExited){Stop-Process -InputObject $vf2Process}}
Write-Output $vf2Folder
