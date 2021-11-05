<#
.SYNOPSIS
    A script for the Couchbase official build servers to use to build CBL-C for Windows
.DESCRIPTION
    This tool will build various flavors of LiteCore and package them according to the format the the Couchbase build server
    is used to dealing with.  It is the responsibility of the build job to then take the artifacts and put them somewhere.  It
    is meant for the official Couchbase build servers.  Do not try to use it, it will only confuse you.  You have been warned.
.PARAMETER Version
    The version number to give to the build (e.g. 3.0.0)
.PARAMETER BuildNum
    The build number of this build (e.g. 123)
.PARAMETER Edition
    The edition to build (community vs enterprise)
#>
param(
    [Parameter(Mandatory=$true, HelpMessage="The version number to give to the build (e.g. 3.0.0)")][string]$Version,
    [Parameter(Mandatory=$true, HelpMessage="The build number of this build (e.g. 123)")][string]$BuildNum,
    [Parameter(Mandatory=$true, HelpMessage="The edition to build (community vs enterprise)")][string]$Edition
)

# NOTE: This is for Couchbase internal CI usage.  
# This room is full of dragons, so you *will* get confused.  
# You have been warned.

$RelPkgDir = "MinSizeRel"

function Make-Package() {
    param(
        [Parameter(Mandatory=$true, Position = 0)][string]$directory,
        [Parameter(Mandatory=$true, Position = 1)][string]$filename
    )

    Push-Location $directory
    Copy-Item $env:WORKSPACE\product-texts\mobile\couchbase-lite\license\LICENSE_$EDITION.txt libcblite-$VERSION\LICENSE.txt
    & 7za a -tzip -mx9 $env:WORKSPACE\$filename libcblite-$VERSION\LICENSE.txt libcblite-$VERSION\include libcblite-$VERSION\lib libcblite-$VERSION\bin
    if($LASTEXITCODE -ne 0) {
        throw "Zip failed"
    }

    $PropFile = "$env:WORKSPACE\publish_x64.prop"
    New-Item -ItemType File -ErrorAction Ignore -Path $PropFile
    Add-Content $PropFile "PRODUCT=couchbase-lite-c"
    Add-Content $PropFile "VERSION=$Version"
    Add-Content $PropFile "BLD_NUM=$BuildNum"
    Add-Content $PropFile "RELEASE_PACKAGE_NAME=$filename"
    Pop-Location
}

function Build() {
    param(
        [Parameter(Mandatory=$true, Position = 0)][string]$directory
    )

    New-Item -ItemType Directory -ErrorAction Ignore $directory
    Push-Location $directory

    & "C:\Program Files\CMake\bin\cmake.exe" -G "Visual Studio 15 2017" -A x64 -DEDITION="$Edition" -DCMAKE_INSTALL_PREFIX="$pwd/libcblite-${Version}" ..
    if($LASTEXITCODE -ne 0) {
        throw "CMake failed"
    }

    & "C:\Program Files\CMake\bin\cmake.exe" --build . --config MinSizeRel --target install --parallel 12
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

Remove-Item -Recurse -Force -ErrorAction Ignore "${env:WORKSPACE}\build_x64\libcblite-${Version}"
New-Item -Type Junction -Target ${env:WORKSPACE}/couchbase-lite-c-ee/couchbase-lite-core-EE -Path ${env:WORKSPACE}/couchbase-lite-c/vendor/couchbase-lite-core-EE
Build "${env:WORKSPACE}\build_x64"
if("${Edition}" -eq "enterprise") {
    Run-UnitTest "${env:WORKSPACE}\build_x64"
}

Make-Package "${env:WORKSPACE}\build_x64" "couchbase-lite-c-$Edition-$Version-$BuildNum-windows-x86_64.zip"

# Windows symbols into a separate archive since they are not included in the "install" anyway
Push-Location "${env:WORKSPACE}\build_x64\couchbase-lite-c\MinSizeRel"
& 7za a -tzip -mx9 "${env:WORKSPACE}\couchbase-lite-c-$Edition-$Version-$BuildNum-windows-x86_64-symbols.zip" cblite.pdb
Pop-Location