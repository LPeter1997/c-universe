param(
    [Parameter(Mandatory=$true)]
    [string]$Compiler,

    [Parameter(Mandatory=$true)]
    [ValidateSet("gcc","msvc")]
    [string]$Style,

    [Parameter(Mandatory=$true)]
    [ValidateSet("x86","x64")]
    [string]$Arch,

    [Parameter(Mandatory=$true)]
    [ValidateSet("version","run")]
    [string]$Action,

    [Parameter(Mandatory=$false)]
    [string[]]$Sources = @(),

    [Parameter(Mandatory=$false)]
    [string[]]$Defines = @(),

    [Parameter(Mandatory=$false)]
    [string]$Output = "a.out",

    [Parameter(Mandatory=$false)]
    [switch]$AllowUnusedParameters,

    [Parameter(Mandatory=$false, ValueFromRemainingArguments=$true)]
    [string[]]$RunArgs = @()
)

$ErrorActionPreference = "Stop"

# Windows executable suffix
if ($IsWindows -or $env:OS -eq "Windows_NT") {
    if (-not $Output.EndsWith(".exe")) {
        $Output = "$Output.exe"
    }
}

if ($Action -eq "run" -and $Sources.Count -eq 0) {
    throw "for running, at least one source file must be provided via -Sources"
}

function Show-Version {
    if ($Style -eq "msvc") {
        & $Compiler
    } else {
        & $Compiler --version
    }
}

function Compile {
    Write-Host "building with $Compiler ($Style)..."

    # On Windows we add _CRT_SECURE_NO_WARNINGS to the defines
    if ($IsWindows -or $env:OS -eq "Windows_NT") {
        $Defines += "_CRT_SECURE_NO_WARNINGS"
    }
    # Build flags
    $Args = @()
    if ($Style -eq "msvc") {
        # Standard, MSVC has no C99 mode, but we can set the compiler to stick to C
        $Args += "/TC"
        # Warnings
        $Args += "/W4"
        $Args += "/WX"
        $Args += "/permissive-"
        $Args += "/Zc:preprocessor"
        # Disable unused params warnings if requested
        if ($AllowUnusedParameters) {
            $Args += "/wd4100"
        }
        # Add each define as a separate /D flag
        foreach ($def in $Defines) {
            $Args += "/D$def"
        }
        # Output
        $Args += "/Fe:$Output"
        # Add each source file
        foreach ($src in $Sources) {
            $Args += $src
        }
    } elseif ($Style -eq "gcc") {
        # Standard
        $Args += "-std=c99"
        # Warnings
        $Args += "-Wall"
        $Args += "-Wextra"
        $Args += "-Wpedantic"
        $Args += "-Wconversion"
        $Args += "-Wshadow"
        $Args += "-Wstrict-prototypes"
        $Args += "-Werror"
        # Disable unused params warnings if requested
        if ($AllowUnusedParameters) {
            $Args += "-Wno-unused-parameter"
        }
        # Add each define as a separate -D flag
        foreach ($def in $Defines) {
            $Args += "-D$def"
        }
        # Architecture
        if ($Arch -eq "x86") {
            $Args += "-m32"
        } elseif ($Arch -eq "x64") {
            $Args += "-m64"
        }
        # Output
        $Args += "-o"
        $Args += $Output
        # Add each source file and specify their source language explicitly as C to avoid compiling a PCH
        foreach ($src in $Sources) {
            $Args += "-x"
            $Args += "c"
            $Args += $src
        }
    } else {
        throw "unknown style $Style"
    }

    & $Compiler $Args

    if ($LASTEXITCODE -ne 0) {
        throw "tool $Compiler failed with exit code $LASTEXITCODE"
    }

    Write-Host "build succeeded with result $Output"
}

function Run {
    if (!(Test-Path $Output)) {
        throw "executable $Output not found, cannot run"
    }

    # Resolve output to absolute path before changing directories
    $AbsOutput = Resolve-Path $Output

    # Change to the directory of the first source file
    $SourceDir = Split-Path -Parent (Resolve-Path $Sources[0])
    $OriginalDir = Get-Location
    Set-Location $SourceDir

    try {
        Write-Host "running $AbsOutput from $SourceDir..."
        & $AbsOutput @RunArgs

        if ($LASTEXITCODE -ne 0) {
            throw "running $Output failed with exit code $LASTEXITCODE"
        }
    } finally {
        # Restore original working directory
        Set-Location $OriginalDir
    }
}

function Clean {
    if (Test-Path $Output) {
        Remove-Item $Output
        Write-Host "deleted $Output"
    }
}

if ($Action -eq "version") {
    Show-Version
} elseif ($Action -eq "run") {
    Compile
    Run
    Clean
} else {
    throw "unknown action $Action"
}
