# Batch B Stage Artifacts

- Claim ceiling: partial package-scale diagnostic replay; closeout stalled
- Public artifact mode: sanitized derived sidecars only
- Live execution: not hosted

## Floor 0 Package admission

Show package-scale intake and dependency context before treating any runtime target as product proof.

### Consumes
- `fixture package manifest` (pass, derived_fields_only, source=actual_artifact): Captures package shape, dependency context, and target roles.

### Emits
- `package intake summary` (pass, available, source=derived_summary): 8 bundled binary entries represented without publishing binaries.

### Proves
- `package boundary` (partial, available, source=derived_summary): Batch B is partial package-scale diagnostic evidence: live campaign rows exist, but closeout stalled.

## Floor 1 Runtime target selection

Separate dependency-only binaries from the curated runtime targets that would receive deeper analysis.

### Consumes
- `bundled binary role list` (pass, derived_fields_only, source=actual_artifact): Preserves role and confidence labels from the package manifest.

### Emits
- `selected runtime target list` (pass, available, source=derived_summary): 8 runtime targets selected from 8 bundled entries.

### Proves
- `target selection rationale` (pass, available, source=derived_summary): Shows why package breadth does not imply every binary is deeply probed immediately.

## Floor 2 Package-scale blocker harvest

Show the partial live package run: most rows reached done, but one reporting row stalled in closeout/status propagation.

### Consumes
- `partial live Batch B campaign` (partial, derived_fields_only, source=actual_artifact): Campaign b32b323a reached 7/8 terminal rows; one row remained closeout-stalled.

### Emits
- `partial diagnostic package replay` (warn, available, source=derived_summary): Conservatively marks Batch B as partial diagnostic: useful package-scale pressure, not product-green proof.

### Proves
- `closeout stalled boundary` (warn, available, source=derived_summary): Prevents 7/8 package progress and verified findings from being mistaken for clean comparable closeout.

## Floor 3 Reporting closeout stall

Localize the Batch B blocker to campaign closeout/status propagation after the reporting row produced findings.

### Consumes
- `reporting findings signal` (warn, derived_fields_only, source=actual_artifact): contoso_reporting wrote findings, so the blocker is not simply missing analyzer output.

### Emits
- `closeout/status blocker` (warn, available, source=derived_summary): The row stayed exploring/running after findings, so the public claim must remain diagnostic.

### Proves
- `fail-closed package claim` (warn, available, source=derived_summary): 7/8 completed rows and verified annotations are useful pressure evidence, but they do not satisfy clean Batch B closeout.
