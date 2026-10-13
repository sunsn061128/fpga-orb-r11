[CmdletBinding()]
param([string]$Python)
$ErrorActionPreference='Stop'
if(-not $Python){
    $orbVersionsProfile='C:/Users/dmsun/Documents/Codex/toolchains/fpga/bin/fpga-python.cmd'
    if(Test-Path -LiteralPath $orbVersionsProfile){$Python=$orbVersionsProfile}
    else {$Python='python'}
}
& $Python (Join-Path $PSScriptRoot 'verify_versions.py')
if($LASTEXITCODE -ne 0){throw "Version integrity check failed: exit=$LASTEXITCODE"}
