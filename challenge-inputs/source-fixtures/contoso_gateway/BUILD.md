# contoso_gateway - Build Instructions

## What this fixture is

`contoso_gateway.dll` is the hard-tier reduced-discovery and wide-string
fixture.

It exists to stress:

- `WCHAR` / `LPWSTR` marshalling
- weaker export clarity
- gateway-style translation around transport handles
- dependence on cleaner upstream transport state while presenting a different
  surface shape

## Compile

```powershell
powershell -ExecutionPolicy Bypass -File .\build-gateway.ps1
```

## Dependency staging

The build script copies:

- `contoso_transport.dll`
- `contoso_cs.dll`

next to the output DLL so the gateway lane keeps its harder transport world
available locally.
