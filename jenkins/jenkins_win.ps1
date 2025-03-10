# Copyright 2022-Present Couchbase, Inc.
#
# Use of this software is governed by the Business Source License included in
# the file licenses/BSL-Couchbase.txt.  As of the Change Date specified in that
# file, in accordance with the Business Source License, use of this software
# will be governed by the Apache License, Version 2.0, included in the file
# licenses/APL2.txt.

# This script is for PR Validation. It builds binaries for Windows platform and runs the tests.

if ($env:CHANGE_TARGET -eq "master") {
    $env:BRANCH = "main"
} else {
    $env:BRANCH = $env:CHANGE_TARGET
}

Write-Host "BRANCH = $env:BRANCH"
Write-Host "BRANCH_NAME = $env:BRANCH_NAME"

Push-Location "$PSScriptRoot\.."
try {
    # Move everyting into couchbase-lite-c directory
    New-Item -Type Directory "couchbase-lite-c"
    Get-ChildItem -Path $pwd -Force -Exclude "couchbase-lite-c" | Move-Item -Destination "couchbase-lite-c"

    # Get couchbase-lite-c-ee
    & 'C:\Program Files\Git\bin\git.exe' clone ssh://git@github.com/couchbase/couchbase-lite-c-ee --branch $env:BRANCH_NAME --recursive --depth 1 couchbase-lite-c-ee
    if($LASTEXITCODE -ne 0) {
        & 'C:\Program Files\Git\bin\git.exe' clone ssh://git@github.com/couchbase/couchbase-lite-c-ee --branch $env:BRANCH --recursive --depth 1 couchbase-lite-c-ee
    }

    # Update couchbase-lite-c submodules
    Push-Location couchbase-lite-c
    & 'C:\Program Files\Git\bin\git.exe' submodule update --init --recursive

    # Move lite-core-ee
    Push-Location vendor
    Move-Item -Path ..\..\couchbase-lite-c-ee\couchbase-lite-core-EE -Destination .
    Pop-Location

    # Download VS extension
    .\scripts\download_vector_search_extension.ps1

    # Build
    New-Item -Type Directory -ErrorAction Ignore build
    Set-Location build
    & 'C:\Program Files\CMake\bin\cmake.exe' -A x64 -DBUILD_ENTERPRISE=ON -DCMAKE_INSTALL_PREFIX="${pwd}\out" ..
    if($LASTEXITCODE -ne 0) {
        Write-Host "Failed to run CMake!" -ForegroundColor Red
        exit 1
    }

    & 'C:\Program Files\CMake\bin\cmake.exe' --build . --parallel 12
    if($LASTEXITCODE -ne 0) {
        Write-Host "Failed to build!" -ForegroundColor Red
        exit 1
    }

    # Run Tests
    $env:LiteCoreTestsQuiet=1
    Set-Location test\Debug
    .\CBL_C_Tests -r list
    if($LASTEXITCODE -ne 0) {
        Write-Host "C++ tests failed!" -ForegroundColor Red
        exit 1
    }
} finally {
    # Clean up downloaded extension files
    Remove-Item -Path $PSScriptRoot\..\test\extensions\windows -Recurse -Force -ErrorAction Ignore 
    Pop-Location
}
