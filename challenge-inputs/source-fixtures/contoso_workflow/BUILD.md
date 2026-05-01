# contoso_workflow - Build Instructions

## What this fixture is

`contoso_workflow.dll` is a deterministic workflow / fulfillment stress
fixture that depends on:

- `contoso_cs.dll`
- `contoso_payments.dll`

at runtime.

It enforces:

- customer binding
- reservation creation
- workflow-handle passing
- nonce/confirmation sequencing
- shipment commit or cancellation

This lets the pipeline test:

- multi-step workflow-state discovery
- handle reuse across functions
- producer/consumer state across DLLs
- whether the pipeline can distinguish:
  - missing upstream state
  - bad handle
  - bad nonce
  - downstream business failure

## Runtime dependencies

The following DLLs must be loadable with `LoadLibraryA`:

- `contoso_cs.dll`
- `contoso_payments.dll`

Simplest setup:

- place all DLLs in the same directory before testing, or
- put that directory on `PATH`

## Exported functions (high-level)

- `WF_Initialize`
- `WF_BindCustomer`
- `WF_CreateReservation`
- `WF_OpenFulfillment`
- `WF_GetWorkflowNonce`
- `WF_ConfirmWorkflow`
- `WF_CommitShipment`
- `WF_GetWorkflowState`
- `WF_GetReservationEcho`
- `WF_CancelWorkflow`
- `WF_ResetWorkflow`
- `WF_GetDiagnostics`

## Build (MSVC, preferred)

From "x64 Native Tools Command Prompt for VS":

```cmd
cl /O2 /GS- /LD /W3 contoso_workflow.c /Fe:contoso_workflow.dll
```

## Build (helper script)

```powershell
powershell -ExecutionPolicy Bypass -File .\build-workflow.ps1
```

## Quick smoke test intent

1. `WF_Initialize()` -> `0`
2. `WF_BindCustomer("CUST-001")` -> `0`
3. `WF_CreateReservation("ACC-USB-C", 2, 30, out, len)` -> `0`
4. `WF_OpenFulfillment(resv, out_handle, len)` -> `0`
5. `WF_GetWorkflowNonce(handle, &n)` -> `0`
6. `WF_ConfirmWorkflow(handle, n)` -> `0`
7. `WF_CommitShipment(handle, &auth)` -> `0`

If step 7 fails with `-2004`, the reservation / confirmation preconditions were
not satisfied.
