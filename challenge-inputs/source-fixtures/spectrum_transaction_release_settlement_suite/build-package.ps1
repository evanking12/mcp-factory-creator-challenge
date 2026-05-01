param(
    [string]$ManifestPath = "",
    [string]$UploadMirrorDir = "C:\\mcp-factory\\uploads"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..\..\..")
if ([string]::IsNullOrWhiteSpace($ManifestPath)) {
    $manifestPath = Join-Path $scriptDir "package_manifest.json"
} else {
    $manifestPath = [System.IO.Path]::GetFullPath($ManifestPath)
}
$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
$binDir = Join-Path $scriptDir "bin"
$objDir = Join-Path $scriptDir ".obj"
$includeDir = Join-Path $scriptDir "include"
$srcDir = Join-Path $scriptDir "src"

function Resolve-RepoPath {
    param([string]$RelativePath)
    return [System.IO.Path]::GetFullPath((Join-Path $repoRoot $RelativePath))
}

function Ensure-ParentDirectory {
    param([string]$FilePath)
    $parent = Split-Path -Parent $FilePath
    if (-not (Test-Path $parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
}

function Get-OptionalProperty {
    param(
        [object]$Object,
        [string]$Name
    )
    $prop = $Object.PSObject.Properties[$Name]
    if ($null -eq $prop) {
        return $null
    }
    return $prop.Value
}

function Invoke-BuildDll {
    param(
        [string]$SourceFile,
        [string]$OutputFile,
        [string]$VsDevCmd,
        [bool]$UseDirectCl
    )

    $sourcePath = Join-Path $srcDir $SourceFile
    $outputPath = Join-Path $binDir $OutputFile
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($OutputFile)
    $objPath = Join-Path $objDir "$baseName.obj"
    $cmdLine = "cl /nologo /O2 /GS- /LD /W3 /I `"$includeDir`" `"$sourcePath`" /Fo`"$objPath`" /Fe:`"$outputPath`""

    if ($VsDevCmd) {
        cmd /c "`"$VsDevCmd`" -arch=x64 -host_arch=x64 >nul && $cmdLine" | Out-Host
    } elseif ($UseDirectCl) {
        & cl.exe /nologo /O2 /GS- /LD /W3 /I $includeDir $sourcePath /Fo$objPath /Fe:$outputPath | Out-Host
    } else {
        throw "Build failed: no usable MSVC toolchain found."
    }

    if (-not (Test-Path $outputPath)) {
        throw "Failed to build $OutputFile"
    }
}

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
    if (Test-Path $cand) {
        $vsDevCmd = $cand
        break
    }
}
$useDirectCl = $false
if (-not $vsDevCmd) {
    $cl = Get-Command cl.exe -ErrorAction SilentlyContinue
    if ($cl) {
        $useDirectCl = $true
    }
}

if (-not (Test-Path $binDir)) {
    New-Item -ItemType Directory -Path $binDir -Force | Out-Null
}
if (-not (Test-Path $objDir)) {
    New-Item -ItemType Directory -Path $objDir -Force | Out-Null
}

$buildTargets = @(
    @{ Source = "spectrum_session_bootstrap.c"; Output = "spectrum_session_bootstrap.dll" },
    @{ Source = "spectrum_reservation_stage.c"; Output = "spectrum_reservation_stage.dll" },
    @{ Source = "spectrum_dispatch_preview.c"; Output = "spectrum_dispatch_preview.dll" },
    @{ Source = "spectrum_fee_quote.c"; Output = "spectrum_fee_quote.dll" },
    @{ Source = "spectrum_attestation_stage.c"; Output = "spectrum_attestation_stage.dll" },
    @{ Source = "spectrum_invoice_sync.c"; Output = "spectrum_invoice_sync.dll" },
    @{ Source = "spectrum_entitlement_sync.c"; Output = "spectrum_entitlement_sync.dll" },
    @{ Source = "spectrum_release_commit.c"; Output = "spectrum_release_commit.dll" },
    @{ Source = "spectrum_replay_branch.c"; Output = "spectrum_replay_branch.dll" },
    @{ Source = "spectrum_settlement_reconcile.c"; Output = "spectrum_settlement_reconcile.dll" },
    @{ Source = "spectrum_audit_trace.c"; Output = "spectrum_audit_trace.dll" },
    @{ Source = "spectrum_quarantine_gate.c"; Output = "spectrum_quarantine_gate.dll" },
    @{ Source = "spectrum_quarantine_audit.c"; Output = "spectrum_quarantine_audit.dll" }
)

foreach ($target in $buildTargets) {
    Write-Host "Building $($target.Output)..."
    Invoke-BuildDll -SourceFile $target.Source -OutputFile $target.Output -VsDevCmd $vsDevCmd -UseDirectCl $useDirectCl
}

$stageRoot = Resolve-RepoPath $manifest.stage_root
$zipPath = Resolve-RepoPath $manifest.zip_path
$stageBinDir = Join-Path $stageRoot "bin"

if (Test-Path $stageRoot) {
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
}
if (Test-Path $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}

New-Item -ItemType Directory -Path $stageBinDir -Force | Out-Null

$bundledLookup = @{}
foreach ($dll in $manifest.bundled_dlls) {
    $sourcePath = Resolve-RepoPath $dll.source_path
    if (-not (Test-Path $sourcePath)) {
        throw "Missing built DLL for package fixture: $($dll.source_path)"
    }
    $destPath = Join-Path $stageBinDir $dll.staged_name
    Copy-Item -LiteralPath $sourcePath -Destination $destPath -Force
    $bundledLookup[$dll.staged_name] = [ordered]@{
        staged_name = $dll.staged_name
        staged_path = $destPath
        role = $dll.role
        dependency_confidence = $dll.dependency_confidence
        selection_reason = (Get-OptionalProperty $dll "selection_reason")
        source_path = $sourcePath
    }
    if (Test-Path $UploadMirrorDir) {
        Copy-Item -LiteralPath $sourcePath -Destination (Join-Path $UploadMirrorDir $dll.staged_name) -Force
    }
}

foreach ($supportFile in $manifest.support_files) {
    $destPath = Join-Path $stageRoot $supportFile.relative_path
    Ensure-ParentDirectory $destPath
    [System.IO.File]::WriteAllText($destPath, [string]$supportFile.contents, [System.Text.Encoding]::UTF8)
}

$attachPlan = @()
foreach ($target in $manifest.selected_runtime_targets) {
    $dllInfo = $bundledLookup[$target.staged_name]
    $attachPlan += [ordered]@{
        staged_name = $target.staged_name
        staged_path = $dllInfo.staged_path
        selection_reason = $target.selection_reason
        dependency_confidence = $target.dependency_confidence
        lane_id = $target.lane_id
        target_name = $target.target_name
    }
}

$campaignRequest = [ordered]@{
    package_id = $manifest.package_id
    package_label = $manifest.package_label
    package_context = $manifest.package_context
    package_purpose = $manifest.package_purpose
    doc_visibility = $manifest.doc_visibility
    dependency_context = $manifest.dependency_context
    operator_notes = $manifest.operator_notes
    analysis_profile = "strict_dll"
    max_dlls = @($manifest.selected_runtime_targets).Count
}

$campaignRequestPath = Join-Path $stageRoot "campaign-request.json"
$attachPlanPath = Join-Path $stageRoot "attach-plan.json"
$manifestCopyPath = Join-Path $stageRoot "package-manifest.json"

$campaignRequest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $campaignRequestPath -Encoding UTF8
$attachPlan | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $attachPlanPath -Encoding UTF8
(Get-Content $manifestPath -Raw) | Set-Content -LiteralPath $manifestCopyPath -Encoding UTF8

Compress-Archive -Path (Join-Path $stageRoot "*") -DestinationPath $zipPath -Force

Write-Host "Built DLLs:      $binDir"
Write-Host "Staged package:  $stageRoot"
Write-Host "Zip package:     $zipPath"
