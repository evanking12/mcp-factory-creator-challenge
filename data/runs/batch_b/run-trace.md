# Batch B Replay Trace

- Claim ceiling: partial package-scale diagnostic replay; closeout stalled
- Source: partial live Batch B campaign artifacts
- Telemetry claim: telemetry is diagnostic evidence only

## Spans

- `Floor 0` Package admission: pass (0 sec)
- `Floor 1` Runtime target selection: pass (0 sec)
- `Floor 2` Batch B campaign execution: partial (0 sec)
- `Floor 3` Reporting closeout: warn (0 sec)

## Metrics

- `bundled_dll_count` = `10` (pass) - Shows package-scale intake breadth before expensive deep analysis.
- `runtime_target_count` = `8` (pass) - Shows selective activation: not every DLL is deeply probed immediately.
- `dependency_confidence_counts` = `{'high': 9, 'medium': 1}` (pass) - Shows that package intake preserves confidence, dependency, and role labels.
- `candidate_comparability_state` = `partial_closeout_stalled` (warn) - Distinguishes diagnostic output from evidence that can support comparison.
- `next_action_confidence` = `medium` (pass) - Tells the operator whether to fix, rerun, inspect, or stop.
- `batch_b_completed_rows` = `7/8` (partial) - Shows the useful partial campaign result without implying clean campaign closeout.
- `verified_success_annotations` = `89` (partial) - Counts verified_success annotations in per-job invocable descriptions; useful for presentation, weaker than a final campaign audit.
- `artifact_backed_findings_verified` = `20` (partial) - Counts verified findings from jobs that wrote findings files before closeout stalled.
- `captured_kpi_verified` = `11` (pass) - Shows the strongest clean captured row available in this partial run.
- `mcp_factory.run.status_staleness` = `unavailable` (unavailable) - Tells an operator whether a run is alive, stale, or misleading.
- `mcp_factory.pipeline.stage_span.duration` = `unavailable` (unavailable) - Shows where time is spent so expensive reruns are not used blindly.
- `mcp_factory.runtime.discovery_child_wait.duration` = `unavailable` (unavailable) - Separates analyzer child wait from bridge wait, prelaunch, and closeout.
- `mcp_factory.runtime.subprocess_interrupts.total` = `unavailable` (unavailable) - Shows whether failures are clustering in runtime or lane infrastructure instead of product behavior.
- `mcp_factory.artifacts.required_emission_lag.duration` = `unavailable` (unavailable) - Separates a completed stage from evidence that is actually readable.
- `mcp_factory.pipeline.checkpoint.save.duration` = `unavailable` (unavailable) - Shows whether checkpoint persistence is cheap enough for fast iteration.
- `mcp_factory.pipeline.checkpoint.resume_seeded_findings` = `unavailable` (unavailable) - Shows how much downstream work checkpoint resume avoided.
- `first_failed_boundary` = `unavailable` (unavailable) - Prevents downstream missing artifacts from being mistaken for the root cause.
- `root_blocker` = `unavailable` (unavailable) - Names the owning layer for the next bounded fix.
