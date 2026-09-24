param([string]$Game='D:\SteamLibrary\steamapps\common\Skyrim Special Edition')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if (Get-Process SkyrimSE -ErrorAction SilentlyContinue) { throw 'Close Skyrim before preparing the runtime.' }
$version=(Get-Item -LiteralPath "$Game\SkyrimSE.exe").VersionInfo.FileVersion
if($version -ne '1.7.104.0'){throw "Unsupported runtime: $version"}
$plugins=Join-Path $Game 'Data\SKSE\Plugins'
New-Item -ItemType Directory -Force -Path $plugins | Out-Null
# Re-encode the published MIT-licensed AE address mapping as CommonLib's dense format 5.
# No addresses are inferred. Zero denotes an absent ID.
$rows=Import-Csv "$root\external\address-library\offsets-1-7-104.0.csv"
$maximum=($rows | Measure-Object aeid -Maximum).Maximum
$offsets=New-Object uint32[] ([int]$maximum+1)
foreach($row in $rows){$offsets[[int]$row.aeid]=[Convert]::ToUInt32($row.ae_addr,16)}
$out=Join-Path $plugins 'versionlib-1-7-104-0.bin'
if(!(Test-Path -LiteralPath $out)){
  $stream=[IO.File]::Create($out)
  $writer=[IO.BinaryWriter]::new($stream)
  try {
    $writer.Write([int]5)
    foreach($v in @(1,7,104,0)){$writer.Write([uint32]$v)}
    $name=New-Object byte[] 64
    [Text.Encoding]::ASCII.GetBytes('SkyrimSE.exe').CopyTo($name,0)
    $writer.Write($name)
    $writer.Write([int]8); $writer.Write([int]0); $writer.Write([int]$offsets.Length)
    foreach($offset in $offsets){$writer.Write([uint32]$offset)}
  } finally { $writer.Dispose() }
}
foreach($entry in @(
  @('build\skse64\skse64_loader\Release\skse64_loader.exe','skse64_loader.exe'),
  @('build\skse64\skse64\Release\skse64_1_7_104.dll','skse64_1_7_104.dll')
)){
  $dest=Join-Path $Game $entry[1]
  if(Test-Path -LiteralPath $dest){throw "Existing runtime file retained: $dest"}
  Copy-Item -LiteralPath (Join-Path $root $entry[0]) -Destination $dest
}
Write-Output "Prepared SKSE 2.3.1 and $($rows.Count) published addresses for Skyrim $version."
