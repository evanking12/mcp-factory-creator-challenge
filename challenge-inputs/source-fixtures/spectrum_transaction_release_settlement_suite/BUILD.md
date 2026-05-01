# Spectrum Transaction Release Settlement Suite - Build Instructions

## What this fixture is

`spectrum_transaction_release_settlement_suite` is the first compiled
proper-named Spectrum suite.

It exists to turn the authored Spectrum semantic family into a real binary
package that can be fed through the pipeline's binary-entry lane.

Current proof scope:

- compile the full 11-DLL family
- stage a package-real install folder
- prove `easy-01` first through `upload`

Current classification:

- Spectrum source family under `docs/.../SpectrumDesignSuite_Source/` =
  `architecture-proof`
- Northstar = `architecture-proof`
- this compiled Spectrum package = the first intended `product-valid` Spectrum
  proving surface

## Active easy-01 DLL subset

The first compiled ingest proof exercises:

- `spectrum_session_bootstrap.dll`
- `spectrum_reservation_stage.dll`
- `spectrum_release_commit.dll`
- `spectrum_audit_trace.dll`

The remaining DLLs are bundled in the package and compiled in v1, but are held
for later medium/hard expansion.

## Build

Preferred:

```powershell
powershell -ExecutionPolicy Bypass -File .\build-package.ps1
```

This script:

- locates an MSVC toolchain
- compiles all 11 DLLs into `bin/`
- mirrors them into the upload directory when available
- stages the package into `.tmp/fixture-packages/`
- emits `campaign-request.json` and `attach-plan.json`
- zips the staged package

## Output locations

- built DLLs: `tests/fixtures/spectrum_transaction_release_settlement_suite/bin/`
- staged package: `.tmp/fixture-packages/spectrum-transaction-release-settlement-suite/`
- zipped package: `.tmp/fixture-packages/spectrum-transaction-release-settlement-suite.zip`

## Ghidra-facing design notes

Each DLL intentionally includes:

- exported symbols with stable semantic names
- deterministic error paths
- semantic token strings such as:
  - `sp-operator-*`
  - `sp-resv-*`
  - `sp-hold-*`
  - `sp-att-*`
  - `sp-branch-*`
- cross-DLL `LoadLibraryA` / `GetProcAddress` seams where the authored suite
  implies downstream coupling

This is deliberate. The binaries are meant to be statically meaningful, not
just loadable.
