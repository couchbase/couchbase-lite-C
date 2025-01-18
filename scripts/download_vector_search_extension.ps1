Push-Location $PSScriptRoot\..\test\extensions

$VsVersionFile = Join-Path (Get-Location).Path "version.txt"
$VersionNumber = Get-Content $VsVersionFile

if ($VersionNumber -like "*-*") {
    $Version = $VersionNumber.Split('-')[0]
    $BuildNum = $VersionNumber.Split('-')[1]
    $ZipFilename = "couchbase-lite-vector-search-${Version}-${BuildNum}-windows-x86_64.zip"
    $Url = "http://latestbuilds.service.couchbase.com/builds/latestbuilds/couchbase-lite-vector-search/${Version}/${BlNum}/${ZipFilename}"
} else {
    $Version = $VersionNumber
    $ZipFilename = "couchbase-lite-vector-search-${Version}-windows-x86_64.zip"
    $Url = "https://packages.couchbase.com/releases/couchbase-lite-vector-search/${Version}/${ZipFilename}"
}

Remove-Item -Path windows\x86_64 -Recurse -Force -ErrorAction Ignore 
New-Item -Path windows\x86_64 -ItemType Directory | Out-Null
Push-Location windows\x86_64

Write-Output "Downloading Vector Search Framework from ${Url}"
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
