param(
    [Parameter(Mandatory=$true)]
    [string]$Compiler,

    [Parameter(Mandatory=$true)]
    [ValidateSet("gcc","msvc")]
    [string]$Style,

    [Parameter(Mandatory=$true)]
    [ValidateSet("x86","x64")]
    [string]$Arch
)

# We invoke compile_and_run.ps1 for each library we test

# 1. ctest library
.\compile_and_run.ps1 `
    -Compiler $Compiler `
    -Style $Style `
    -Arch $Arch `
    -Action "run" `
    -AllowUnusedParameters `
    -Sources @("../src/ctest.h") `
    -Defines @("CTEST_STATIC", "CTEST_IMPLEMENTATION", "CTEST_SELF_TEST")
