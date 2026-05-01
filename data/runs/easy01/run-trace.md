# Easy01 Replay Trace

- Claim ceiling: product-green baseline replay
- Source: real pinned baseline artifacts
- Telemetry claim: telemetry is diagnostic evidence only

## Spans

- `S00` Compiled/no-source admission: pass (0 sec)
- `S00B-S02` Ghidra-backed route authority: pass (115.52 sec)
- `S02-S05` Target analysis runs: pass (330.05 sec)
- `S03-S06` Semantic checkpoint comparison: pass (16.5 sec)
- `S07` Claim ceiling and closeout: pass (0 sec)

## Metrics

- `mcp_factory.pipeline.stage_span.duration` = `330.05` (pass) - Shows where time is spent so expensive reruns are not used blindly.
- `mcp_factory.run.status_staleness` = `0` (pass) - Tells an operator whether a run is alive, stale, or misleading.
- `mcp_factory.artifacts.required_emission_lag.duration` = `0` (pass) - Separates a completed stage from evidence that is actually readable.
- `first_failed_boundary` = `none` (pass) - Prevents downstream missing artifacts from being mistaken for the root cause.
- `root_blocker` = `none` (pass) - Names the owning layer for the next bounded fix.
- `candidate_comparability_state` = `comparable` (pass) - Distinguishes diagnostic output from evidence that can support comparison.
- `safe_to_compare` = `True` (pass) - Protects against false benchmark or regression claims.
- `next_action_confidence` = `high` (pass) - Tells the operator whether to fix, rerun, inspect, or stop.
- `mcp_factory.runtime.discovery_child_wait.duration` = `unavailable` (unavailable) - Separates analyzer child wait from bridge wait, prelaunch, and closeout.
- `mcp_factory.runtime.subprocess_interrupts.total` = `unavailable` (unavailable) - Shows whether failures are clustering in runtime or lane infrastructure instead of product behavior.
- `mcp_factory.pipeline.checkpoint.save.duration` = `unavailable` (unavailable) - Shows whether checkpoint persistence is cheap enough for fast iteration.
- `mcp_factory.pipeline.checkpoint.resume_seeded_findings` = `unavailable` (unavailable) - Shows how much downstream work checkpoint resume avoided.
