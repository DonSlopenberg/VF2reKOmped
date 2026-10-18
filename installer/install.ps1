<#
Virtua Fighter 2 reKOmped - installer.

Builds a ready-to-run installation from the user's own copy of the
Xbox 360 XBLA package and the modified binaries in .\payload.

  INSTALL.cmd [source] [destination]

source       "Virtua Fighter 2.rar" or the STFS content file
             65D8FAC10B21FA185AAB93607663594CB568C78658.
             Default: searched next to INSTALL.cmd (and one folder above).
destination  Default: ".\Virtua Fighter 2" next to INSTALL.cmd.

The original files are read only from the source; nothing is downloaded.
STFS reading follows Xenia's stfs_xbox.h / stfs_container_device.cc (BSD).
Re-running over an earlier installation keeps data\userdata (saves,
achievements) and data\engine\vf2.toml (settings).
#>
param([string]$Source=$env:VF2_SOURCE, [string]$Destination=$env:VF2_DEST)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2

$PackageRoot = Split-Path $PSScriptRoot -Parent
$Payload = Join-Path $PackageRoot 'payload'
$ContentName = '65D8FAC10B21FA185AAB93607663594CB568C78658'
$Expected = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'game_files.json') | ConvertFrom-Json

function Fail([string]$Message) { throw [System.Exception]::new($Message) }
function Step([string]$Message) { Write-Host "==> $Message" }
function Sha256Hex([byte[]]$Bytes, [int]$Offset = 0, [int]$Count = -1) {
  if ($Count -lt 0) { $Count = $Bytes.Length - $Offset }
  $sha = [System.Security.Cryptography.SHA256]::Create()
  try { return ([System.BitConverter]::ToString($sha.ComputeHash($Bytes, $Offset, $Count)) -replace '-', '').ToLowerInvariant() }
  finally { $sha.Dispose() }
}
function FileSha256([string]$Path) { return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToLowerInvariant() }
function RunTool([string]$Exe, [string[]]$Arguments) {
  # Runs a native tool and returns @{Code; Out; Err} without PowerShell's
  # stderr-as-error behaviour.
  $info = New-Object System.Diagnostics.ProcessStartInfo
  $info.FileName = $Exe
  $info.Arguments = ($Arguments | ForEach-Object { '"' + ($_ -replace '"', '\"') + '"' }) -join ' '
  $info.UseShellExecute = $false
  $info.RedirectStandardOutput = $true
  $info.RedirectStandardError = $true
  $info.CreateNoWindow = $true
  $process = [System.Diagnostics.Process]::Start($info)
  $errTask = $process.StandardError.ReadToEndAsync()
  $out = $process.StandardOutput.ReadToEnd()
  $process.WaitForExit()
  return @{ Code = $process.ExitCode; Out = $out; Err = $errTask.Result }
}

# ---------------------------------------------------------------- source --
function FindSource {
  if ($Source) {
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf)) { Fail "Source file not found: $Source" }
    return (Resolve-Path -LiteralPath $Source).Path
  }
  $names = @('Virtua Fighter 2.rar', $ContentName,
             "5841129C\000D0000\$ContentName", "Virtua Fighter 2\5841129C\000D0000\$ContentName")
  foreach ($folder in @($PackageRoot, (Split-Path $PackageRoot -Parent))) {
    if (-not $folder) { continue }
    foreach ($name in $names) {
      $candidate = Join-Path $folder $name
      if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }
  }
  Fail ("No game source found. Provide your own copy of the Xbox 360 XBLA package:`n" +
        "  INSTALL.cmd ""<path>\Virtua Fighter 2.rar""`n" +
        "  INSTALL.cmd ""<path>\$ContentName""`n" +
        "or place one of them next to INSTALL.cmd.")
}

