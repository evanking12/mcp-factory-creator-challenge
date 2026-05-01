# Hard02 Stage Artifacts

- Claim ceiling: standing-baseline signal, not public replay
- Public artifact mode: sanitized derived sidecars only
- Live execution: not hosted

## S03-S07 Frontier signal boundary

Represent a known hard-family frontier without pretending it is a sanitized public replay run.

### Consumes
- `hard-family signal map` (warn, sanitized_reference, source=derived_summary): Names the current hard-family pressure and blocker routing without publishing raw sessions.

### Emits
- `Hard02 frontier status` (warn, included, source=derived_summary): not bundled as a sanitized public replay; current truth marks a comparable finish-line signal but not product-green promotion

### Proves
- `claim ceiling` (warn, included, source=derived_summary): standing-baseline signal, not public replay
