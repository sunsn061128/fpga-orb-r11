[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$fastv2Root = Split-Path -Parent $PSScriptRoot
$cmake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
$ninja = 'C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$compiler = 'C:/Users/dmsun/mingw/mingw64/bin/g++.exe'
$compilerBin = 'C:\Users\dmsun\mingw\mingw64\bin'
$referenceRoot = Join-Path (Split-Path -Parent (Split-Path -Parent $fastv2Root)) 'FPGA_ORB_SHARED_v1.0'
$buildRoot = Join-Path $fastv2Root 'host_build'
$savedPath = $env:PATH

try {
    $env:PATH = "$compilerBin;$savedPath"
    & $cmake -S $fastv2Root -B $buildRoot -G Ninja `
        "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_CXX_COMPILER=$compiler" `
        "-DCMAKE_BUILD_TYPE=Release" "-DORB_REFERENCE_ROOT=$referenceRoot"
    if ($LASTEXITCODE -ne 0) { throw 'fastv2 CMake configure failed' }
    # Keep compiler process creation bounded on this Windows host. Unbounded
    # Ninja parallelism intermittently prevents MinGW from launching cc1plus.
    & $cmake --build $buildRoot --parallel 2
    if ($LASTEXITCODE -ne 0) { throw 'fastv2 host build failed' }
    & $ctest --test-dir $buildRoot --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'fastv2 contract regression failed' }
} finally {
    $env:PATH = $savedPath
}
