$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
function Get-PinnedDependency([string]$Name,[string]$Url,[string]$Revision){
 $destination=Join-Path $root "external\$Name"
 if(Test-Path -LiteralPath $destination){
  $actual=git -C $destination rev-parse HEAD
  if($LASTEXITCODE -ne 0 -or $actual -ne $Revision){throw "Existing $Name does not match pinned revision $Revision. It was not modified."}
  return
 }
 New-Item -ItemType Directory -Path $destination -Force | Out-Null
 git -C $destination init
 if($LASTEXITCODE -ne 0){throw 'git init failed'}
 git -C $destination remote add origin $Url
 if($LASTEXITCODE -ne 0){throw 'git remote failed'}
 git -C $destination fetch --depth 1 origin $Revision
 if($LASTEXITCODE -ne 0){throw "Could not fetch $Name"}
 git -C $destination checkout --detach FETCH_HEAD
 if($LASTEXITCODE -ne 0){throw "Could not check out $Name"}
}
Get-PinnedDependency 'CommonLibSSE-NG' 'https://github.com/alandtse/CommonLibSSE-NG.git' 'a898f469851c464d05137bb74b069dd234897643'
Get-PinnedDependency 'vcpkg' 'https://github.com/microsoft/vcpkg.git' '6ade29bbd5a4c7e99439ec52adbc63c073953a57'
& "$root\external\vcpkg\bootstrap-vcpkg.bat" -disableMetrics
if($LASTEXITCODE -ne 0){throw 'vcpkg bootstrap failed'}
& "$root\external\vcpkg\vcpkg.exe" install spdlog:x64-windows-static directxtk:x64-windows-static rapidcsv:x64-windows-static --disable-metrics
if($LASTEXITCODE -ne 0){throw 'Dependency installation failed'}
Write-Output 'Dependencies ready. See README.md for CMake build commands.'
