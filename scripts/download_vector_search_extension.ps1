Push-Location $PSScriptRoot\..\test\extensions

$VsVersionFile = Join-Path (Get-Location).Path "version.txt"
$Version = (Get-Content $VsVersionFile).Split('-')[0]
$BlNum = (Get-Content $VsVersionFile).Split('-')[1]

Remove-Item -Path windows\x86_64 -Recurse -Force -ErrorAction Ignore 
New-Item -Path windows\x86_64 -ItemType Directory | Out-Null
Push-Location windows\x86_64

Write-Output "Downloading Vector Search Framework ${Version}-${BlNum} ..."

$Filename = "couchbase-lite-vector-search-${Version}-${BlNum}-windows-x86_64"
$ZipFilename = "${Filename}.zip"
$Url = "http://latestbuilds.service.couchbase.com/builds/latestbuilds/couchbase-lite-vector-search/${Version}/${BlNum}/${ZipFilename}"
$DestFile = Join-Path (Get-Location).Path $ZipFilename
Invoke-WebRequest $Url -OutFile $DestFile
Expand-Archive -Path $ZipFilename -DestinationPath (Get-Location).Path

Move-Item lib\*.* .
Move-Item bin\*.* .
Remove-Item lib -Recurse -Force
Remove-Item bin -Recurse -Force
Remove-Item $ZipFilename
    
Pop-Location
Pop-Location
