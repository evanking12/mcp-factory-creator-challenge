# Challenge Inputs

This folder exposes the public, reviewable side of the fixture inputs used by
the replay demo.

## What Is Included

- `source-fixtures/`: synthetic source fixtures, truth manifests, package
  manifests, and build scripts.
- `compiled-artifacts.json`: metadata and SHA-256 hashes for compiled artifacts
  used by the private/local product route.

## What Is Not Included

Raw compiled binaries are not bundled in this public static demo. The replay
site is meant to be safe to host on GitHub Pages: no browser-side binary
execution, no Ghidra bridge, no model credential, and no live backend.

## Boundary

The source files are included for human inspection and fixture transparency.
They are not the product-route input. MCP Factory's product route is designed
around compiled/no-source artifacts; source exists here only so reviewers can
understand the synthetic ground truth and build recipe.
