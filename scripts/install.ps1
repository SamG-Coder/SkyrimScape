param([string]$Game='D:\SteamLibrary\steamapps\common\Skyrim Special Edition')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(Get-Process SkyrimSE -ErrorAction SilentlyContinue){throw 'Close Skyrim before installing SkyrimScape. No files changed.'}
if((Get-Item -LiteralPath "$Game\SkyrimSE.exe").VersionInfo.FileVersion -ne '1.7.104.0'){throw 'This experimental build requires Skyrim 1.7.104.0.'}
foreach($dependency in @('skse64_loader.exe','skse64_1_7_104.dll','Data\SKSE\Plugins\versionlib-1-7-104-0.bin')){
 if(!(Test-Path -LiteralPath (Join-Path $Game $dependency))){throw "Missing dependency: $dependency"}
}
$source=Join-Path $root 'build\plugin\Release\SkyrimScape.dll'
if(!(Test-Path -LiteralPath $source)){throw 'Build SkyrimScape first.'}
$destination=Join-Path $Game 'Data\SKSE\Plugins\SkyrimScape.dll'
if(Test-Path -LiteralPath $destination){
 $backup=Join-Path $root ('backups\'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
 New-Item -ItemType Directory -Force -Path $backup | Out-Null
 Copy-Item -LiteralPath $destination -Destination $backup
}
Copy-Item -LiteralPath $source -Destination $destination -Force
if((Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $destination).Hash){throw 'Installed DLL hash mismatch.'}
Write-Output 'SkyrimScape installed. Start skse64_loader.exe yourself, load a character, then press F8.'
