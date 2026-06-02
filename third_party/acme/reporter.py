#
#    reporter.py
#
#    ZeroM2M
#    Copyright (C) 2026 ZeroM2M Authors
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License v3.0 (GPL-3.0).
#

from __future__ import annotations

import json
import os
import time
import traceback
from dataclasses import dataclass, field, asdict
from datetime import datetime, timezone
from typing import Any, Optional

# Data model


@dataclass
class RequestRecord:
    seq: int
    protocol: str  # http | mqtt | ws | coap
    method: str  # GET / POST / PUT / DELETE / NOTIFY ...
    url: str
    req_headers: dict
    req_body: Any
    resp_status: Optional[int]
    resp_headers: dict
    resp_body: Any
    timestamp: str  # ISO-8601


@dataclass
class TestCaseRecord:
    name: str
    module: str
    status: str  # pass | fail | skip | error
    duration_s: float
    failure_msg: str
    traceback: str
    requests: list[RequestRecord] = field(default_factory=list)
    start_ts: str = ""


# Global capture state

_current_test: Optional[TestCaseRecord] = None
_all_tests: list[TestCaseRecord] = []
_req_seq: int = 0
_test_start: float = 0.0


def begin_test(name: str, module: str) -> None:
    global _current_test, _req_seq, _test_start
    _req_seq = 0
    _test_start = time.perf_counter()
    _current_test = TestCaseRecord(
        name=name,
        module=module,
        status="pass",
        duration_s=0.0,
        failure_msg="",
        traceback="",
        requests=[],
        start_ts=datetime.now(tz=timezone.utc).isoformat(),
    )


def end_test(status: str, failure_msg: str = "", tb: str = "") -> None:
    global _current_test
    if _current_test is None:
        return
    _current_test.duration_s = time.perf_counter() - _test_start
    _current_test.status = status
    _current_test.failure_msg = failure_msg
    _current_test.traceback = tb
    _all_tests.append(_current_test)
    _current_test = None


def record_request(
    protocol: str,
    method: str,
    url: str,
    req_headers: dict,
    req_body: Any,
    resp_status: Optional[int],
    resp_headers: dict,
    resp_body: Any,
) -> None:
    global _req_seq
    if _current_test is None:
        return
    _req_seq += 1
    _current_test.requests.append(
        RequestRecord(
            seq=_req_seq,
            protocol=protocol,
            method=method,
            url=url,
            req_headers=req_headers or {},
            req_body=req_body,
            resp_status=resp_status,
            resp_headers=resp_headers or {},
            resp_body=resp_body,
            timestamp=datetime.now(tz=timezone.utc).isoformat(),
        )
    )


def get_all_tests() -> list[TestCaseRecord]:
    return list(_all_tests)


def clear() -> None:
    global _all_tests, _current_test
    _all_tests = []
    _current_test = None


def update_test_status(
    name: str, module: str, status: str, failure_msg: str = "", tb: str = ""
) -> None:
    """Update the status of a previously recorded test (by name+module).

    This is used when the test runner reports the final outcome after
    the test's tearDown has already appended a pass record. We search
    the recorded tests in reverse order and update the first match.
    """
    for t in reversed(_all_tests):
        if t.name == name and t.module == module:
            t.status = status
            t.failure_msg = failure_msg
            t.traceback = tb
            return


# Component grouping


def _component_name(module: str) -> str:
    """testACP → ACP,  testTimeSeries → TimeSeries"""
    if module.startswith("test"):
        return module[4:] or module
    return module


def _component_summary(tests: list[TestCaseRecord]) -> list[dict]:
    groups: dict[str, dict] = {}
    for t in tests:
        comp = _component_name(t.module)
        if comp not in groups:
            groups[comp] = {
                "component": comp,
                "total": 0,
                "pass": 0,
                "fail": 0,
                "skip": 0,
                "error": 0,
                "duration_s": 0.0,
            }
        g = groups[comp]
        g["total"] += 1
        g[t.status] += 1
        g["duration_s"] += t.duration_s
    # health % (pass / non-skipped)
    for g in groups.values():
        ran = g["total"] - g["skip"]
        g["health"] = round(g["pass"] / ran * 100) if ran else 0
    return sorted(groups.values(), key=lambda x: x["component"])


