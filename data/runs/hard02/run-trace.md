# Hard02 Replay Trace

- Claim ceiling: standing-baseline signal, not public replay
- Source: canonical hard-family signal docs; not replay-backed
- Telemetry claim: telemetry-shaped metrics are planned diagnostic expansion only

## Spans

- `S03-S07` Frontier pressure signal: diagnostic (0 sec)

## Metrics

- `first_failed_boundary` = `not_bundled_public_replay` (warn) - Prevents downstream missing artifacts from being mistaken for the root cause.
- `root_blocker` = `not bundled as a sanitized public replay; current truth marks a comparable finish-line signal but not product-green promotion` (warn) - Names the owning layer for the next bounded fix.
- `candidate_comparability_state` = `not_public_replay_comparable` (warn) - Distinguishes diagnostic output from evidence that can support comparison.
- `safe_to_compare` = `False` (warn) - Protects against false benchmark or regression claims.
- `next_action_confidence` = `medium` (warn) - Tells the operator whether to fix, rerun, inspect, or stop.
- `mcp_factory.run.status_staleness` = `unavailable` (unavailable) - Tells an operator whether a run is alive, stale, or misleading.
- `mcp_factory.pipeline.stage_span.duration` = `unavailable` (unavailable) - Shows where time is spent so expensive reruns are not used blindly.
- `mcp_factory.runtime.discovery_child_wait.duration` = `unavailable` (unavailable) - Separates analyzer child wait from bridge wait, prelaunch, and closeout.
- `mcp_factory.runtime.subprocess_interrupts.total` = `unavailable` (unavailable) - Shows whether failures are clustering in runtime or lane infrastructure instead of product behavior.
- `mcp_factory.artifacts.required_emission_lag.duration` = `unavailable` (unavailable) - Separates a completed stage from evidence that is actually readable.
- `mcp_factory.pipeline.checkpoint.save.duration` = `unavailable` (unavailable) - Shows whether checkpoint persistence is cheap enough for fast iteration.
- `mcp_factory.pipeline.checkpoint.resume_seeded_findings` = `unavailable` (unavailable) - Shows how much downstream work checkpoint resume avoided.
