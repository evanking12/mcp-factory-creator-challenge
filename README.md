# MCP Factory Creator Challenge Replay

Static GitHub Pages demo for the Codex Creator Challenge.

MCP Factory makes opaque compiled enterprise systems usable by AI agents by
recovering evidence, turning it into structured artifacts, and refusing to trust
outputs that cannot be verified.

## Demo Boundary

This repository is public and static by design.

- No hosted backend.
- No hosted model credential.
- No hosted Ghidra bridge.
- No browser-side binary execution.
- No raw compiled binaries.

The site replays sanitized artifacts from real local MCP Factory runs. Live
analysis remains local because it requires user-owned credentials,
binary-analysis dependencies, and safe execution boundaries.

## What To Open

Open `index.html` through GitHub Pages or serve it locally:

```powershell
python -m http.server 8088
```

Then open:

```text
http://127.0.0.1:8088/
```

## What Is Included

- `assets/`: static cockpit UI.
- `data/`: sanitized replay JSON and Markdown artifacts.
- `challenge-inputs/source-fixtures/`: inspectable synthetic fixture source,
  truth manifests, package manifests, and build notes.
- `challenge-inputs/compiled-artifacts.json`: compiled artifact metadata and
  SHA-256 hashes. Raw compiled artifacts are intentionally not bundled.
- `CHALLENGE_NARRATIVE.md`: concise challenge narrative.

## Claim Ceiling

This demo can claim static replay of real local artifacts and an evidence-gated
UI for compiled/no-source analysis.

It cannot claim live hosted product execution or product-green status from
telemetry alone.