# JSON report


def write_json(path: str, extra_meta: dict | None = None) -> None:
    tests_all = get_all_tests()
    # Only consider actual test cases (those named like unittest test methods)
    tests = [t for t in tests_all if t.name.startswith("test_")]

    payload = {
        "meta": {
            "generated_at": datetime.now(tz=timezone.utc).isoformat(),
            "total": len(tests),
            "pass": sum(1 for t in tests if t.status == "pass"),
            "fail": sum(1 for t in tests if t.status == "fail"),
            "skip": sum(1 for t in tests if t.status == "skip"),
            "error": sum(1 for t in tests if t.status == "error"),
            **(extra_meta or {}),
        },
        "components": _component_summary(tests),
        # keep full test list for detailed logs (including setup/teardown)
        "tests": [asdict(t) for t in tests_all],
    }
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(payload, f, indent=2, default=str)


# HTML report


def _status_badge(status: str) -> str:
    icons = {"pass": "✓", "fail": "✗", "skip": "◌", "error": "⚠"}
    return f'<span class="badge badge-{status}">{icons.get(status,"?")} {status.upper()}</span>'


def _health_ring(health: int, comp: str, total: int, fail: int) -> str:
    r = 28
    cir = 2 * 3.14159 * r
    off = cir * (1 - health / 100)
    # Match the new viewer colors
    color = "#00d900" if health >= 90 else "#ffef62" if health >= 60 else "#f92672"
    return f"""
<div class="comp-card" data-comp="{comp}">
  <svg viewBox="0 0 70 70" class="ring-svg">
    <circle cx="35" cy="35" r="{r}" class="ring-bg"/>
    <circle cx="35" cy="35" r="{r}" class="ring-fg"
      stroke="{color}"
      stroke-dasharray="{cir:.1f}"
      stroke-dashoffset="{off:.1f}"/>
    <text x="60" y="30" class="ring-pct">{health}%</text>
  </svg>
  <div class="comp-name">{comp}</div>
  <div class="comp-stats">{total} tests · {fail} failed</div>
</div>"""


def _render_body(body: Any) -> str:
    if body is None:
        return '<span class="empty">— empty —</span>'
    if isinstance(body, (dict, list)):
        try:
            return f'<pre class="json-body">{json.dumps(body, indent=2, default=str)}</pre>'
        except Exception:
            pass
    if isinstance(body, bytes):
        try:
            return (
                f'<pre class="json-body">{json.dumps(json.loads(body), indent=2)}</pre>'
            )
        except Exception:
            return f'<pre class="raw-body">{body!r}</pre>'
    return f'<pre class="raw-body">{body}</pre>'


def _render_headers(hds: dict) -> str:
    if not hds:
        return '<span class="empty">— none —</span>'
    rows = "".join(
        f'<tr><td class="hdr-k">{k}</td><td class="hdr-v">{v}</td></tr>'
        for k, v in hds.items()
    )
    return f'<table class="hdr-table">{rows}</table>'


def _render_request(req: RequestRecord, idx: int) -> str:
    rsc_ok = req.resp_status is not None and 2000 <= req.resp_status < 3000
    rsc_cls = "rsc-ok" if rsc_ok else "rsc-err"
    return f"""
<details class="req-block">
  <summary class="req-summary">
    <span class="req-seq">#{req.seq}</span>
    <span class="method-tag method-{req.method.lower()}">{req.method}</span>
    <span class="req-url">{req.url}</span>
    <span class="rsc-badge {rsc_cls}">{req.resp_status}</span>
    <span class="req-proto proto-{req.protocol}">{req.protocol.upper()}</span>
  </summary>
  <div class="req-detail">
    <div class="side">
      <div class="side-label">REQUEST</div>
      <div class="section-title">Headers</div>
      {_render_headers(req.req_headers)}
      <div class="section-title">Body</div>
      {_render_body(req.req_body)}
    </div>
    <div class="divider-v"></div>
    <div class="side">
      <div class="side-label">RESPONSE</div>
      <div class="section-title">Headers</div>
      {_render_headers(req.resp_headers)}
      <div class="section-title">Body</div>
      {_render_body(req.resp_body)}
    </div>
  </div>
</details>"""


