param(
    [Parameter(Mandatory=$true)]
    [string]$Compiler,

    [Parameter(Mandatory=$true)]
    [ValidateSet("gcc","msvc")]
    [string]$Style,

    [Parameter(Mandatory=$true)]
    [ValidateSet("version","run")]
    [string]$Action,

    [Parameter(Mandatory=$false)]
    [string[]]$Sources = @(),

    [Parameter(Mandatory=$false)]
    [string[]]$Defines = @(),

    [Parameter(Mandatory=$false)]
    [string]$Output = "a.out"
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

    # Convert defines
    $DefineFlags = @()
    foreach ($def in $Defines) {
        if ($Style -eq "msvc") {
            $DefineFlags += "/D$def"
        } elseif ($Style -eq "gcc") {
            $DefineFlags += "-D$def"
        } else {
            throw "unknown style $Style"
        }
    }

    if ($Style -eq "msvc") {
        & $Compiler `
            /std:c99 `
            /W4 /WX `
            /permissive- `
            /Zc:preprocessor `
            $DefineFlags `
            /Fe:$Output `
            $Sources
    } elseif ($Style -eq "gcc") {
        & $Compiler `
            -std=c99 `
            -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes -Werror `
            $DefineFlags `
            $Sources `
            -o $Output
    } else {
        throw "unknown style $Style"
    }

    if ($LASTEXITCODE -ne 0) {
        throw "tool $Compiler failed with exit code $LASTEXITCODE"
    }

    Write-Host "build succeeded with result $Output"
}

function Run {
    if (!(Test-Path $Output)) {
        throw "executable $Output not found, cannot run"
    }

    Write-Host "running $Output..."
    & "./$Output"

    if ($LASTEXITCODE -ne 0) {
        throw "running $Output failed with exit code $LASTEXITCODE"
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
