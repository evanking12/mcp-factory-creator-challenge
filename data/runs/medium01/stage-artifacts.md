# Medium01 Stage Artifacts

- Claim ceiling: product-green baseline replay
- Public artifact mode: sanitized derived sidecars only
- Live execution: not hosted

## S00 Compiled/no-source admission

Accept the compiled binary fixture as the product-route input and keep authored source out of the route.

### Consumes
- `compiled binary fixture input` (pass, described_not_downloaded, source=actual_artifact): The replay represents compiled/no-source analysis. Public demo does not publish binaries.
- `synthetic source ground truth` (warn, not_route_input, source=derived_summary): Source exists to judge fixture correctness, but the product route does not receive it. unavailable_reason=Not exposed as product-route evidence because it is not the route input.

### Emits
- `compiled admission summary` (pass, available, source=derived_summary): Records the no-source route boundary used by the replay.

### Proves
- `source boundary` (pass, available, source=derived_summary): Prevents the demo from overclaiming source-assisted recovery.

## S00B-S02 Ghidra-backed route authority

Prove the intended compiled-analysis route was entered before downstream artifacts are trusted.

### Consumes
- `compiled ingest summary` (pass, derived_fields_only, source=actual_artifact): Provides sanitized route, lane, bridge, and readiness fields.

### Emits
- `route authority evidence` (pass, available, source=derived_summary): route_authority_present=True.
- `recovery backend boundary` (pass, available, source=derived_summary): ghidra_required=True.

### Proves
- `compiled route trust` (pass, available, source=derived_summary): Downstream target evidence can be read only if route authority is present.

## S02-S05 Target analysis runs

Replay per-target analysis results and expose the recovered run evidence without publishing raw sessions.

### Consumes
- `target run records` (pass, derived_fields_only, source=actual_artifact): 5 target records available in sanitized form.

### Emits
- `target evidence table` (pass, available, source=derived_summary): 5/5 target records are valid_run=true.
- `recovered symbols and invocables` (pass, available, source=derived_summary): Recovered symbol flow is summarized for inspection without raw analyzer output.

### Proves
- `run evidence breadth` (pass, available, source=derived_summary): Shows whether the run has enough per-target evidence to support comparison.

## S03-S06 Semantic checkpoint comparison

Compare expected semantic checkpoints against recovered symbols and run evidence.

### Consumes
- `compiled ingest comparison report` (pass, derived_fields_only, source=actual_artifact): Provides expected, discovered, and semantic symbol-flow summaries.

### Emits
- `semantic checkpoint alignment` (pass, available, source=derived_summary): semantic_checkpoint_alignment_succeeded=True.
- `gap summary` (pass, available, source=derived_summary): semantic_gaps=0, embodiment_gaps=0.

### Proves
- `safe comparison precondition` (pass, available, source=derived_summary): Shows whether recovered behavior aligns with the expected checkpoint sequence.

## S07 Claim ceiling and closeout

Emit the final replay claim ceiling, comparability state, and next action.

### Consumes
- `stage trace and metrics` (pass, available, source=derived_summary): Final claim uses sanitized trace, metrics, and comparison summaries.

### Emits
- `final claim ceiling` (pass, available, source=derived_summary): product-green baseline replay
- `first failed boundary` (pass, available, source=derived_summary): first_failed_boundary=none.

### Proves
- `safe-to-compare state` (pass, available, source=derived_summary): safe_to_compare=True.
