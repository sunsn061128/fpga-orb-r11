<# .SYNOPSIS
Builds and tests the isolated next revision; never overwrites fastv2 baseline outputs.
#>
[CmdletBinding()]
param([ValidateRange(1,8)][int]$BuildJobs = 1, [string]$LogRoot)
$ErrorActionPreference = 'Stop'
$nextRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $PSScriptRoot 'host_process.ps1')
if (-not $LogRoot) {
    $LogRoot=Join-Path $nextRoot ('host_build/native_logs/'+(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+[guid]::NewGuid().ToString('N').Substring(0,8))
}
$cmake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ninja = 'C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
$ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
$compiler = 'C:/Users/dmsun/mingw/mingw64/bin/g++.exe'
$cCompiler = 'C:/Users/dmsun/mingw/mingw64/bin/gcc.exe'
# Pin subprocess lookup to the same installed compiler/binutils using canonical
# paths: the driver's ../../ fallback intermittently failed Windows CreateProcess.
# Do not upgrade tools or change global PATH; keep optimization flags unchanged.
$hostToolFlags = '-B C:/Users/dmsun/mingw/mingw64/libexec/gcc/x86_64-w64-mingw32/14.2.0/ -B C:/Users/dmsun/mingw/mingw64/x86_64-w64-mingw32/bin/'
$savedTaskPath = $env:PATH
try {
    $env:PATH = "C:\Users\dmsun\mingw\mingw64\bin;$savedTaskPath"
    Invoke-LoggedHostProcess -Tool $cmake -Arguments @('-S',$nextRoot,'-B',(Join-Path $nextRoot 'host_build'),'-G','Ninja',"-DCMAKE_MAKE_PROGRAM=$ninja","-DCMAKE_CXX_COMPILER=$compiler","-DCMAKE_C_COMPILER=$cCompiler","-DCMAKE_C_FLAGS=$hostToolFlags","-DCMAKE_CXX_FLAGS=$hostToolFlags",'-DCMAKE_BUILD_TYPE=Release') -WorkingDirectory $nextRoot -LogRoot $LogRoot -Name configure -TimeoutSeconds 120
    # Serial compilation is the conservative default after observed MinGW child
    # launch failures at parallel=2; this is not a claim that the cause is fixed.
    Invoke-LoggedHostProcess -Tool $cmake -Arguments @('--build',(Join-Path $nextRoot 'host_build'),'--parallel',"$BuildJobs") -WorkingDirectory $nextRoot -LogRoot $LogRoot -Name compile -TimeoutSeconds 300
    Invoke-LoggedHostProcess -Tool $ctest -Arguments @('--test-dir',(Join-Path $nextRoot 'host_build'),'--output-on-failure','--timeout','120') -WorkingDirectory $nextRoot -LogRoot $LogRoot -Name tests -TimeoutSeconds 2140
} finally { $env:PATH = $savedTaskPath }
