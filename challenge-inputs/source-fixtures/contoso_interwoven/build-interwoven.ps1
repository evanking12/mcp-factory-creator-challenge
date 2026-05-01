param(
    [string]$Source = "contoso_cs_interwoven.c",
    [string]$Output = "contoso_cs_interwoven.dll",
    [string]$ContosoBaseDll = "..\\contoso_legacy\\contoso_cs.dll"
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $here
try {
    if (-not (Test-Path $Source)) {
        throw "Source file not found: $Source"
    }

    $built = $false

    # Prefer a full MSVC toolchain environment (VsDevCmd), then fall back to
    # direct cl.exe if caller is already inside a Developer Command Prompt.
    $vsDevCandidates = @()
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
        if ($installPath) {
            $vsDevCandidates += (Join-Path $installPath "Common7\Tools\VsDevCmd.bat")
        }
    }
    $vsDevCandidates += @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"
    )

    $vsDevCmd = $null
    foreach ($cand in $vsDevCandidates) {
        if (Test-Path $cand) { $vsDevCmd = $cand; break }
    }

    if ($vsDevCmd) {
        Write-Host "Building with MSVC via VsDevCmd..."
        $cmd = "`"$vsDevCmd`" -arch=x64 -host_arch=x64 >nul && cl /O2 /GS- /LD /W3 $Source /Fe:$Output"
        cmd /c $cmd | Out-Host
        if (Test-Path $Output) {
            $built = $true
        }
    }

    if (-not $built) {
        $cl = Get-Command cl.exe -ErrorAction SilentlyContinue
        if ($cl) {
            Write-Host "Building with direct cl.exe (expects dev prompt env)..."
            & cl.exe /O2 /GS- /LD /W3 $Source /Fe:$Output | Out-Host
            if (Test-Path $Output) {
                $built = $true
            }
        }
    }

    if (-not $built) {
        $gcc = Get-Command gcc.exe -ErrorAction SilentlyContinue
        if ($gcc) {
            Write-Host "Building with MinGW gcc.exe..."
            & gcc.exe -O2 -shared -o $Output $Source "-Wl,--out-implib,libcontoso_cs_interwoven.a" | Out-Host
            if (Test-Path $Output) {
                $built = $true
            }
        }
    }

    if (-not $built) {
        throw "Build failed: no usable MSVC/MinGW toolchain found. Use a VS Developer Command Prompt or install gcc."
    }

    if (Test-Path $ContosoBaseDll) {
        Copy-Item $ContosoBaseDll (Join-Path $here "contoso_cs.dll") -Force
        Write-Host "Copied dependency: contoso_cs.dll"
    } else {
        Write-Warning "Base DLL not found at $ContosoBaseDll. Ensure contoso_cs.dll is on PATH or next to output DLL."
    }

    Write-Host "Build complete: $(Join-Path $here $Output)"
}
finally {
    Pop-Location
}
