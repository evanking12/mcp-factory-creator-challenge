# contoso_cs_interwoven - Build Instructions

## What this fixture is
`contoso_cs_interwoven.dll` is a deterministic stress fixture that depends on
`contoso_cs.dll` at runtime. It enforces call-order prerequisites and forwards
critical operations into the base Contoso DLL.

This lets the pipeline test:
- init/bind/prime prerequisite discovery,
- write/read dependency handling,
- sentinel interpretation,
- retry + regression behavior.

## Runtime dependency
`contoso_cs.dll` must be loadable by `LoadLibraryA("contoso_cs.dll")`.

Simplest setup:
- Place both DLLs in the same directory before testing, or
- Put that directory on `PATH`.

## Exports (high-level)
- `CI_Initialize`
- `CI_BindCustomer`
- `CI_GetExpectedNonce`
- `CI_PrimeSession`
- `CI_GetUnlockTokenHex`
- `CI_GetCompositeState`
- `CI_EarnThenRedeem`
- `CI_UnlockAndDebit`
- `CI_GetVersionBridge`
- `CI_ResetSession`
- `CI_GetLastErrorText`
- `CI_GetFixtureDiagnostics`

## Build (MSVC, preferred)
From "x64 Native Tools Command Prompt for VS":

```cmd
cl /O2 /GS- /LD /W3 contoso_cs_interwoven.c /Fe:contoso_cs_interwoven.dll
```

## Build (MinGW fallback)
```cmd
gcc -O2 -shared -o contoso_cs_interwoven.dll contoso_cs_interwoven.c -Wl,--out-implib,libcontoso_cs_interwoven.a
```

## Quick smoke test intent
1. `CI_Initialize()` -> 0
2. `CI_BindCustomer("CUST-001")` -> 0
3. `CI_GetExpectedNonce(&n)` -> 0
4. `CI_PrimeSession(n)` -> 0
5. `CI_GetUnlockTokenHex(buf, len)` -> `"E5414243"`
6. `CI_EarnThenRedeem("CUST-001", 1000, 10, &out)` -> 0

If step 6 fails with `-1004`, the prime/bind preconditions were not satisfied.

