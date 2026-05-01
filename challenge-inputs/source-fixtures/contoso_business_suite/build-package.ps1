param(
    [string]$ManifestPath = ""
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

function Ensure-BundledBinary {
    param([object]$Dll)

    $sourcePath = Resolve-RepoPath $Dll.source_path
    if (Test-Path $sourcePath) {
        return $sourcePath
    }

    $buildScript = Get-OptionalProperty $Dll "build_script"
    if ([string]::IsNullOrWhiteSpace([string]$buildScript)) {
        throw "Missing source DLL for package fixture: $($Dll.source_path)"
    }

    $buildScriptPath = Resolve-RepoPath ([string]$buildScript)
    if (-not (Test-Path $buildScriptPath)) {
        throw "Missing build script for package fixture: $buildScript"
    }

    Write-Host "Source DLL missing for $($Dll.staged_name); building via $buildScript"
    & powershell -ExecutionPolicy Bypass -File $buildScriptPath | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "Build script failed for $($Dll.staged_name): $buildScript"
    }

    if (-not (Test-Path $sourcePath)) {
        throw "Build script completed but source DLL is still missing: $($Dll.source_path)"
    }
    return $sourcePath
}

$stageRoot = Resolve-RepoPath $manifest.stage_root
$zipPath = Resolve-RepoPath $manifest.zip_path
$binDir = Join-Path $stageRoot "bin"

if (Test-Path $stageRoot) {
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
}
if (Test-Path $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}

New-Item -ItemType Directory -Path $binDir -Force | Out-Null

$bundledLookup = @{}
foreach ($dll in $manifest.bundled_dlls) {
    $sourcePath = Ensure-BundledBinary $dll
    $destPath = Join-Path $binDir $dll.staged_name
    Copy-Item -LiteralPath $sourcePath -Destination $destPath -Force
    $bundledLookup[$dll.staged_name] = [ordered]@{
        staged_name = $dll.staged_name
        staged_path = $destPath
        role = $dll.role
        dependency_confidence = $dll.dependency_confidence
        selection_reason = (Get-OptionalProperty $dll "selection_reason")
        source_path = $sourcePath
    }
}

foreach ($supportFile in $manifest.support_files) {
    $destPath = Join-Path $stageRoot $supportFile.relative_path
    Ensure-ParentDirectory $destPath
    [System.IO.File]::WriteAllText($destPath, [string]$supportFile.contents, [System.Text.Encoding]::UTF8)
}

$attachPlan = @()
foreach ($target in $manifest.selected_runtime_targets) {
    if (-not $bundledLookup.ContainsKey($target.staged_name)) {
        throw "Selected runtime target missing from bundled_dlls: $($target.staged_name)"
    }
    $dllInfo = $bundledLookup[$target.staged_name]
    $attachPlan += [ordered]@{
        staged_name = $target.staged_name
        staged_path = $dllInfo.staged_path
        selection_reason = $target.selection_reason
        dependency_confidence = $target.dependency_confidence
        lane_id = $target.lane_id
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

Write-Host "Staged package: $stageRoot"
Write-Host "Zip package:    $zipPath"
Write-Host "Campaign JSON:  $campaignRequestPath"
Write-Host "Attach plan:    $attachPlanPath"
