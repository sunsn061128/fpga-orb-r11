[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$cmake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
$ninja = 'C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$compiler = 'C:/Users/dmsun/mingw/mingw64/bin/g++.exe'
$compilerBin = 'C:\Users\dmsun\mingw\mingw64\bin'
$referenceRoot = Join-Path (Split-Path -Parent $projectRoot) 'FPGA_ORB_SHARED_v1.0'
$buildRoot = Join-Path $projectRoot 'host_build_v1'
$oldPath = $env:PATH

try {
    $env:PATH = "$compilerBin;$oldPath"
    & $cmake -S $projectRoot -B $buildRoot -G Ninja `
        "-DCMAKE_MAKE_PROGRAM=$ninja" "-DCMAKE_CXX_COMPILER=$compiler" `
        "-DCMAKE_BUILD_TYPE=Release" "-DORB_REFERENCE_ROOT=$referenceRoot"
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
    & $cmake --build $buildRoot --parallel
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    & $ctest --test-dir $buildRoot --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'CTest failed' }
} finally {
    $env:PATH = $oldPath
}
