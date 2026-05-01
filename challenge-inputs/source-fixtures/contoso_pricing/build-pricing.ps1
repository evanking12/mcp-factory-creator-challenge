param(
    [string]$Source = "contoso_pricing.c",
    [string]$Output = "contoso_pricing.dll",
    [string]$ContosoCsDll = "..\\contoso_legacy\\contoso_cs.dll",
    [string]$ContosoPaymentsDll = "..\\contoso_legacy\\contoso_payments.dll"
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $here
try {
    if (-not (Test-Path $Source)) {
        throw "Source file not found: $Source"
    }

    $built = $false
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
        if (Test-Path $Output) { $built = $true }
    }

    if (-not $built) {
        $cl = Get-Command cl.exe -ErrorAction SilentlyContinue
        if ($cl) {
            Write-Host "Building with direct cl.exe (expects dev prompt env)..."
            & cl.exe /O2 /GS- /LD /W3 $Source /Fe:$Output | Out-Host
            if (Test-Path $Output) { $built = $true }
        }
    }

    if (-not $built) {
        throw "Build failed: no usable MSVC toolchain found."
    }

    if (Test-Path $ContosoCsDll) {
        Copy-Item $ContosoCsDll (Join-Path $here "contoso_cs.dll") -Force
        Write-Host "Copied dependency: contoso_cs.dll"
    }
    if (Test-Path $ContosoPaymentsDll) {
        Copy-Item $ContosoPaymentsDll (Join-Path $here "contoso_payments.dll") -Force
        Write-Host "Copied dependency: contoso_payments.dll"
    }

    Write-Host "Build complete: $(Join-Path $here $Output)"
}
finally {
    Pop-Location
}
