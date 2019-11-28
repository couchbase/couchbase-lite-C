if($env:CHANGE_TARGET) {
    $env:BRANCH = $env:CHANGE_TARGET
}

Push-Location "$PSScriptRoot\.."
try {
    & 'C:\Program Files\Git\bin\git.exe' submodule update --init --recursive
    Push-Location vendor
    & 'C:\Program Files\Git\bin\git.exe' clone ssh://git@github.com/couchbase/couchbase-lite-core-EE --branch $env:BRANCH --recursive --depth 1 couchbase-lite-core-EE
    Pop-Location

    New-Item -Type Directory -ErrorAction Ignore build
    Set-Location build
    & 'C:\Program Files\CMake\bin\cmake.exe' -G "Visual Studio 15 2017 Win64" -DBUILD_ENTERPRISE=ON ..
    if($LASTEXITCODE -ne 0) {
        Write-Host "Failed to run CMake!" -ForegroundColor Red
        exit 1
    }

    & 'C:\Program Files\CMake\bin\cmake.exe' --build .
    if($LASTEXITCODE -ne 0) {
        Write-Host "Failed to build!" -ForegroundColor Red
        exit 1
    }

    $env:LiteCoreTestsQuiet=1
    Set-Location test\Debug
    .\CBL_C_Tests -r list
    if($LASTEXITCODE -ne 0) {
        Write-Host "C++ tests failed!" -ForegroundColor Red
        exit 1
    }
} finally {
    Pop-Location
}