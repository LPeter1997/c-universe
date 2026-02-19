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

# Function to not have to specify -Compiler, -Style, -Arch and -Action for each test
function Compile-And-Run {
    param(
        [Parameter(Mandatory=$true)]
        [string[]]$Sources,

        [Parameter(Mandatory=$false)]
        [string[]]$Defines = @(),

        [Parameter(Mandatory=$false)]
        [string]$Output = "a.out",

        [Parameter(Mandatory=$false)]
        [switch]$AllowUnusedParameters
    )

    .\compile_and_run.ps1 `
        -Compiler $Compiler `
        -Style $Style `
        -Arch $Arch `
        -Action "run" `
        -Sources $Sources `
        -Defines $Defines `
        -Output $Output `
        -AllowUnusedParameters:$AllowUnusedParameters
}

# 1. ctest library
Write-Host "running self-test for ctest library..."
Compile-And-Run `
    -Sources @("../src/ctest.h") `
    -Defines @("CTEST_STATIC", "CTEST_IMPLEMENTATION", "CTEST_SELF_TEST") `
    -AllowUnusedParameters
Write-Host "running example for ctest library..."
Compile-And-Run `
    -Sources @("../src/ctest.h") `
    -Defines @("CTEST_EXAMPLE") `
    -AllowUnusedParameters

# 2. GC library
Write-Host "running self-test for GC library..."
Compile-And-Run `
    -Sources @("../src/gc.h") `
    -Defines @("GC_STATIC", "GC_IMPLEMENTATION", "GC_SELF_TEST") `
    -AllowUnusedParameters
Write-Host "running example for GC library..."
Compile-And-Run `
    -Sources @("../src/gc.h") `
    -Defines @("GC_EXAMPLE") `
    -AllowUnusedParameters

# 3. Argparse library
Write-Host "running self-test for Argparse library..."
Compile-And-Run `
    -Sources @("../src/argparse.h") `
    -Defines @("ARGPARSE_STATIC", "ARGPARSE_IMPLEMENTATION", "ARGPARSE_SELF_TEST") `
    -AllowUnusedParameters
