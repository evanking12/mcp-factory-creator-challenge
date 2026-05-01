# contoso_pricing - Build Instructions

## What this is
A deliberately undocumented Win32 DLL simulating a 2004-2006 pricing and rule
engine that sits beside the Contoso customer-services and payments libraries.

It is authored in a mid-2000s-compatible C style:

- fixed buffers
- sentinel return codes
- mixed `cdecl` and `stdcall`
- global state and staged workflow guards

The source is written to a 2005-era realism profile, but the current local
builder is a modern MSVC fallback because a true 2005 toolchain is not
installed on this machine.

## Compile
```cmd
powershell -ExecutionPolicy Bypass -File build-pricing.ps1
```

## Dependency staging
The build script copies:

- `contoso_cs.dll`
- `contoso_payments.dll`

next to the output DLL so upload-backed runs can stage sibling dependencies.