def _render_test(t: TestCaseRecord) -> str:
    reqs_html = "".join(_render_request(r, i) for i, r in enumerate(t.requests))
    empty_reqs = (
        '<p class="empty">No requests captured for this test.</p>'
        if not t.requests
        else ""
    )
    tb_html = ""
    if t.traceback:
        tb_html = f'<pre class="traceback">{t.traceback}</pre>'
    fail_html = ""
    if t.failure_msg:
        fail_html = (
            f'<div class="fail-msg"><strong>Failure:</strong> {t.failure_msg}</div>'
        )
    dur = f"{t.duration_s*1000:.1f} ms"
    return f"""
<details class="test-block status-{t.status}">
  <summary class="test-summary">
    {_status_badge(t.status)}
    <span class="test-name">{t.name}</span>
    <span class="test-dur">{dur}</span>
    <span class="test-reqcount">{len(t.requests)} req</span>
  </summary>
  <div class="test-detail">
    {fail_html}
    {tb_html}
    <div class="requests-label">HTTP Traffic ({len(t.requests)} requests)</div>
    {reqs_html}
    {empty_reqs}
  </div>
</details>"""


_CSS = """
:root {
  --bg: #151515;
  --surface: rgba(32, 32, 32, 0.85);
  --surface2: rgba(48, 48, 48, 0.65);
  --border: rgba(48, 48, 48, 0.85);
  --border-light: rgba(85, 85, 85, 0.9);
  --text: #f8f8f0;
  --text-dim: #757571;
  --accent: #f92672;
  --accent-2: #b7134f;
  --pass: #00d900;
  --fail: #f92672;
  --skip: #757571;
  --error: #ffef62;
  --radius: 14px;
  --font-mono: 'JetBrains Mono', 'SFMono-Regular', Consolas, monospace;
  --font-ui: 'DM Sans', 'Segoe UI', sans-serif;
}
*, *::before, *::after { box-sizing: border-box; margin: 0; padding: 0; }
html { scroll-behavior: smooth; }
body {
  background:
    radial-gradient(circle at top left, rgba(249, 38, 114, 0.10), transparent 28%),
    radial-gradient(circle at top right, rgba(183, 19, 79, 0.10), transparent 24%),
    linear-gradient(180deg, #151515 0%, #181818 36%, #101010 100%);
  color: var(--text);
  font-family: var(--font-ui);
  font-size: 14px;
  line-height: 1.6;
  min-height: 100vh;
}
a { color: var(--accent); text-decoration: none; }

/* Header */
.header {
  background: linear-gradient(180deg, rgba(32, 32, 32, 0.95) 0%, rgba(21, 21, 21, 0.98) 100%);
  border-bottom: 1px solid var(--border);
  padding: 32px 40px 24px;
}
.header-top { display: flex; align-items: baseline; gap: 12px; margin-bottom: 6px; }
.logo {
  font-family: var(--font-ui);
  font-size: 28px;
  font-weight: 700;
  color: var(--text);
  letter-spacing: -0.02em;
}
.run-meta { color: var(--text-dim); font-size: 13px; max-width: 60ch; }
.global-stats {
  display: flex; gap: 12px; margin-top: 18px; flex-wrap: wrap;
}
.stat-pill {
  background: var(--surface2);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 10px 14px;
  font-size: 13px;
  display: flex; align-items: center; gap: 8px;
  color: var(--text-dim);
  text-transform: uppercase;
  letter-spacing: 0.05em;
}
.stat-pill .num { font-weight: 700; font-family: var(--font-mono); font-size: 14px; }
.num-pass  { color: var(--pass); }
.num-fail  { color: var(--fail); }
.num-skip  { color: var(--skip); }
.num-error { color: var(--error); }

/* Component overview */
.section-header {
  font-size: 11px;
  font-weight: 600;
  letter-spacing: 0.14em;
  text-transform: uppercase;
  color: #d2d2c8;
  padding: 24px 40px 12px;
}
.comp-grid {
  display: flex; flex-wrap: wrap; gap: 16px;
  padding: 10px 40px 24px;
  border-bottom: 1px solid var(--border);
}
.comp-card {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 16px;
  width: 140px;
  text-align: center;
  transition: border-color .15s ease, background .15s ease;
  cursor: default;
}
.comp-card:hover { border-color: rgba(249, 38, 114, 0.4); background: rgba(38, 38, 38, 0.95); }
.ring-svg { width: 70px; height: 70px; transform: rotate(-90deg); }
.ring-bg  { fill: none; stroke: var(--border); stroke-width: 5; }
.ring-fg  { fill: none; stroke-width: 5; stroke-linecap: round;
             transition: stroke-dashoffset .6s ease; }
.ring-pct {
  fill: var(--text);
  font-family: var(--font-mono);
  font-size: 13px;
  font-weight: 700;
  transform: rotate(90deg);
  transform-box: fill-box;
  dominant-baseline: middle;
  text-anchor: middle;
}
.comp-name  { font-size: 13px; font-weight: 600; margin-top: 10px; }
.comp-stats { font-size: 11px; color: var(--text-dim); margin-top: 4px; font-family: var(--font-mono); }

/* Suite filter bar */
.filter-bar {
  display: flex; gap: 10px; align-items: center;
  padding: 20px 40px;
  flex-wrap: wrap;
}
.filter-bar input {
  background: rgba(32, 32, 32, 0.95);
  border: 1px solid rgba(48, 48, 48, 1);
  border-radius: var(--radius);
  color: var(--text);
  font-family: var(--font-mono);
  font-size: 13px;
  padding: 10px 14px;
  width: 280px;
  outline: none;
  transition: border-color 0.15s ease, box-shadow 0.15s ease;
}
.filter-bar input:focus {
  border-color: var(--accent);
  box-shadow: 0 0 0 3px rgba(249, 38, 114, 0.14);
}
.filter-btn {
  background: rgba(32, 32, 32, 0.95);
  border: 1px solid rgba(85, 85, 85, 0.9);
  border-radius: var(--radius);
  color: var(--text);
  cursor: pointer;
  font-size: 13px;
  padding: 10px 14px;
  font-family: var(--font-ui);
  transition: border-color 0.15s ease, background 0.15s ease;
}
.filter-btn:hover { border-color: rgba(249, 38, 114, 0.7); background: rgba(38, 38, 38, 0.98); }
.filter-btn.active {
  background: rgba(249, 38, 114, 0.12);
  border-color: rgba(249, 38, 114, 0.55);
  color: #fff;
}
.expand-all-btn {
  margin-left: auto;
  background: transparent;
  border: 1px solid var(--border);
  border-radius: 999px;
  color: var(--text-dim);
  cursor: pointer;
  font-size: 12px;
  padding: 8px 14px;
  transition: border-color 0.15s ease, color 0.15s ease;
}
.expand-all-btn:hover { border-color: rgba(249, 38, 114, 0.55); color: var(--text); }

/* Test Suite groups */
.suite {
  margin: 0 40px 16px;
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius);
}
.suite-header {
  display: flex; align-items: center; gap: 12px;
  padding: 14px 18px;
  cursor: pointer;
  user-select: none;
}
.suite-name  { font-family: var(--font-mono); font-size: 14px; font-weight: 700; color: #d2d2c8; }
.suite-stats { font-size: 12px; color: var(--text-dim); margin-left: auto; font-family: var(--font-mono); }
.suite-chevron { color: var(--text-dim); transition: transform .2s; font-size: 10px; }
.suite.open .suite-chevron { transform: rotate(90deg); }
.suite-body { display: none; padding: 0 18px 18px; }
.suite.open .suite-body { display: block; }

/* Test blocks */
.test-block {
  border: 1px solid var(--border);
  border-radius: var(--radius);
  margin: 8px 0;
  overflow: hidden;
  transition: border-color .2s;
  background: rgba(21, 21, 21, 0.6);
}
.test-block[open] { border-color: rgba(249, 38, 114, 0.3); }
.test-block.status-fail { border-left: 4px solid var(--fail); }
.test-block.status-error { border-left: 4px solid var(--error); }
.test-block.status-pass { border-left: 4px solid var(--pass); }
.test-block.status-skip { border-left: 4px solid var(--skip); opacity: .7; }
.test-summary {
  display: flex; align-items: center; gap: 10px;
  padding: 12px 16px;
  background: rgba(32, 32, 32, 0.4);
  cursor: pointer;
  list-style: none;
}
.test-summary::-webkit-details-marker { display: none; }
.test-name  { font-family: var(--font-mono); font-size: 13px; flex: 1; }
.test-dur   { font-size: 11px; color: var(--text-dim); font-family: var(--font-mono); }
.test-reqcount { font-size: 11px; color: var(--text); padding: 4px 10px;
                  background: rgba(183, 19, 79, 0.14); border: 1px solid rgba(249, 38, 114, 0.28); border-radius: 999px; }
.test-detail { padding: 16px; background: rgba(21, 21, 21, 0.8); }
.fail-msg {
  background: rgba(249, 38, 114, 0.08);
  border: 1px dashed rgba(249, 38, 114, 0.34);
  border-radius: var(--radius);
  color: #ff8b94;
  font-family: var(--font-mono);
  font-size: 12px;
  margin-bottom: 16px;
  padding: 12px 16px;
  white-space: pre-wrap;
  word-break: break-all;
}
.traceback {
  background: rgba(21, 21, 21, 0.9);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  color: #ff8b94;
  font-family: var(--font-mono);
  font-size: 12px;
  margin-bottom: 16px;
  padding: 12px 16px;
  white-space: pre-wrap;
  overflow-x: auto;
}
.requests-label {
  font-size: 11px;
  letter-spacing: 0.14em;
  text-transform: uppercase;
  color: var(--text-dim);
  margin-bottom: 12px;
}

/* Request blocks */
.req-block {
  background: var(--surface2);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  margin: 8px 0;
  overflow: hidden;
}
.req-block[open] { border-color: rgba(249, 38, 114, 0.4); }
.req-summary {
  display: flex; align-items: center; gap: 10px;
  padding: 10px 14px;
  cursor: pointer;
  list-style: none;
}
.req-summary::-webkit-details-marker { display: none; }
.req-seq  { font-family: var(--font-mono); font-size: 11px; color: var(--text-dim); width: 24px; }
.req-url  { font-family: var(--font-mono); font-size: 12px; flex: 1;
             white-space: nowrap; overflow: hidden; text-overflow: ellipsis; color: #d2d2c8; }
.req-proto { font-size: 10px; padding: 3px 8px; border-radius: 999px;
              background: rgba(32, 32, 32, 0.95); border: 1px solid var(--border); color: var(--text-dim); font-family: var(--font-mono); }
.proto-http  { color: #66d9ef; }
.proto-mqtt  { color: var(--pass); }
.proto-ws    { color: #fd971f; }
.proto-coap  { color: #ae81ff; }

/* HTTP Method Colors */
.method-tag {
  font-family: var(--font-mono);
  font-size: 10px;
  font-weight: 700;
  padding: 3px 8px;
  border-radius: 6px;
  letter-spacing: .5px;
}
.method-get    { background: rgba(166, 226, 46, 0.15);  color: var(--pass); }
.method-post   { background: rgba(102, 217, 239, 0.15); color: #66d9ef; }
.method-put    { background: rgba(253, 151, 31, 0.15);  color: #fd971f; }
.method-delete { background: rgba(249, 38, 114, 0.15);  color: var(--fail); }
.method-notify { background: rgba(174, 129, 255, 0.15); color: #ae81ff; }

.rsc-badge {
  font-family: var(--font-mono);
  font-size: 11px;
  font-weight: 700;
  padding: 3px 8px;
  border-radius: 999px;
}
.rsc-ok  { background: rgba(166, 226, 46, 0.15);  color: var(--pass); }
.rsc-err { background: rgba(249, 38, 114, 0.15);  color: var(--fail); }

/* Request detail split-view */
.req-detail {
  display: grid;
  grid-template-columns: 1fr 1px 1fr;
  gap: 0;
  background: var(--bg);
  border-top: 1px solid var(--border);
}
.side { padding: 16px; overflow-x: auto; }
.side-label {
  font-size: 11px;
  letter-spacing: 0.14em;
  text-transform: uppercase;
  color: var(--accent);
  margin-bottom: 12px;
}
.divider-v { background: var(--border); }
.section-title {
  font-size: 11px;
  letter-spacing: 0.1em;
  text-transform: uppercase;
  color: var(--text-dim);
  margin: 14px 0 6px;
  border-bottom: 1px solid rgba(48, 48, 48, 0.5);
  padding-bottom: 4px;
}
.hdr-table { border-collapse: collapse; width: 100%; }
.hdr-k { color: #66d9ef; font-family: var(--font-mono); font-size: 12px;
          padding: 4px 10px 4px 0; white-space: nowrap; vertical-align: top; }
.hdr-v { font-family: var(--font-mono); font-size: 12px; color: var(--text);
          word-break: break-all; padding: 4px 0; }
.json-body, .raw-body {
  font-family: var(--font-mono);
  font-size: 12px;
  white-space: pre-wrap;
  word-break: break-all;
  color: #e6e6e6;
  line-height: 1.5;
  background: rgba(32, 32, 32, 0.4);
  padding: 12px;
  border-radius: 8px;
  border: 1px solid rgba(48, 48, 48, 0.6);
}

/* Badge */
.badge {
  display: inline-flex; align-items: center;
  font-family: var(--font-mono); font-size: 11px; font-weight: 700;
  padding: 3px 10px; border-radius: 999px;
  letter-spacing: .5px; white-space: nowrap;
}
.badge-pass  { background: rgba(166, 226, 46, 0.15);  color: var(--pass); border: 1px solid rgba(166, 226, 46, 0.3); }
.badge-fail  { background: rgba(249, 38, 114, 0.15);  color: var(--fail); border: 1px solid rgba(249, 38, 114, 0.3); }
.badge-skip  { background: rgba(117, 117, 113, 0.15); color: var(--skip); border: 1px solid rgba(117, 117, 113, 0.3); }
.badge-error { background: rgba(255, 239, 98, 0.15);  color: var(--error); border: 1px solid rgba(255, 239, 98, 0.3); }

.empty { color: var(--text-dim); font-style: italic; font-size: 12px; }

/* Scrollbar */
::-webkit-scrollbar { width: 8px; height: 8px; }
::-webkit-scrollbar-track { background: transparent; }
::-webkit-scrollbar-thumb { background: rgba(85, 85, 85, 0.8); border-radius: 4px; }
::-webkit-scrollbar-thumb:hover { background: rgba(117, 117, 113, 0.9); }

/* Hidden filter */
.hidden { display: none !important; }

@media (max-width: 768px) {
  .req-detail { grid-template-columns: 1fr; }
  .divider-v { height: 1px; width: 100%; }
}
"""

