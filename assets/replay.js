const state = {
  index: null,
  runs: new Map(),
  selectedRunId: null,
  selectedStageIndex: 0,
  filter: "",
  view: "overview",
};

const $ = (id) => document.getElementById(id);

function escapeHtml(value) {
  return String(value ?? "")
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

function formatBool(value) {
  return value ? "yes" : "no";
}

function formatDuration(value) {
  const n = Number(value || 0);
  if (!n) return "n/a";
  if (n < 90) return `${n.toFixed(1)}s`;
  return `${Math.floor(n / 60)}m ${Math.round(n % 60)}s`;
}

function statusClass(value) {
  const text = String(value ?? "").toLowerCase();
  if (value === true || text === "pass" || text === "ok" || text === "comparable") return "pass";
  if (value === false || text === "fail" || text === "failed" || text === "blocked") return "fail";
  if (text.includes("partial") || text.includes("mostly")) return "partial";
  if (text.includes("diagnostic") || text.includes("frontier") || text === "warn" || text === "unavailable") return "warn";
  return "";
}

async function getJson(path) {
  const response = await fetch(path);
  if (!response.ok) throw new Error(`${path} -> ${response.status}`);
  return response.json();
}

function filteredRuns() {
  const needle = state.filter.trim().toLowerCase();
  if (!needle) return state.index.runs;
  return state.index.runs.filter((run) =>
    [run.label, run.tier, run.difficulty, run.claim_ceiling].some((field) =>
      String(field || "").toLowerCase().includes(needle)
    )
  );
}

function renderRunList() {
  const root = $("run-list");
  const runs = filteredRuns();
  $("run-count").textContent = String(runs.length);
  $("overview-button").className = `run-button overview-button ${state.view === "overview" ? "active" : ""}`;
  root.innerHTML = "";
  runs.forEach((run) => {
    const button = document.createElement("button");
    button.className = `run-button ${state.view === "run" && run.id === state.selectedRunId ? "active" : ""}`;
    button.type = "button";
    button.innerHTML = `
      <strong>${escapeHtml(run.label)}</strong>
      <span>${escapeHtml(run.difficulty)}</span>
      <span>${run.target_count ?? 0} targets - ${formatDuration(run.duration_sec)}</span>
    `;
    button.addEventListener("click", () => selectRun(run.id));
    root.appendChild(button);
  });
  if (!runs.length) {
    root.innerHTML = `<div class="muted">No replay runs match the filter.</div>`;
  }
}

function renderOverview() {
  state.view = "overview";
  state.selectedRunId = null;
  $("overview-view").hidden = false;
  $("run-view").hidden = true;
  $("run-subtitle").textContent = "Overview / Trust Model";
  renderRunList();
}

function renderSummary(run) {
  $("run-subtitle").textContent = `${run.label} - ${run.claim_ceiling}`;
  $("run-title").textContent = run.label;
  $("run-summary").textContent = `${run.difficulty}. ${run.why_it_matters}`;
  $("pipeline-state").textContent = run.claim_ceiling;
  $("footer-claim").textContent = run.otel?.claim_ceiling || state.index.telemetry_claim_ceiling;

  $("metric-targets").textContent = String(run.summary.target_count ?? 0);
  $("metric-verified").textContent = run.summary.discovered_total
    ? `${run.summary.verified_total ?? 0} / ${run.summary.discovered_total}`
    : String(run.summary.verified_total ?? "n/a");
  $("metric-verified").className = statusClass(run.summary.verified_total ? "partial" : "warn");
  $("metric-duration").textContent = formatDuration(run.summary.total_duration_sec);
  $("metric-compare").textContent = formatBool(run.summary.safe_to_compare);
  $("metric-compare").className = statusClass(run.summary.safe_to_compare);
  $("metric-green").textContent = run.summary.green_contract_level
    ? `replayed ${run.summary.green_contract_level}`
    : "n/a";
  $("metric-green").className = run.summary.product_green_eligible ? "pass" : "warn";
}

function renderPipeline(run) {
  const root = $("pipeline");
  const spans = run.otel?.spans || [];
  root.innerHTML = "";
  spans.forEach((span, index) => {
    const button = document.createElement("button");
    button.type = "button";
    button.className = `stage ${statusClass(span.status)} ${index === state.selectedStageIndex ? "active" : ""}`;
    button.innerHTML = `
      <div class="stage-code">${escapeHtml(span.stage || "stage")}</div>
      <div class="stage-name">${escapeHtml(span.name)}</div>
      <div class="stage-state">${escapeHtml(span.status || "unknown")}</div>
    `;
    button.addEventListener("click", () => {
      state.selectedStageIndex = index;
      renderPipeline(run);
      renderStageDetails(run);
      renderStageArtifacts(run);
      renderStageExplanation(run);
      renderLog(run);
    });
    root.appendChild(button);
  });
}

function artifactNames(cards) {
  const names = (cards || []).map((card) => card.name).filter(Boolean);
  return names.length ? names.join(", ") : "none represented";
}

function failureHint(run, span, group) {
  const status = statusClass(span?.status);
  if (status === "fail") return span?.message || run.summary?.blocking_signal || "Selected stage is blocked.";
  if (!group) return "No stage artifact contract is available for this replay.";
  if (!(group.consumes || []).length) return "This stage would fail review if consumed evidence is missing.";
  if (!(group.emits || []).length) return "This stage would fail review if it emits no durable artifact.";
  if (!(group.proves || []).length) return "This stage would fail review if it has no proof artifact.";
  return "Missing consumed evidence, missing emitted artifacts, absent proof cards, or a non-pass stage status.";
}

function renderStageExplanation(run) {
  const root = $("stage-explanation");
  const spans = run.otel?.spans || [];
  const span = spans[state.selectedStageIndex] || spans[0] || {};
  const group = selectedStageArtifactGroup(run);
  if (!group) {
    root.innerHTML = `
      <article class="stage-explain-card">
        <strong>No selected-stage explanation</strong>
        <div class="small">This replay has no stage_artifacts group for the selected stage.</div>
      </article>
    `;
    return;
  }
  const items = [
    ["Stage goal", group.purpose],
    ["Consumes", artifactNames(group.consumes)],
    ["Emits", artifactNames(group.emits)],
    ["Proves", artifactNames(group.proves)],
    ["Why it matters", span.message || group.purpose],
    ["What would make it fail", failureHint(run, span, group)],
  ];
  root.innerHTML = items
    .map(
      ([label, value]) => `
        <article class="stage-explain-card">
          <span>${escapeHtml(label)}</span>
          <strong>${escapeHtml(value)}</strong>
        </article>
      `
    )
    .join("");
}

function renderFactList(facts) {
  return (facts || [])
    .map(
      (item) => `
        <div class="mechanic-fact">
          <span>${escapeHtml(item.label)}</span>
          <strong>${escapeHtml(item.value)}</strong>
        </div>
      `
    )
    .join("");
}

function renderPipelineMechanics(run) {
  const root = $("pipeline-mechanics");
  const mechanics = run.pipeline_mechanics || [];
  if (!mechanics.length) {
    root.innerHTML = `<div class="mechanic-card"><strong>No mechanics contract</strong><div class="small">This replay has no pipeline_mechanics section.</div></div>`;
    return;
  }
  root.innerHTML = mechanics
    .map(
      (item) => `
        <article class="mechanic-card ${statusClass(item.status)}">
          <div class="mechanic-top">
            <strong>${escapeHtml(item.title)}</strong>
            <span class="${statusClass(item.status)}">${escapeHtml(item.status)}</span>
          </div>
          <div class="small">${escapeHtml(item.summary)}</div>
          <div class="mechanic-facts">${renderFactList(item.facts)}</div>
          <div class="small">evidence=${escapeHtml(item.evidence_source || "n/a")}</div>
        </article>
      `
    )
    .join("");
}

function renderVerifiability(run) {
  const root = $("verifiability-list");
  const rows = run.verifiability || [];
  if (!rows.length) {
    root.innerHTML = `<div class="verify-row"><strong>No trust gate contract</strong><div class="small">This replay has no verifiability section.</div></div>`;
    return;
  }
  root.innerHTML = rows
    .map(
      (item) => `
        <article class="verify-row ${statusClass(item.status)}">
          <div class="verify-top">
            <strong>${escapeHtml(item.name)}</strong>
            <span class="${statusClass(item.status)}">${escapeHtml(item.status)}</span>
          </div>
          <div class="small">${escapeHtml(item.question)}</div>
          <div>${escapeHtml(item.meaning)}</div>
          <div class="small">source=${escapeHtml(item.evidence_source || "n/a")}</div>
        </article>
      `
    )
    .join("");
}

function logLinesFor(run) {
  const spans = run.otel?.spans || [];
  const metrics = run.otel?.metrics || [];
  const selected = spans[state.selectedStageIndex];
  const lines = [
    `Replay: ${run.label}`,
    `Claim ceiling: ${run.claim_ceiling}`,
    `Source: ${run.generated_from}`,
    `Safe to compare: ${formatBool(run.summary.safe_to_compare)}`,
    "",
    "Stage spans:",
  ];
  if (selected) {
    const group = selectedStageArtifactGroup(run);
    lines.push(`[selected] ${selected.stage} ${selected.name}`);
    lines.push(`status: ${selected.status}`);
    lines.push(`duration: ${formatDuration(selected.duration_sec)}`);
    lines.push(`span_id: ${selected.span_id}`);
    lines.push(`message: ${selected.message}`);
    if (group) {
      lines.push(`artifact purpose: ${group.purpose}`);
      lines.push(
        `artifact counts: consumes=${group.consumes.length}, emits=${group.emits.length}, proves=${group.proves.length}`
      );
    }
  } else {
    spans.forEach((span) => {
      lines.push(
        `[${span.stage}] ${span.name} | ${span.status} | ${formatDuration(span.duration_sec)} | ${span.message}`
      );
    });
  }
  lines.push("", "All stage spans:");
  spans.forEach((span) => {
    lines.push(`[${span.stage}] ${span.name} | ${span.status} | ${formatDuration(span.duration_sec)}`);
  });
  lines.push("", "Decision metrics:");
  metrics
    .filter((metric) =>
      ["first_failed_boundary", "root_blocker", "candidate_comparability_state", "safe_to_compare", "next_action_confidence"].includes(metric.name)
    )
    .forEach((metric) => {
      lines.push(`${metric.name}: ${JSON.stringify(metric.value)} (${metric.status})`);
    });
  return lines;
}

function renderLog(run) {
  const spans = run.otel?.spans || [];
  const selected = spans[state.selectedStageIndex];
  $("log-title").textContent = selected ? `${selected.stage} ${selected.name} Log` : "Replay Evidence Log";
  $("event-count").textContent = `${spans.length} stages`;
  $("log").textContent = logLinesFor(run).join("\n");
}

function detailRow(label, value, className = "") {
  return `
    <div class="detail-row">
      <span>${escapeHtml(label)}</span>
      <strong class="${className}">${escapeHtml(value ?? "n/a")}</strong>
    </div>
  `;
}

function renderStageDetails(run) {
  const spans = run.otel?.spans || [];
  const span = spans[state.selectedStageIndex] || spans[0] || {};
  const summary = run.summary || {};
  $("stage-details").innerHTML = [
    detailRow("Stage", `${span.stage || "n/a"} - ${span.name || "n/a"}`),
    detailRow("State", span.status || "unknown", statusClass(span.status)),
    detailRow("Duration", formatDuration(span.duration_sec)),
    detailRow("Trace ID", run.otel?.trace_id || "n/a"),
    detailRow("Span ID", span.span_id || "n/a"),
    detailRow("First Failed Boundary", summary.first_failed_boundary || "n/a", statusClass(summary.first_failed_boundary === "none")),
    detailRow("Root Blocker", summary.blocking_signal || "none", summary.blocking_signal ? "warn" : "pass"),
    detailRow("Next Action", run.next_action || "n/a"),
    detailRow("Message", span.message || "n/a"),
  ].join("");
}

function renderTelemetry(run) {
  const root = $("metric-list");
  root.innerHTML = "";
  (run.otel?.metrics || []).forEach((metric) => {
    const value = typeof metric.value === "object" ? JSON.stringify(metric.value) : String(metric.value);
    const labels = metric.labels ? Object.entries(metric.labels).slice(0, 6).map(([k, v]) => `${k}=${v}`).join(", ") : "";
    const item = document.createElement("div");
    item.className = "telemetry-item";
    item.innerHTML = `
      <div class="telemetry-top">
        <strong>${escapeHtml(metric.name)}</strong>
        <span class="${statusClass(metric.status)}">${escapeHtml(metric.status)}</span>
      </div>
      <div><code>${escapeHtml(value)}</code> ${escapeHtml(metric.unit || "")}</div>
      <div class="small">${escapeHtml(metric.why)}</div>
      <div class="small">source=${escapeHtml(metric.value_source || "unknown")}${labels ? ` | ${escapeHtml(labels)}` : ""}</div>
      ${metric.unavailable_reason ? `<div class="small">unavailable_reason=${escapeHtml(metric.unavailable_reason)}</div>` : ""}
    `;
    root.appendChild(item);
  });
}

function selectedStageArtifactGroup(run) {
  const spans = run.otel?.spans || [];
  const selected = spans[state.selectedStageIndex] || spans[0] || {};
  const groups = run.stage_artifacts || [];
  return groups.find((group) => group.stage === selected.stage) || groups[0] || null;
}

function renderArtifactCard(card) {
  const link = card.link
    ? `<a class="artifact-link" href="${escapeHtml(card.link)}" target="_blank" rel="noreferrer">open sanitized sidecar</a>`
    : "";
  const path = card.path ? `<div><code>${escapeHtml(card.path)}</code></div>` : "";
  const unavailable = card.unavailable_reason ? `<div class="small">unavailable_reason=${escapeHtml(card.unavailable_reason)}</div>` : "";
  return `
    <article class="stage-artifact-card ${statusClass(card.status)}">
      <strong>${escapeHtml(card.name)}</strong>
      <div class="artifact-meta">
        <span class="artifact-chip">${escapeHtml(card.kind)}</span>
        <span class="artifact-chip">${escapeHtml(card.availability)}</span>
        <span class="artifact-chip">source=${escapeHtml(card.value_source)}</span>
      </div>
      <div class="small">${escapeHtml(card.proof_meaning)}</div>
      ${path}
      ${unavailable}
      ${link}
    </article>
  `;
}

function renderArtifactColumn(title, cards) {
  const body = (cards || []).length
    ? cards.map(renderArtifactCard).join("")
    : `<div class="stage-artifact-card"><strong>none</strong><div class="small">No artifacts represented for this role.</div></div>`;
  return `<div class="artifact-column"><h3>${escapeHtml(title)}</h3>${body}</div>`;
}

function renderStageArtifacts(run) {
  const group = selectedStageArtifactGroup(run);
  const root = $("stage-artifacts");
  if (!group) {
    $("stage-artifact-title").textContent = "Stage Artifacts";
    $("stage-artifact-purpose").textContent = "No stage artifact contract is available for this replay.";
    root.innerHTML = "";
    return;
  }
  $("stage-artifact-title").textContent = `${group.stage} ${group.name} Artifacts`;
  $("stage-artifact-purpose").textContent = group.purpose;
  root.innerHTML = [
    renderArtifactColumn("Consumes", group.consumes),
    renderArtifactColumn("Emits", group.emits),
    renderArtifactColumn("Proves", group.proves),
  ].join("");
}

function renderChallengeProfile(run) {
  const root = $("challenge-profile");
  const profile = run.challenge_profile || {};
  const jobIds = Array.isArray(profile.real_job_ids) ? profile.real_job_ids : [];
  const ladder = (state.index?.challenge_ladder || [])
    .map(
      (item) => `
        <div class="challenge-ladder-row ${statusClass(item.status === "replay-backed" ? "pass" : item.status === "active-blocker" ? "blocked" : "warn")}">
          <div class="ladder-row-head">
            <strong>${escapeHtml(item.label)}</strong>
            <span>${escapeHtml(item.status)}</span>
          </div>
          <div class="ladder-grid">
            <div class="ladder-cell"><span>Intent</span><strong>${escapeHtml(item.intent || item.challenge || "n/a")}</strong></div>
            <div class="ladder-cell"><span>Pipeline Pressure</span><strong>${escapeHtml(item.pipeline_pressure || item.challenge || "n/a")}</strong></div>
            <div class="ladder-cell"><span>Trust Gate</span><strong>${escapeHtml(item.trust_gate || item.claim_ceiling || "n/a")}</strong></div>
            <div class="ladder-cell"><span>Current Status</span><strong>${escapeHtml(item.current_status || item.status || "n/a")}</strong></div>
          </div>
          <div class="ladder-meta">
            <span>stage/window: ${escapeHtml(item.stage_window || "n/a")}</span>
            <span>blocker: ${escapeHtml(item.blocker || "none")}</span>
            <span>claim: ${escapeHtml(item.claim_ceiling || "n/a")}</span>
          </div>
          <div class="small">${escapeHtml(item.why_it_matters || "")}</div>
        </div>
      `
    )
    .join("");
  root.innerHTML = [
    `<div class="challenge-item"><strong>${escapeHtml(profile.headline || "Challenge")}</strong><div class="small">${escapeHtml(profile.pipeline_pressure || "n/a")}</div></div>`,
    `<div class="challenge-item"><strong>Breakpoint</strong><div class="small">${escapeHtml(profile.breakpoint || "n/a")}</div></div>`,
    `<div class="challenge-item"><strong>Operator Question</strong><div class="small">${escapeHtml(profile.operator_question || "n/a")}</div></div>`,
    `<div class="challenge-item"><strong>Real Job IDs</strong><div class="small">${escapeHtml(jobIds.length ? jobIds.join(", ") : (profile.job_id_note || "No replay job ids available."))}</div></div>`,
    `<div class="challenge-item challenge-ladder"><strong>Breakpoint Ladder</strong>${ladder || `<div class="small">No ladder metadata available.</div>`}</div>`,
  ].join("");
}

function renderFlow(run) {
  const root = $("flow-list");
  root.innerHTML = "";
  const flows = run.symbol_flow
    ? [
        ["Expected", run.symbol_flow.expected || []],
        ["Discovered", run.symbol_flow.discovered || []],
        ["Semantic", run.symbol_flow.semantic || []],
      ]
    : [["Selected runtime targets", (run.selected_runtime_targets || []).map((item) => item.target)]];
  flows.forEach(([label, values]) => {
    const item = document.createElement("div");
    item.className = "flow-item";
    item.innerHTML = `<strong>${escapeHtml(label)}</strong><div class="small">${values.length ? escapeHtml(values.join(" -> ")) : "n/a"}</div>`;
    root.appendChild(item);
  });
}

function renderArtifacts(run) {
  const root = $("artifact-list");
  root.innerHTML = "";
  (run.artifact_summary || []).forEach((artifact) => {
    const item = document.createElement("div");
    item.className = "artifact-item";
    item.innerHTML = `
      <strong>${escapeHtml(artifact.name)}</strong>
      <div class="small">${escapeHtml(artifact.kind)} - ${escapeHtml(artifact.included)}</div>
      <div><code>${escapeHtml(artifact.path)}</code></div>
    `;
    root.appendChild(item);
  });
  const note = document.createElement("div");
  note.className = "artifact-item";
  note.innerHTML = `<strong>Ground truth boundary</strong><div class="small">${escapeHtml(run.ground_truth_note)}</div>`;
  root.appendChild(note);
}

function renderTargets(run) {
  const root = $("target-table");
  root.innerHTML = "";
  (run.targets || []).forEach((target) => {
    const name = target.target || target.binary_name || "target";
    const status = target.status || target.role || "n/a";
    const duration = target.duration_sec ? formatDuration(target.duration_sec) : target.dependency_confidence || "n/a";
    const evidence = target.required_semantic_checkpoint || target.selection_reason || target.claim_ceiling || "";
    const row = document.createElement("tr");
    row.innerHTML = `
      <td><strong>${escapeHtml(name)}</strong><div class="small">${escapeHtml(target.binary_name || target.source_path || "")}</div></td>
      <td class="${statusClass(target.safe_to_compare ?? status)}">${escapeHtml(status)}</td>
      <td>${escapeHtml(duration)}</td>
      <td class="small">${escapeHtml(evidence)}</td>
    `;
    root.appendChild(row);
  });
}

function renderRun(run) {
  state.view = "run";
  $("overview-view").hidden = true;
  $("run-view").hidden = false;
  renderRunList();
  renderSummary(run);
  renderPipeline(run);
  renderPipelineMechanics(run);
  renderVerifiability(run);
  renderStageDetails(run);
  renderStageArtifacts(run);
  renderStageExplanation(run);
  renderLog(run);
  renderChallengeProfile(run);
  renderFlow(run);
  renderArtifacts(run);
  renderTargets(run);
  renderTelemetry(run);
}

async function selectRun(runId) {
  state.view = "run";
  state.selectedRunId = runId;
  state.selectedStageIndex = 0;
  if (!state.runs.has(runId)) {
    const entry = state.index.runs.find((item) => item.id === runId);
    state.runs.set(runId, await getJson(entry.path));
  }
  renderRun(state.runs.get(runId));
}

async function init() {
  try {
    state.index = await getJson("data/index.json");
    $("overview-button").addEventListener("click", renderOverview);
    $("run-filter").addEventListener("input", (event) => {
      state.filter = event.target.value;
      renderRunList();
    });
    $("refresh").addEventListener("click", async () => {
      state.index = await getJson("data/index.json");
      state.runs.clear();
      if (state.view === "overview") {
        renderOverview();
      } else {
        await selectRun(state.selectedRunId || state.index.runs[0]?.id);
      }
    });
    renderOverview();
  } catch (error) {
    $("run-title").textContent = "Replay data failed to load";
    $("run-summary").textContent = error.message;
    $("log").textContent = error.stack || error.message;
  }
}

init();
