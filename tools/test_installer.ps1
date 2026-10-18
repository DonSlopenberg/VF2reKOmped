# Tests the clean-room package end to end from a freshly extracted ZIP.
#   powershell -ExecutionPolicy Bypass -File tools\test_installer.ps1 -Work <empty folder>
# Needs "Virtua Fighter 2.rar" and/or the STFS content file in the repository root.
param([Parameter(Mandatory)][string]$Work,[string]$Rar='',[string]$Stfs='')
$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot -Parent
$Zip = Get-ChildItem "$Root\dist\VF2-reKOmped-*-cleanroom.zip" | Select-Object -First 1
$Reference = $Zip.FullName -replace '-cleanroom\.zip$', ''
if (-not $Rar) { $Rar = Join-Path $Root 'Virtua Fighter 2.rar' }
if (-not $Stfs) { $Stfs = Join-Path $Root '65D8FAC10B21FA185AAB93607663594CB568C78658' }
if (-not (Test-Path -LiteralPath $Stfs)) { throw "STFS source not found: $Stfs (pass -Stfs)" }
if (Test-Path $Work) { Remove-Item -Recurse -Force $Work }
New-Item -ItemType Directory $Work | Out-Null
$env:VF2_NO_PAUSE = '1'
$results = @()
function Record($name, $ok, $detail) { $script:results += [pscustomobject]@{ Test = $name; Pass = [bool]$ok; Detail = $detail } }
function NewPackage($name) { $p = Join-Path $Work $name; Expand-Archive $Zip.FullName $p; $p }
function Install($pkg, $source, $dest) {
  $lines = cmd /c "`"$pkg\INSTALL.cmd`" `"$source`" `"$dest`"" 2>&1 | ForEach-Object { "$_" }
  return @{ Code = $LASTEXITCODE; Text = ($lines -join "`n") }
}
function Tree($dir) {
  $map = @{}
  Get-ChildItem -LiteralPath $dir -Recurse -Force | ForEach-Object {
    $rel = $_.FullName.Substring($dir.Length + 1) -replace '\\', '/'
    $map[$rel] = if ($_.PSIsContainer) { 'DIR' } else { (Get-FileHash -LiteralPath $_.FullName).Hash }
  }
  return $map
}
function SameTree($a, $b) {
  $diff = @()
  foreach ($k in $a.Keys) { if (-not $b.ContainsKey($k)) { $diff += "missing $k" } elseif ($a[$k] -ne $b[$k]) { $diff += "different $k" } }
  foreach ($k in $b.Keys) { if (-not $a.ContainsKey($k)) { $diff += "extra $k" } }
  return $diff
}
$refTree = Tree $Reference

# 1-2: both source formats, compared file by file with the reference release
foreach ($case in @(@{ Name = 'RAR'; Src = $Rar }, @{ Name = 'STFS'; Src = $Stfs })) {
  if (-not (Test-Path -LiteralPath $case.Src)) { Write-Host "SKIPPED install from $($case.Name): source not present ($($case.Src))"; continue }
  $pkg = NewPackage "pkg-$($case.Name)"
  $r = Install $pkg $case.Src "$pkg\Virtua Fighter 2"
  $diff = if ($r.Code -eq 0) { SameTree $refTree (Tree "$pkg\Virtua Fighter 2") } else { @('install failed') }
  Record "install from $($case.Name)" ($r.Code -eq 0 -and $diff.Count -eq 0) ("exit $($r.Code); " + $(if ($diff.Count) { $diff -join ', ' } else { 'identical to reference' }))
  $left = @(Get-ChildItem $pkg -Force -Directory | Where-Object Name -like '.vf2-install*')
  Record "no temporary files ($($case.Name))" ($left.Count -eq 0) "$($left.Count) left"
}

