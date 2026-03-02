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

    [Parameter(Mandatory=$false)]
    [switch]$AllowUnusedFunctions,

    [Parameter(Mandatory=$false)]
    [string[]]$AdditionalCompilerArgs = @(),

    [Parameter(Mandatory=$false, ValueFromRemainingArguments=$true)]
    [string[]]$RunArgs = @()
)

$ErrorActionPreference = "Stop"

# Platform detection
$IsWindowsPlatform = $IsWindows -or $env:OS -eq "Windows_NT"
$IsLinuxPlatform = $IsLinux -or (Test-Path "/proc/version" -ErrorAction SilentlyContinue)
$IsMacOSPlatform = $IsMacOS -or (Test-Path "/System/Library" -ErrorAction SilentlyContinue)

# Windows executable suffix
if ($IsWindowsPlatform) {
    if (-not $Output.EndsWith(".exe")) {
        $Output = "$Output.exe"
    }
}

if ($Action -eq "run" -and $Sources.Count -eq 0) {
    throw "for running, at least one source file must be provided via -Sources"
}

# On Windows we add _CRT_SECURE_NO_WARNINGS to the defines
# This must be at script scope, not inside a function, to avoid PowerShell scoping issues with +=
if ($IsWindowsPlatform) {
    $Defines += "_CRT_SECURE_NO_WARNINGS"
}

# Platform-specific compiler/linker flags
# These are added automatically based on platform and compiler style
$PlatformCompilerArgs = @()
$PlatformLinkerArgs = @()

if ($Style -eq "gcc") {
    # Always include debug symbols for meaningful stack traces and debugging
    $PlatformCompilerArgs += "-g"

    if ($IsLinuxPlatform) {
        # -rdynamic: Export symbols for dladdr() to resolve function names
        # -ldl: Link with libdl for dladdr()
        $PlatformLinkerArgs += "-rdynamic"
        $PlatformLinkerArgs += "-ldl"
    } elseif ($IsMacOSPlatform) {
        # -ldl: Link with libdl for dladdr()
        $PlatformLinkerArgs += "-ldl"
    }
    # Windows + MinGW: -g is already added above, addr2line doesn't need extra linker flags
} elseif ($Style -eq "msvc") {
    # /Zi: Generate debug info for PDB (needed for DbgHelp symbol resolution)
    # /DEBUG: Linker flag to generate PDB file
    $PlatformCompilerArgs += "/Zi"
    $PlatformLinkerArgs += "/DEBUG"
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
        # Disable unused functions warnings if requested
        if ($AllowUnusedFunctions) {
            $Args += "/wd4505"
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
        # Disable unused functions warnings if requested
        if ($AllowUnusedFunctions) {
            $Args += "-Wno-unused-function"
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

    # Add platform-specific compiler args
    $Args += $PlatformCompilerArgs

    # Add any additional compiler args specified by the user
    $Args += $AdditionalCompilerArgs

    # Add platform-specific linker args
    if ($Style -eq "msvc") {
        # MSVC: linker args go after /link
        if ($PlatformLinkerArgs.Count -gt 0) {
            $Args += "/link"
            $Args += $PlatformLinkerArgs
        }
    } elseif ($Style -eq "gcc") {
        # GCC: linker args go at the end
        $Args += $PlatformLinkerArgs
    }

    # Run the compiler. We temporarily set ErrorActionPreference to Continue because
    # MSVC writes its version banner to stderr, which PowerShell would otherwise treat
    # as an error. We check LASTEXITCODE manually instead.
    $OldErrorPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & $Compiler $Args 2>&1 | Write-Host
    } finally {
        $ErrorActionPreference = $OldErrorPreference
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

    # Resolve output to absolute path before changing directories
    $AbsOutput = Resolve-Path $Output

    # Change to the directory of the first source file
    $SourceDir = Split-Path -Parent (Resolve-Path $Sources[0])
    $OriginalDir = Get-Location
    Set-Location $SourceDir

    try {
        Write-Host "running $AbsOutput from $SourceDir..."

        # Temporarily set ErrorActionPreference to Continue because programs may
        # legitimately write to stderr (e.g., error messages, debug info), which
        # PowerShell would otherwise treat as errors. We check LASTEXITCODE manually.
        $OldErrorPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        try {
            & $AbsOutput @RunArgs 2>&1 | Write-Host
        } finally {
            $ErrorActionPreference = $OldErrorPreference
        }

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
