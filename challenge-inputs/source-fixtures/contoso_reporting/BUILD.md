# contoso_reporting - Build Instructions

## What this is
A deliberately undocumented Win32 DLL simulating a mid-2000s reporting and
export surface layered on top of the existing Contoso workflow stack.

The fixture is intended to stress:

- report-job state machines
- output-buffer heavy APIs
- reservation/workflow reuse
- format-code ambiguity

Like the rest of the Contoso family, the source stays in a period-compatible C
subset even though the current local builder is a modern MSVC fallback.

## Compile
```cmd
powershell -ExecutionPolicy Bypass -File build-reporting.ps1
```

## Dependency staging
The build script copies:

- `contoso_cs.dll`
- `contoso_payments.dll`
- `contoso_workflow.dll`

next to the output DLL so upload-backed runs can stage the full interwoven set.
