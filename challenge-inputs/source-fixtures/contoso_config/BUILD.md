# contoso_config - Build Instructions

## What this fixture is

`contoso_config.dll` is the first hard-tier fixture.

It exists to stress:

- environment-gated initialization
- controlled external config artifacts
- mode-dependent capability (`trial` vs `licensed`)
- runtime classification of environment failure vs bad-argument failure

The external artifact is a local config file written next to the DLL:

- `contoso_config.ini`

This keeps the challenge honest without introducing machine-global side effects.

## Compile

```powershell
powershell -ExecutionPolicy Bypass -File .\build-config.ps1
```

## Dependency staging

The build script copies:

- `contoso_cs.dll`

next to the output DLL so the hard-tier family keeps the shared Contoso
customer identity surface nearby.