function ExtractFromRar([string]$Rar, [string]$WorkDir) {
  $tools = @()
  $tar = Join-Path $env:SystemRoot 'System32\tar.exe'
  if (Test-Path -LiteralPath $tar) { $tools += @{ Name = 'tar'; Exe = $tar } }
  foreach ($root in @($env:ProgramFiles, ${env:ProgramFiles(x86)})) {
    if ($root -and (Test-Path -LiteralPath (Join-Path $root '7-Zip\7z.exe'))) {
      $tools += @{ Name = '7z'; Exe = (Join-Path $root '7-Zip\7z.exe') }
    }
  }
  if ($tools.Count -eq 0) {
    Fail "Cannot read RAR files: neither Windows tar.exe (Windows 10 1803+) nor 7-Zip is available. Extract $ContentName from the archive yourself and pass that file instead."
  }
  $problems = @()
  foreach ($tool in $tools) {
    if ($tool.Name -eq 'tar') {
      $list = RunTool $tool.Exe @('-tf', $Rar)
      if ($list.Code -ne 0) { $problems += "tar: $($list.Err.Trim())"; continue }
      $member = @($list.Out -split "`r?`n" | Where-Object { $_ -match "(^|/)$ContentName$" })
      if ($member.Count -eq 0) { Fail "The RAR archive does not contain $ContentName. Is it the Virtua Fighter 2 XBLA package?" }
      if ($member.Count -gt 1) { Fail "The RAR archive contains $ContentName more than once." }
      $run = RunTool $tool.Exe @('-xf', $Rar, '-C', $WorkDir, $member[0])
      if ($run.Code -ne 0) { $problems += "tar: $($run.Err.Trim())"; continue }
    } else {
      $run = RunTool $tool.Exe @('e', '-y', "-o$WorkDir", $Rar, $ContentName, '-r')
      if ($run.Code -ne 0) { $problems += "7z: $($run.Err.Trim())"; continue }
    }
    $found = @(Get-ChildItem -LiteralPath $WorkDir -Recurse -File -Filter $ContentName)
    if ($found.Count -eq 1) { Step "Extracted $ContentName with $($tool.Name)"; return $found[0].FullName }
    $problems += "$($tool.Name): $ContentName was not extracted"
  }
  Fail ("Could not extract the RAR archive (damaged, password-protected or unsupported):`n  " + ($problems -join "`n  "))
}