_JS = """
// Suite accordion toggle
document.querySelectorAll('.suite-header').forEach(hdr => {
  hdr.addEventListener('click', () => hdr.closest('.suite').classList.toggle('open'));
});

// Expand / Collapse all
document.getElementById('expandAll').addEventListener('click', function() {
  const isExpand = this.dataset.state !== 'open';
  document.querySelectorAll('.suite').forEach(s => {
    isExpand ? s.classList.add('open') : s.classList.remove('open');
  });
  document.querySelectorAll('.test-block, .req-block').forEach(el => {
    isExpand ? el.setAttribute('open','') : el.removeAttribute('open');
  });
  this.dataset.state = isExpand ? 'open' : '';
  this.textContent   = isExpand ? 'Collapse All' : 'Expand All';
});

// Status filter buttons
document.querySelectorAll('.filter-btn[data-status]').forEach(btn => {
  btn.addEventListener('click', function() {
    this.classList.toggle('active');
    applyFilters();
  });
});

// Text search
document.getElementById('searchBox').addEventListener('input', applyFilters);

function applyFilters() {
  const activeStatuses = [...document.querySelectorAll('.filter-btn[data-status].active')]
                         .map(b => b.dataset.status);
  const query = document.getElementById('searchBox').value.toLowerCase().trim();

  document.querySelectorAll('.test-block').forEach(block => {
    const status = block.className.match(/status-(\\w+)/)?.[1] || '';
    const name   = block.querySelector('.test-name')?.textContent?.toLowerCase() || '';
    const statusOk = activeStatuses.length === 0 || activeStatuses.includes(status);
    const searchOk = !query || name.includes(query);
    block.classList.toggle('hidden', !(statusOk && searchOk));
  });

  // Hide suites with no visible tests
  document.querySelectorAll('.suite').forEach(suite => {
    const visible = suite.querySelectorAll('.test-block:not(.hidden)').length > 0;
    suite.classList.toggle('hidden', !visible);
  });
}
"""


