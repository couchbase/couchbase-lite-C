<#
.SYNOPSIS
    A script for the Couchbase official build servers to use to build CBL-C for Windows
.DESCRIPTION
    This tool will build various flavors of LiteCore and package them according to the format the the Couchbase build server
    is used to dealing with.  It is the responsibility of the build job to then take the artifacts and put them somewhere.  It
    is meant for the official Couchbase build servers.  Do not try to use it, it will only confuse you.  You have been warned.
.PARAMETER Version
    The version number to give to the build (e.g. 3.0.0)
.PARAMETER ShaVersion
    The commit SHA that this build was built from
.PARAMETER Edition
    The edition to build (community vs enterprise)
#>
param(
    [Parameter(Mandatory=$true, HelpMessage="The version number to give to the build (e.g. 3.0.0)")][string]$Version,
    [Parameter(Mandatory=$true, HelpMessage="The commit SHA that this build was built from")][string]$ShaVersion,
    [Parameter(Mandatory=$true, HelpMessage="The edition to build (community vs enterprise)")][string]$Edition
)

$RelPkgDir = "MinSizeRel"

function Make-Package() {
    param(
        [Parameter(Mandatory=$true, Position = 0)][string]$directory,
        [Parameter(Mandatory=$true, Position = 1)][string]$filename
    )

    Push-Location $directory
    & 7za a -tzip -mx9 $env:WORKSPACE\$filename include lib bin
    if($LASTEXITCODE -ne 0) {
        throw "Zip failed"
    }

    $PropFile = "$env:WORKSPACE\publish_$arch.prop"
    New-Item -ItemType File -ErrorAction Ignore -Path $PropFile
    Add-Content $PropFile "PRODUCT=couchbase-lite-c"
    Add-Content $PropFile "VERSION=$ShaVersion"
    Add-Content $PropFile "${config}_PACKAGE_NAME_$architecture=$filename"
    Pop-Location
}

function Build() {
    param(
        [Parameter(Mandatory=$true, Position = 0)][string]$directory
    )

    New-Item -ItemType Directory -ErrorAction Ignore $directory
    Push-Location $directory

    Write-Host $Edition
    & "C:\Program Files\CMake\bin\cmake.exe" -G "Visual Studio 15 2017" -A x64 -DEDITION="$Edition" -DCMAKE_INSTALL_PREFIX="$pwd/out" ..
    if($LASTEXITCODE -ne 0) {
        throw "CMake failed"
    }

    & "C:\Program Files\CMake\bin\cmake.exe" --build . --config MinSizeRel --target install
    if($LASTEXITCODE -ne 0) {
        throw "Build failed ($LASTEXITCODE)"
    }

    Pop-Location
}

function Run-UnitTest() {
    param(
        [Parameter(Mandatory=$true, Position = 0)][string]$directory
    )

    Pop-Location
    Push-Location $directory\couchbase-lite-c\test\MinSizeRel
    & .\CBL_C_Tests -r list
    if($LASTEXITCODE -ne 0) {
        throw "CBL_C_Tests failed"
    }

    Pop-Location
}

Remove-Item -Recurse -Force "${env:WORKSPACE}\build_x64\out"
Build "${env:WORKSPACE}\build_x64"
Run-UnitTest "${env:WORKSPACE}\build_x64"
Make-Package "${env:WORKSPACE}\build_x64\out" "couchbase-lite-c-$Version-$ShaVersion-windows-x64.zip"