# ------------------------------------------------------------------ STFS --
function ReadStfs([byte[]]$Data) {
  # Returns the files of a read-only, single-file LIVE/STFS package after
  # checking its complete SHA-1 hash tree.
  function U([int]$Pos, [int]$Size) {
    if ($Pos -lt 0 -or $Pos + $Size -gt $Data.Length) { Fail ("Invalid package: read outside the file at 0x{0:X}." -f $Pos) }
    $v = [uint64]0; for ($i = 0; $i -lt $Size; $i++) { $v = ($v -shl 8) -bor $Data[$Pos + $i] }; return $v
  }
  function UL([byte[]]$B, [int]$Pos, [int]$Size) {
    $v = [uint64]0; for ($i = $Size - 1; $i -ge 0; $i--) { $v = ($v -shl 8) -bor $B[$Pos + $i] }; return $v
  }
  if ($Data.Length -lt 0x1000 -or [System.Text.Encoding]::ASCII.GetString($Data, 0, 4) -ne 'LIVE') {
    if ($Data.Length -ge 4 -and [System.Text.Encoding]::ASCII.GetString($Data, 0, 4) -in @('CON ', 'PIRS')) {
      Fail 'This is a CON/PIRS package, not the XBLA LIVE package of Virtua Fighter 2.'
    }
    Fail 'The source is not an Xbox 360 LIVE/STFS package.'
  }
  $titleId = '{0:X8}' -f (U 0x360 4)
  if ($titleId -ne $Expected.title_id) { Fail "Wrong game: package title ID $titleId, Virtua Fighter 2 is $($Expected.title_id)." }
  if ((U 0x3A9 4) -ne 0) { Fail 'Unsupported package: multi-file STFS.' }
  if ((U 0x379 1) -ne 0x24 -or -not ((U 0x37B 1) -band 1)) { Fail 'Unsupported package: not a read-only STFS hash layout.' }
  $total = [int](U 0x395 4)
  if ($total -le 170 -or $total -gt 28900) { Fail "Unsupported package: $total data blocks." }
  $base = [int]((U 0x340 4) + 4095) -band -bnot 4095
  $sha1 = [System.Security.Cryptography.SHA1]::Create()
  $blockOffset = { param([int]$b)
    if ($b -lt 0 -or $b -ge $total) { Fail "Invalid package: block $b out of range." }
    $p = $b + [math]::Floor($b / 170) + 1
    if ($b -ge 170) { $p += [math]::Floor($b / 28900) + 1 }
    $off = $base + $p * 4096
    if ($off + 4096 -gt $Data.Length) { Fail 'The package is truncated.' }
    [int]$off }
  $tableOffset = { param([int]$g) $p = 0; if ($g -gt 0) { $p = $g * 171 + 1 }; [int]($base + $p * 4096) }
  $equal = { param([byte[]]$h, [int]$at) [Convert]::ToBase64String($h) -eq [Convert]::ToBase64String($Data, $at, 20) }

  $top = $base + 171 * 4096
  if ($top + 4096 -gt $Data.Length) { Fail 'The package is truncated.' }
  if (-not (& $equal $sha1.ComputeHash($Data, $top, 4096) 0x381)) { Fail 'The package is damaged: root hash mismatch.' }
  $groups = [math]::Ceiling($total / 170)
  for ($g = 0; $g -lt $groups; $g++) {
    $t = & $tableOffset $g
    if ($t + 4096 -gt $Data.Length) { Fail 'The package is truncated.' }
    if (-not (& $equal $sha1.ComputeHash($Data, $t, 4096) ($top + $g * 24))) { Fail "The package is damaged: hash table $g mismatch." }
  }
  $record = { param([int]$b) [int]((& $tableOffset ([math]::Floor($b / 170))) + ($b % 170) * 24) }
  $nextBlock = { param([int]$b) $r = & $record $b; [int](([int]$Data[$r + 21] -shl 16) -bor ([int]$Data[$r + 22] -shl 8) -bor $Data[$r + 23]) }
  for ($b = 0; $b -lt $total; $b++) {
    if (-not (& $equal $sha1.ComputeHash($Data, (& $blockOffset $b), 4096) (& $record $b))) {
      Fail "The package is damaged: data block $b fails its SHA-1 check."
    }
  }

  $entries = New-Object System.Collections.ArrayList
  $tableCount = [int](UL $Data 0x37C 2)
  $tableBlock = [int](UL $Data 0x37E 3)
  $visited = @{}
  for ($n = 0; $n -lt $tableCount; $n++) {
    if ($visited.ContainsKey($tableBlock)) { Fail 'Invalid package: directory chain loops.' }
    $visited[$tableBlock] = $true
    $at = & $blockOffset $tableBlock
    for ($i = 0; $i -lt 64; $i++) {
      $row = $at + $i * 64
      if ($Data[$row] -eq 0) { break }
      $length = $Data[$row + 40] -band 63
      if ($length -lt 1 -or $length -gt 40) { Fail 'Invalid package: bad file name.' }
      $name = [System.Text.Encoding]::ASCII.GetString($Data, $row, $length)
      if ($name -in @('.', '..') -or $name.IndexOfAny([char[]]'/\:') -ge 0) { Fail "Invalid package: unsafe file name '$name'." }
      $parent = [int](([int]$Data[$row + 50] -shl 8) -bor $Data[$row + 51])
      if ($parent -ne 65535 -and ($parent -ge $entries.Count -or -not $entries[$parent].Directory)) { Fail 'Invalid package: bad parent directory.' }
      $path = if ($parent -eq 65535) { $name } else { $entries[$parent].Path + '/' + $name }
      [void]$entries.Add([pscustomobject]@{
        Path = $path; Directory = [bool]($Data[$row + 40] -band 128)
        Size = [int64](U ($row + 52) 4); Start = [int](UL $Data ($row + 47) 3); Blocks = [int](UL $Data ($row + 44) 3) })
    }
    $tableBlock = & $nextBlock $tableBlock
  }
  if ($tableBlock -ne 0xFFFFFF) { Fail 'Invalid package: directory chain longer than its header.' }

  $files = @()
  foreach ($e in $entries) {
    if ($e.Directory) { continue }
    $stream = New-Object System.IO.MemoryStream
    $block = $e.Start; $remaining = $e.Size; $seen = @{}
    while ($remaining -gt 0) {
      if ($block -eq 0xFFFFFF -or $seen.ContainsKey($block)) { Fail "Invalid package: broken block chain in $($e.Path)." }
      $seen[$block] = $true
      $count = [int][math]::Min($remaining, 4096)
      $stream.Write($Data, (& $blockOffset $block), $count)
      $remaining -= $count
      $block = & $nextBlock $block
    }
    if ($seen.Count -ne $e.Blocks -or ($seen.Count -gt 0 -and $block -ne 0xFFFFFF)) { Fail "Invalid package: block count mismatch in $($e.Path)." }
    $files += [pscustomobject]@{ Path = $e.Path; Bytes = $stream.ToArray() }
  }
  return $files
}

