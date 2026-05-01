# contoso_events - Build Instructions

## What this fixture is

`contoso_events.dll` is the hard-tier callback and flag-space fixture.

It exists to stress:

- callback registration and enumeration
- small enum-space discovery
- flag-mask exploration
- honest classification of currently hard or unprobeable shapes

## Compile

```powershell
powershell -ExecutionPolicy Bypass -File .\build-events.ps1
```

## Dependency staging

The build script copies:

- `contoso_transport.dll`
- `contoso_config.dll`

next to the output DLL so the callback/flags surface remains tied to the
hard-tier transport and config world.
