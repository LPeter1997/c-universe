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
        [switch]$AllowUnusedParameters,

        [Parameter(Mandatory=$false)]
        [switch]$AllowUnusedFunctions,

        [Parameter(Mandatory=$false, ValueFromRemainingArguments=$true)]
        [string[]]$RunArgs = @()
    )

    .\compile_and_run.ps1 `
        -Compiler $Compiler `
        -Style $Style `
        -Arch $Arch `
        -Action "run" `
        -Sources $Sources `
        -Defines $Defines `
        -Output $Output `
        -AllowUnusedParameters:$AllowUnusedParameters `
        -AllowUnusedFunctions:$AllowUnusedFunctions `
        -RunArgs $RunArgs
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
Write-Host "running example for Argparse library..."
Compile-And-Run `
    -Sources @("../src/argparse.h") `
    -Defines @("ARGPARSE_EXAMPLE") `
    -AllowUnusedParameters `
    -RunArgs @("run", "--verbose", "--", "arg1", "arg2", "arg3")

# 4. StringBuilder library
Write-Host "running self-test for StringBuilder library..."
Compile-And-Run `
    -Sources @("../src/string_builder.h") `
    -Defines @("STRING_BUILDER_STATIC", "STRING_BUILDER_IMPLEMENTATION", "STRING_BUILDER_SELF_TEST") `
    -AllowUnusedParameters `
    -AllowUnusedFunctions
Write-Host "running example for StringBuilder library..."
Compile-And-Run `
    -Sources @("../src/string_builder.h") `
    -Defines @("STRING_BUILDER_EXAMPLE") `
    -AllowUnusedParameters

# 5. JSON library
Write-Host "running self-test for JSON library..."
Compile-And-Run `
    -Sources @("../src/json.h") `
    -Defines @("JSON_STATIC", "JSON_IMPLEMENTATION", "JSON_SELF_TEST") `
    -AllowUnusedParameters
Write-Host "running example for JSON library..."
Compile-And-Run `
    -Sources @("../src/json.h") `
    -Defines @("JSON_EXAMPLE") `
    -AllowUnusedParameters