# ------------------------------------------------------------------ main --
$staging = $null
$exitCode = 1
try {
  if (-not (Test-Path -LiteralPath (Join-Path $Payload 'data\engine\vf2.exe'))) { Fail "The installer package is incomplete: $Payload\data\engine\vf2.exe is missing." }
  $reference = Get-Content -Raw -LiteralPath (Join-Path $Payload 'MANIFEST.json') | ConvertFrom-Json
  if (-not $Destination) { $Destination = Join-Path $PackageRoot 'Virtua Fighter 2' }
  $Destination = [System.IO.Path]::GetFullPath($Destination)
  $previous = $false
  if (Test-Path -LiteralPath $Destination) {
    $previous = (Test-Path -LiteralPath (Join-Path $Destination 'Virtua Fighter 2.exe')) -and
                (Test-Path -LiteralPath (Join-Path $Destination 'data\engine\vf2.exe'))
    if (-not $previous -and @(Get-ChildItem -LiteralPath $Destination -Force).Count -gt 0) {
      Fail "Destination exists and is not a Virtua Fighter 2 installation: $Destination"
    }
  }
  $parentDir = Split-Path $Destination -Parent
  New-Item -ItemType Directory -Force -Path $parentDir | Out-Null
  $drive = New-Object System.IO.DriveInfo ([System.IO.Path]::GetPathRoot($Destination))
  if ($drive.AvailableFreeSpace -lt 400MB) { Fail ("Not enough free space on {0}: 400 MB needed." -f $drive.Name) }

  $staging = Join-Path $parentDir ('.vf2-install-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
  New-Item -ItemType Directory -Path $staging | Out-Null

  $sourcePath = FindSource
  Step "Source: $sourcePath"
  $header = New-Object byte[] 4
  $fs = [System.IO.File]::OpenRead($sourcePath)
  try { [void]$fs.Read($header, 0, 4) } finally { $fs.Dispose() }
  $magic = [System.Text.Encoding]::ASCII.GetString($header)
  if ($magic -eq 'Rar!') {
    $work = Join-Path $staging '_source'
    New-Item -ItemType Directory -Path $work | Out-Null
    $contentPath = ExtractFromRar $sourcePath $work
  } elseif ($magic -in @('LIVE', 'CON ', 'PIRS')) {
    $contentPath = $sourcePath
  } else {
    Fail "Unrecognised source file (neither RAR nor Xbox 360 package): $sourcePath"
  }

  Step 'Validating the XBLA package'
  $data = [System.IO.File]::ReadAllBytes($contentPath)
  $sourceHash = Sha256Hex $data
  $files = ReadStfs $data
  if ($sourceHash -ne $Expected.source_sha256) {
    Fail ("This Virtua Fighter 2 package is intact but a different revision (SHA-256 $sourceHash). " +
          "The installer was built and tested for $($Expected.source_sha256) ($($Expected.source_size) bytes).")
  }
  $want = @{}; foreach ($f in $Expected.files) { $want[$f.path] = $f }
  if ($files.Count -ne $want.Count) { Fail "Unexpected package contents: $($files.Count) files, expected $($want.Count)." }

  Step "Extracting $($files.Count) original files"
  foreach ($f in $files) {
    $info = $want[$f.Path]
    if (-not $info) { Fail "Unexpected file in package: $($f.Path)" }
    if ($f.Bytes.Length -ne $info.size -or (Sha256Hex $f.Bytes) -ne $info.sha256) { Fail "Original file failed validation: $($f.Path)" }
    $target = Join-Path $staging ('data\game\' + ($f.Path -replace '/', '\'))
    New-Item -ItemType Directory -Force -Path (Split-Path $target -Parent) | Out-Null
    [System.IO.File]::WriteAllBytes($target, $f.Bytes)
  }
  $data = $null; $files = $null
  if (Test-Path -LiteralPath (Join-Path $staging '_source')) { Remove-Item -LiteralPath (Join-Path $staging '_source') -Recurse -Force }

  Step 'Installing the modified game files'
  Get-ChildItem -LiteralPath $Payload -Recurse -File | ForEach-Object {
    $target = Join-Path $staging $_.FullName.Substring($Payload.Length + 1)
    New-Item -ItemType Directory -Force -Path (Split-Path $target -Parent) | Out-Null
    Copy-Item -LiteralPath $_.FullName -Destination $target
  }
  foreach ($dir in @('data\userdata', 'data\logs')) { New-Item -ItemType Directory -Force -Path (Join-Path $staging $dir) | Out-Null }

  Step 'Verifying the installation against the reference release'
  $keep = @('data/engine/vf2.toml')
  $problems = @()
  foreach ($item in $reference) {
    $path = Join-Path $staging ($item.path -replace '/', '\')
    if (-not (Test-Path -LiteralPath $path)) { $problems += "missing: $($item.path)"; continue }
    if ((Get-Item -LiteralPath $path).Length -ne $item.bytes -or (FileSha256 $path) -ne $item.sha256) { $problems += "different: $($item.path)" }
  }
  $listed = @{}; foreach ($item in $reference) { $listed[$item.path] = $true }
  $listed['MANIFEST.json'] = $true
  Get-ChildItem -LiteralPath $staging -Recurse -File | ForEach-Object {
    $rel = $_.FullName.Substring($staging.Length + 1) -replace '\\', '/'
    if (-not $listed.ContainsKey($rel)) { $problems += "unexpected: $rel" }
  }
  if ($problems.Count) { Fail ("Installation does not match the reference release:`n  " + ($problems -join "`n  ")) }

  if ($previous) {
    Step 'Keeping saves and settings of the earlier installation'
    $oldUser = Join-Path $Destination 'data\userdata'
    if (Test-Path -LiteralPath $oldUser) {
      Remove-Item -LiteralPath (Join-Path $staging 'data\userdata') -Recurse -Force
      Copy-Item -LiteralPath $oldUser -Destination (Join-Path $staging 'data\userdata') -Recurse
    }
    foreach ($rel in $keep) {
      $old = Join-Path $Destination ($rel -replace '/', '\')
      if (Test-Path -LiteralPath $old) { Copy-Item -LiteralPath $old -Destination (Join-Path $staging ($rel -replace '/', '\')) -Force }
    }
    $retired = "$Destination.old-" + [guid]::NewGuid().ToString('N').Substring(0, 8)
    Move-Item -LiteralPath $Destination -Destination $retired
    Move-Item -LiteralPath $staging -Destination $Destination
    $staging = $null
    Remove-Item -LiteralPath $retired -Recurse -Force
  } else {
    if (Test-Path -LiteralPath $Destination) { Remove-Item -LiteralPath $Destination -Force -Recurse }
    Move-Item -LiteralPath $staging -Destination $Destination
    $staging = $null
  }
  Step "Done. Start: $(Join-Path $Destination 'Virtua Fighter 2.exe')"
  $exitCode = 0
} catch {
  Write-Host ''
  Write-Host "INSTALLATION FAILED: $($_.Exception.Message)" -ForegroundColor Red
} finally {
  if ($staging -and (Test-Path -LiteralPath $staging)) { Remove-Item -LiteralPath $staging -Recurse -Force -ErrorAction SilentlyContinue }
}
exit $exitCode