# 3: source found automatically next to INSTALL.cmd, no arguments
$pkg = NewPackage 'pkg-auto'
Copy-Item -LiteralPath $Stfs -Destination $pkg
$lines = cmd /c "`"$pkg\INSTALL.cmd`"" 2>&1 | ForEach-Object { "$_" }
Record 'auto-detect source, no arguments' ($LASTEXITCODE -eq 0 -and (Test-Path "$pkg\Virtua Fighter 2\data\game\default.xex")) "exit $LASTEXITCODE"

# 4: invalid sources
$neg = NewPackage 'pkg-neg'
$orig = [IO.File]::ReadAllBytes($Stfs)
function Variant($name, [byte[]]$bytes) { $p = Join-Path $Work $name; [IO.File]::WriteAllBytes($p, $bytes); $p }
$bad = [ordered]@{}
$bad['missing source']           = @{ Src = (Join-Path $Work 'nope.rar'); Expect = 'Source file not found' }
$bad['not a game file']          = @{ Src = (Variant 'notes.txt' ([Text.Encoding]::ASCII.GetBytes('not a game'))); Expect = 'Unrecognised source' }
$t = [byte[]]$orig.Clone(); [Array]::Resize([ref]$t, 30000000)
$bad['truncated package']        = @{ Src = (Variant 'trunc.bin' $t); Expect = 'truncated' }
$t = [byte[]]$orig.Clone(); $t[40000000] = $t[40000000] -bxor 0xFF
$bad['damaged package']          = @{ Src = (Variant 'flip.bin' $t); Expect = 'fails its SHA-1 check' }
$t = [byte[]]$orig.Clone(); $t[0x363] = 0x9D
$bad['other game']               = @{ Src = (Variant 'title.bin' $t); Expect = 'Wrong game' }
$t = [byte[]]$orig.Clone(); [Text.Encoding]::ASCII.GetBytes('CON ').CopyTo($t, 0)
$bad['CON package']              = @{ Src = (Variant 'conpkg.bin' $t); Expect = 'CON/PIRS package' }
if (Test-Path -LiteralPath $Rar) {
  $t = [IO.File]::ReadAllBytes($Rar); [Array]::Resize([ref]$t, 20000000)
  $bad['damaged RAR']            = @{ Src = (Variant 'trunc.rar' $t); Expect = 'Could not extract the RAR' }
}
foreach ($k in $bad.Keys) {
  $r = Install $neg $bad[$k].Src "$Work\neg-out"
  $ok = $r.Code -ne 0 -and $r.Text -like "*$($bad[$k].Expect)*" -and -not (Test-Path "$Work\neg-out")
  $msg = ($r.Text -split "`n" | Where-Object { $_ -like '*FAILED*' } | Select-Object -First 1)
  Record "rejects: $k" $ok "exit $($r.Code); $msg"
}

# 5: foreign destination is never touched
New-Item -ItemType Directory "$Work\foreign" | Out-Null; Set-Content "$Work\foreign\x.txt" 'mine'
$r = Install $neg $Stfs "$Work\foreign"
Record 'refuses non-empty foreign destination' ($r.Code -ne 0 -and (Test-Path "$Work\foreign\x.txt") -and @(Get-ChildItem "$Work\foreign").Count -eq 1) "exit $($r.Code)"

# 6: incomplete installer package
$broken = NewPackage 'pkg-broken'
Remove-Item "$broken\payload\data\engine\rexgpu-xenos.dll"
$r = Install $broken $Stfs "$Work\broken-out"
Record 'detects incomplete payload' ($r.Code -ne 0 -and $r.Text -like '*missing: data/engine/rexgpu-xenos.dll*' -and -not (Test-Path "$Work\broken-out")) "exit $($r.Code)"

# 7: reinstall keeps saves and settings, repairs binaries
$inst = "$Work\reinstall"
$r = Install $neg $(if (Test-Path -LiteralPath $Rar) { $Rar } else { $Stfs }) $inst
New-Item -ItemType Directory -Force "$inst\data\userdata\save" | Out-Null
Set-Content "$inst\data\userdata\save\slot.dat" 'my save'
Add-Content "$inst\data\engine\vf2.toml" 'vf2_background_blur = 3'
Set-Content "$inst\data\engine\vf2.exe" 'broken'
$r2 = Install $neg $Stfs $inst
$keptSave = (Get-Content "$inst\data\userdata\save\slot.dat") -eq 'my save'
$keptToml = [bool](Select-String -LiteralPath "$inst\data\engine\vf2.toml" -Pattern 'vf2_background_blur = 3' -SimpleMatch)
$repaired = (Get-FileHash "$inst\data\engine\vf2.exe").Hash -eq $refTree['data/engine/vf2.exe']
$siblings = @(Get-ChildItem $Work -Force -Directory | Where-Object Name -like 'reinstall.old*').Count
Record 'reinstall keeps saves/settings, repairs exe' ($r.Code -eq 0 -and $r2.Code -eq 0 -and $keptSave -and $keptToml -and $repaired -and $siblings -eq 0) "save=$keptSave settings=$keptToml exe=$repaired leftovers=$siblings"

$results | Format-Table -AutoSize -Wrap | Out-String -Width 220
$results | ConvertTo-Json | Set-Content (Join-Path $Work 'installer_test_results.json')
if ($results | Where-Object { -not $_.Pass }) { exit 1 }