def write_html(path: str, cse_info: dict | None = None) -> None:
    tests_all = get_all_tests()
    # Only consider real unit tests for the summary counts
    tests = [t for t in tests_all if t.name.startswith("test_")]
    total = len(tests)
    n_pass = sum(1 for t in tests if t.status == "pass")
    n_fail = sum(1 for t in tests if t.status == "fail")
    n_skip = sum(1 for t in tests if t.status == "skip")
    n_error = sum(1 for t in tests if t.status == "error")

    gen_at = datetime.now(tz=timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
    comps = _component_summary(tests)
    ci = cse_info or {}

    # Component ring grid
    rings_html = "".join(
        _health_ring(c["health"], c["component"], c["total"], c["fail"] + c["error"])
        for c in comps
    )

    # Group tests by module (use only real tests for grouping)
    modules: dict[str, list[TestCaseRecord]] = {}
    for t in tests:
        modules.setdefault(t.module, []).append(t)

    suites_html = ""
    for mod, mod_tests in modules.items():
        pass_c = sum(1 for t in mod_tests if t.status == "pass")
        fail_c = sum(1 for t in mod_tests if t.status in ("fail", "error"))
        skip_c = sum(1 for t in mod_tests if t.status == "skip")
        tests_html = "".join(_render_test(t) for t in mod_tests)
        suites_html += f"""
<div class="suite">
  <div class="suite-header">
    <span class="suite-chevron">▶</span>
    <span class="suite-name">{mod}</span>
    <span class="suite-stats">
      <span class="num-pass" style="color: var(--pass);">✓ {pass_c}</span>
      &nbsp;
      <span class="num-fail" style="color: var(--fail);">✗ {fail_c}</span>
      &nbsp;
      <span class="num-skip" style="color: var(--skip);">◌ {skip_c}</span>
      &nbsp;<span style="color: var(--text-dim);">/ {len(mod_tests)} total</span>
    </span>
  </div>
  <div class="suite-body">{tests_html}</div>
</div>"""

    # CSE info string
    cse_str = ""
    if ci:
        parts = [
            f'<span style="color:var(--text-dim)">{k}:</span> <strong>{v}</strong>'
            for k, v in ci.items()
        ]
        cse_str = " &nbsp;·&nbsp; ".join(parts)

    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ACME CSE Test Report — {gen_at}</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=DM+Sans:wght@400;600;700&family=JetBrains+Mono:wght@400;700&display=swap" rel="stylesheet">
<style>{_CSS}</style>
</head>
<body>

<div class="header">
  <div class="header-top">
    <span class="logo">ACME CSE Test Report</span>
  </div>
  <div class="run-meta">{gen_at}</div>
  {f'<div class="run-meta" style="margin-top:8px">{cse_str}</div>' if cse_str else ''}
  <div class="global-stats">
    <div class="stat-pill"><span class="num num-pass">{n_pass}</span> Passed</div>
    <div class="stat-pill"><span class="num num-fail">{n_fail}</span> Failed</div>
    <div class="stat-pill"><span class="num num-error">{n_error}</span> Errors</div>
    <div class="stat-pill"><span class="num num-skip">{n_skip}</span> Skipped</div>
    <div class="stat-pill" style="border-color: rgba(249, 38, 114, 0.4);"><span class="num" style="color:var(--accent)">{total}</span> Total</div>
  </div>
</div>

<div class="section-header">Component Health</div>
<div class="comp-grid">{rings_html}</div>

<div class="filter-bar">
  <input id="searchBox" type="text" placeholder="Search test name…">
  <button class="filter-btn" data-status="fail">✗ Failed</button>
  <button class="filter-btn" data-status="error">⚠ Errors</button>
  <button class="filter-btn" data-status="pass">✓ Passed</button>
  <button class="filter-btn" data-status="skip">◌ Skipped</button>
  <button class="expand-all-btn" id="expandAll" data-state="">Expand All</button>
</div>

{suites_html}

<script>{_JS}</script>
</body>
</html>"""

    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        f.write(html)
