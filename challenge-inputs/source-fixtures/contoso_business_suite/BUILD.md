# contoso_business_suite - Package Fixture Build

## What this fixture is

`contoso_business_suite` is the package-mode proving ground for the
blocker-harvest methodology.

It does not replace the single-DLL truth fixtures. It stages a small
install-style folder that keeps real Contoso sibling dependencies together and
preselects a curated runtime subset for campaign testing.

The first-wave runtime targets are:

- `contoso_config.dll`
- `contoso_transport.dll`
- `contoso_workflow.dll`
- `contoso_reporting.dll`

Those targets intentionally mix:

- environment gating
- struct/payload shaping
- state and prerequisite sequencing
- output/readback and artifact chaining

## Build the staged package

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\fixtures\contoso_business_suite\build-package.ps1
```

To stage one of the cumulative Wave 2 manifests, pass it explicitly:

```powershell
powershell -ExecutionPolicy Bypass -File .\tests\fixtures\contoso_business_suite\build-package.ps1 -ManifestPath .\tests\fixtures\contoso_business_suite\package_manifest_batch_a.json
```

The script stages the package under:

- `.tmp/fixture-packages/contoso-business-suite`

and creates:

- `.tmp/fixture-packages/contoso-business-suite.zip`
- `.tmp/fixture-packages/contoso-business-suite/campaign-request.json`
- `.tmp/fixture-packages/contoso-business-suite/attach-plan.json`

## What the generated files are for

- `campaign-request.json`
  - package metadata for the local campaign creation route
- `attach-plan.json`
  - the curated compiled-artifact subset for the local campaign attach step

This keeps package-mode testing reproducible and prevents operator drift when
running blocker-harvest campaigns.

## Included manifest variants

- `package_manifest.json`
  - Wave 1 proving set (`config`, `transport`, `workflow`, `reporting`)
- `package_manifest_batch_a.json`
  - Wave 2 Batch A (`+ pricing`, `+ gateway`)
- `package_manifest_batch_b.json`
  - Wave 2 Batch B (`+ events`, `+ interwoven`)
- `package_manifest_batch_c.json`
  - Wave 2 Batch C full package (`+ cs`, `+ payments`)

## Why this is the right proving ground

Use this fixture before broader external-package harvesting because it preserves:

- truthful fixture dependencies
- a controlled runtime subset
- known blocker-family coverage
- low ambiguity when comparing package-mode and single-DLL results
