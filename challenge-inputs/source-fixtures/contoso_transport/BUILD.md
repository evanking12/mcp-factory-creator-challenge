# contoso_transport - Build Instructions

## What this fixture is

`contoso_transport.dll` is the hard-tier struct and opaque-handle fixture.

It exists to stress:

- struct inputs
- struct outputs
- opaque numeric session handles
- producer-consumer chains that are not string-shaped
- alloc/free-style lifecycle through blob tokens

## Compile

```powershell
powershell -ExecutionPolicy Bypass -File .\build-transport.ps1
```

## Dependency staging

The build script copies:

- `contoso_config.dll`
- `contoso_cs.dll`

next to the output DLL so the hard-tier transport surface keeps its config and
customer-anchor dependencies available.
