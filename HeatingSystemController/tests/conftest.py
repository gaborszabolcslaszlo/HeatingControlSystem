from __future__ import annotations

import json
import html
import os
import time
from datetime import datetime, timezone
from pathlib import Path

import pytest
import requests


_CURRENT_TEST: str | None = None
_TEST_CASES: dict[str, dict] = {}
_TEST_RESULTS: dict[str, str] = {}
STATE_SETTLE_SECONDS = 5.0
API_REQUEST_RETRIES = 2


class TraceSession(requests.Session):
    def __init__(self) -> None:
        super().__init__()
        self.trace: list[dict] = []
        self.assertions: list[dict] = []
        self.wait_before_next_state = 0.0

    def record_assertion(
        self,
        description: str,
        expected,
        actual,
        passed: bool,
        element: str | None = None,
        trace_index: int | None = None,
        checks: list[dict] | None = None,
    ) -> None:
        assertion = {
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "testCase": _CURRENT_TEST,
            "description": description,
            "expected": expected,
            "actual": actual,
            "passed": passed,
            "element": element,
            "traceIndex": trace_index,
            "checks": checks or [],
        }
        self.assertions.append(assertion)
        if trace_index is not None and 0 <= trace_index < len(self.trace):
            self.trace[trace_index].setdefault("assertions", []).append(assertion)

    @staticmethod
    def _body(value) -> str | None:
        if value is None:
            return None
        if isinstance(value, bytes):
            return value.decode("utf-8", errors="replace")
        if isinstance(value, str):
            return value
        return json.dumps(value, ensure_ascii=False, default=str)

    def request(self, method, url, **kwargs):
        started = time.perf_counter()
        request_data = {
            "method": method,
            "url": url,
            "headers": dict(kwargs.get("headers") or {}),
            "body": self._body(kwargs.get("data", kwargs.get("json"))),
        }

        for attempt in range(API_REQUEST_RETRIES + 1):
            try:
                response = super().request(method, url, **kwargs)
            except (requests.ConnectionError, requests.Timeout) as exc:
                if attempt < API_REQUEST_RETRIES:
                    time.sleep(0.25 * (attempt + 1))
                    continue
                request_data["attempts"] = attempt + 1
                self.trace.append(
                    {
                        "timestamp": datetime.now(timezone.utc).isoformat(),
                        "testCase": _CURRENT_TEST,
                        "request": request_data,
                        "error": repr(exc),
                        "durationMs": round((time.perf_counter() - started) * 1000, 2),
                    }
                )
                raise

            if response.status_code != 429:
                break

            if attempt < API_REQUEST_RETRIES:
                retry_after_value = response.headers.get("Retry-After", "0.25")
                try:
                    retry_after = max(float(retry_after_value), 0.25)
                except ValueError:
                    retry_after = 0.25 * (attempt + 1)
                response.close()
                time.sleep(retry_after)

        # Ha az összes próbálkozás 429 választ adott, az utolsó válasz maradjon
        # meg a riportban és a teszt kapjon egyértelmű HTTP-hibát.

        # Use the prepared request so automatically added requests headers
        # (for example User-Agent and Accept-Encoding) are included too.
        prepared_request = response.request
        # A GET végpontok paraméterei a query stringben vannak, ezért a
        # ténylegesen elküldött URL-t kell rögzíteni, nem az eredeti URL-t.
        request_data["url"] = prepared_request.url
        request_data["headers"] = dict(prepared_request.headers)
        request_data["body"] = self._body(prepared_request.body)

        self.trace.append(
            {
                "timestamp": datetime.now(timezone.utc).isoformat(),
                "testCase": _CURRENT_TEST,
                "request": request_data,
                "response": {
                    "statusCode": response.status_code,
                    "reason": response.reason,
                    "headers": dict(response.headers),
                    "body": response.text,
                },
                "durationMs": round((time.perf_counter() - started) * 1000, 2),
            }
        )
        if url.rstrip("/").endswith(("/setTemp", "/heatingRequest")):
            self.wait_before_next_state = STATE_SETTLE_SECONDS
        return response


def pytest_addoption(parser):
    parser.addoption(
        "--api-trace",
        default="reports/api-trace.json",
        help="JSON file for detailed API request/response tracing",
    )
    parser.addoption(
        "--api-report",
        default="reports/api-report.md",
        help="Readable Markdown file for API test case reporting",
    )
    parser.addoption(
        "--api-html-report",
        default="reports/api-report.html",
        help="Interactive HTML file for API test case reporting",
    )


def pytest_runtest_setup(item) -> None:
    global _CURRENT_TEST
    _CURRENT_TEST = item.nodeid
    _TEST_CASES[item.nodeid] = {
        "description": (item.obj.__doc__ or "").strip(),
    }


def pytest_runtest_teardown(item, nextitem) -> None:
    global _CURRENT_TEST
    _CURRENT_TEST = None


def pytest_runtest_logreport(report) -> None:
    if report.when == "call":
        _TEST_RESULTS[report.nodeid] = report.outcome


def _write_trace(session: TraceSession, path_value: str) -> None:
    if not path_value:
        return
    path = Path(path_value)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(session.trace, indent=2, ensure_ascii=False), encoding="utf-8")


def _markdown_body(value) -> str:
    if value is None or value == "":
        return "(üres)"
    try:
        return json.dumps(json.loads(value), indent=2, ensure_ascii=False)
    except (TypeError, json.JSONDecodeError):
        return str(value)


def _write_readable_report(session: TraceSession, path_value: str) -> None:
    if not path_value:
        return

    path = Path(path_value)
    path.parent.mkdir(parents=True, exist_ok=True)
    grouped: dict[str, list[dict]] = {}
    for entry in session.trace:
        grouped.setdefault(entry.get("testCase") or "session setup", []).append(entry)

    lines = [
        "# HeatingSystemController API tesztriport",
        "",
        f"**Generálva:** {datetime.now(timezone.utc).isoformat()}",
        "",
        "A riport tesztesetenként mutatja a végrehajtott API lépéseket, "
        "a küldött kéréseket és az eszköz válaszait.",
        "",
    ]

    for test_case, entries in grouped.items():
        case_info = _TEST_CASES.get(test_case, {})
        description = case_info.get("description") or "Nincs külön leírás megadva."
        result = _TEST_RESULTS.get(test_case, "ismeretlen").upper()
        lines.extend([
            f"## Teszteset: `{test_case}`",
            "",
            f"**Eredmény:** `{result}`",
            "",
            f"**Leírás:** {description}",
            "",
        ])

        for step_number, entry in enumerate(entries, start=1):
            trace_index = next(
                index
                for index, candidate in enumerate(session.trace)
                if candidate is entry
            )
            response_assertions = list(entry.get("assertions", []))
            if not response_assertions:
                response_assertions = [
                    assertion
                    for assertion in session.assertions
                    if assertion.get("testCase") == test_case
                    and assertion.get("traceIndex") == trace_index
                ]
            request_data = entry["request"]
            method = request_data["method"]
            url = request_data["url"]
            lines.extend([
                f"### {step_number}. lépés: `{method} {url}`",
                "",
                f"**Időpont:** {entry['timestamp']}  ",
                f"**Válaszidő:** {entry['durationMs']} ms",
                "",
                "#### Küldött kérés",
                "",
                f"**Metódus:** `{method}`  ",
                f"**URL:** `{url}`",
                "",
                "**Fejlécek:**",
                "",
                "```json",
                _markdown_body(json.dumps(request_data["headers"], ensure_ascii=False)),
                "```",
                "",
                "**Törzs:**",
                "",
                "```json",
                _markdown_body(request_data["body"]),
                "```",
                "",
            ])

            if "response" in entry:
                response = entry["response"]
                lines.extend([
                    "#### Eszköz válasza",
                    "",
                    f"**Státusz:** `{response['statusCode']} {response['reason']}`",
                    "",
                    "**Fejlécek:**",
                    "",
                    "```json",
                    _markdown_body(json.dumps(response["headers"], ensure_ascii=False)),
                    "```",
                    "",
                    "**Törzs:**",
                    "",
                    "```json",
                    _markdown_body(response["body"]),
                    "```",
                    "",
                ])
            else:
                lines.extend([
                    "#### Hiba",
                    "",
                    f"`{entry['error']}`",
                    "",
                ])

        assertions = [
            assertion
            for assertion in session.assertions
            if assertion.get("testCase") == test_case
        ]
        if assertions:
            lines.extend(["#### Ellenőrzési pontok", ""])
            for assertion in assertions:
                result = "✅ PASS" if assertion["passed"] else "❌ FAIL"
                if assertion.get("checks"):
                    lines.append(f"- {result} **{assertion['description']}**")
                    for check in assertion["checks"]:
                        check_result = "✅" if check["passed"] else "❌"
                        lines.append(
                            f"  - {check_result} **{check['element']}**; "
                            f"elvárt: `{check['expected']}`, "
                            f"tényleges: `{check['actual']}`"
                        )
                else:
                    lines.append(
                        f"- {result} **{assertion['description']}**; "
                        f"elvárt: `{assertion['expected']}`, "
                        f"tényleges: `{assertion['actual']}`"
                    )
            lines.append("")

    path.write_text("\n".join(lines), encoding="utf-8")


def _html_code_block(title: str, value: str | None, language: str = "json") -> str:
    formatted = _markdown_body(value)
    return (
        f"<h4>{html.escape(title)}</h4>"
        f"<pre><code class=\"language-{language}\">"
        f"{html.escape(formatted)}</code></pre>"
    )


def _render_json_value(
    value,
    path: tuple[str, ...],
    expected_states: dict[str, dict],
    expected_values: dict[str, object],
    level: int = 0,
) -> str:
    indent = "  " * level
    if isinstance(value, dict):
        items = list(value.items())
        lines = ["{"]
        for index, (key, child) in enumerate(items):
            key_html = f'<span class="json-key">&quot;{html.escape(str(key))}&quot;</span>'
            child_path = path + (str(key),)
            child_html = _render_json_value(
                child, child_path, expected_states, expected_values, level + 1
            )
            child_lines = child_html.splitlines()
            line = f"{'  ' * (level + 1)}{key_html}: {child_lines[0]}"
            expected = None
            if str(key) == "state" and path:
                state_assertion = expected_states.get(str(path[-1]))
                if state_assertion is not None:
                    expected = state_assertion["expected"]
            elif not path and str(key) in expected_values:
                expected = expected_values[str(key)]

            if expected is not None:
                css_class = "json-match" if child == expected else "json-mismatch"
                title = "elvárt: " + str(expected)
                line = (
                    f'<span class="{css_class}" title="{html.escape(title)}">'
                    f"{line}</span>"
                )
            lines.append(line)
            lines.extend(child_lines[1:])
            if index < len(items) - 1:
                lines[-1] += ","
        lines.append(f"{indent}}}")
        return "\n".join(lines)

    if isinstance(value, list):
        items = [
            _render_json_value(
                child,
                path + (str(index),),
                expected_states,
                expected_values,
                level + 1,
            )
            for index, child in enumerate(value)
        ]
        return "[" + ", ".join(items) + "]"

    text = json.dumps(value, ensure_ascii=False)
    expected = None
    if len(path) >= 2 and path[-1] == "state":
        assertion = expected_states.get(path[-2])
        if assertion is not None:
            expected = assertion["expected"]
    elif len(path) == 1 and path[0] in expected_values:
        expected = expected_values[path[0]]

    if expected is not None:
        css_class = "json-match" if value == expected else "json-mismatch"
        title = "elvárt: " + str(expected)
        return (
            f'<span class="{css_class} json-value" title="{html.escape(title)}">'
            f"{html.escape(text)}</span>"
        )
    return html.escape(text)


def _html_json_response_block(
    title: str,
    body: str | None,
    assertions: list[dict],
    expected_values: dict[str, object] | None = None,
) -> str:
    if body is None or body == "":
        rendered = "(üres)"
    else:
        try:
            value = json.loads(body)
            expected_states = {}
            for assertion in assertions:
                if assertion.get("element"):
                    expected_states[assertion["element"]] = assertion
                for check in assertion.get("checks", []):
                    expected_states[check["element"]] = check
            rendered = _render_json_value(
                value, (), expected_states, expected_values or {}
            )
            missing_lines = []
            if isinstance(value, dict):
                state_checks = []
                for assertion in assertions:
                    if assertion.get("element"):
                        state_checks.append(assertion)
                    state_checks.extend(assertion.get("checks", []))
                for assertion in state_checks:
                    element = assertion["element"]
                    actual_element = value.get(element)
                    if not isinstance(actual_element, dict) or "state" not in actual_element:
                        expected = json.dumps(
                            {"state": assertion.get("expected")}, ensure_ascii=False
                        )
                        missing_lines.append(
                            f'<span class="json-mismatch missing-value" '
                            f'title="elvárt: {html.escape(expected)}">'
                            f'  // HIÁNYZÓ ELVÁRT MEZŐ: '
                            f'&quot;{html.escape(str(element))}&quot;: '
                            f'{html.escape(expected)}</span>'
                        )
            if missing_lines:
                rendered += "\n" + "\n".join(missing_lines)
        except (TypeError, json.JSONDecodeError):
            rendered = html.escape(str(body))
    return f"<h4>{html.escape(title)}</h4><pre><code>{rendered}</code></pre>"


def _test_tree_group(test_case: str) -> tuple[str, str]:
    """Return the module and logical group used by the HTML test tree."""
    parts = test_case.split("::")
    module = Path(parts[0]).name if parts and parts[0] else "session setup"
    test_name = parts[-1] if parts else test_case

    if "stress" in test_name.lower():
        category = "API stressztesztek"
    elif module == "test_state_transitions.py":
        category = "Állapotátmeneti tesztek"
    else:
        category = "API funkcionális tesztek"
    return module, category


def _expected_sensor_values(
    session: TraceSession, test_case: str, trace_index: int
) -> dict[str, object]:
    """Return the last values sent by setTemp before this state response."""
    for previous in reversed(session.trace[:trace_index]):
        if previous.get("testCase") != test_case:
            continue
        request = previous.get("request", {})
        if request.get("method") != "POST" or not request.get("url", "").endswith(
            "/setTemp"
        ):
            continue
        try:
            body = json.loads(request.get("body") or "{}")
        except (TypeError, json.JSONDecodeError):
            return {}
        return body if isinstance(body, dict) else {}
    return {}


def _write_html_report(session: TraceSession, path_value: str) -> None:
    if not path_value:
        return

    path = Path(path_value)
    path.parent.mkdir(parents=True, exist_ok=True)
    grouped: dict[str, list[dict]] = {}
    for entry in session.trace:
        grouped.setdefault(entry.get("testCase") or "session setup", []).append(entry)

    result_counts = {
        "passed": sum(result == "passed" for result in _TEST_RESULTS.values()),
        "failed": sum(result == "failed" for result in _TEST_RESULTS.values()),
        "skipped": sum(result == "skipped" for result in _TEST_RESULTS.values()),
    }
    result_counts["total"] = sum(result_counts.values())

    sections = []
    for test_case, entries in grouped.items():
        case_info = _TEST_CASES.get(test_case, {})
        description = case_info.get("description") or "Nincs külön leírás megadva."
        result = _TEST_RESULTS.get(test_case, "ismeretlen")
        result_class = {
            "passed": "passed",
            "failed": "failed",
            "skipped": "skipped",
        }.get(result, "unknown")
        steps = []

        for step_number, entry in enumerate(entries, start=1):
            trace_index = next(
                index
                for index, candidate in enumerate(session.trace)
                if candidate is entry
            )
            response_assertions = [
                assertion
                for assertion in session.assertions
                if assertion.get("testCase") == test_case
                and assertion.get("traceIndex") == trace_index
            ]
            request_data = entry["request"]
            request_headers = json.dumps(request_data["headers"], ensure_ascii=False)
            response_html = ""
            if "response" in entry:
                response = entry["response"]
                response_headers = json.dumps(response["headers"], ensure_ascii=False)
                expected_sensor_values = _expected_sensor_values(
                    session, test_case, trace_index
                )
                response_html = (
                    f"<h4>Eszköz válasza: "
                    f"<span class=\"status status-{response['statusCode'] // 100}xx\">"
                    f"{response['statusCode']} {html.escape(response['reason'])}</span></h4>"
                    f"{_html_code_block('Válasz fejlécei', response_headers)}"
                    f"{_html_json_response_block('Válasz törzse', response['body'], response_assertions, expected_sensor_values)}"
                )
            else:
                response_html = (
                    f"<h4>Hiba</h4><pre><code>{html.escape(entry['error'])}</code></pre>"
                )

            steps.append(
                f"<details class=\"step\"><summary>"
                f"{step_number}. lépés: <strong>{html.escape(request_data['method'])} "
                f"{html.escape(request_data['url'])}</strong> "
                f"<span class=\"duration\">{entry['durationMs']} ms</span>"
                f"</summary><div class=\"step-content\">"
                f"<p><b>Időpont:</b> {html.escape(entry['timestamp'])}</p>"
                f"{_html_code_block('Kérés fejlécei', request_headers)}"
                f"{_html_code_block('Kérés törzse', request_data['body'])}"
                f"{response_html}</div></details>"
            )

        assertions = [
            assertion
            for assertion in session.assertions
            if assertion.get("testCase") == test_case
        ]
        assertion_html = ""
        if assertions:
            assertion_html = "<h3>Ellenőrzési pontok</h3>"
            for assertion in assertions:
                assertion_class = "passed" if assertion["passed"] else "failed"
                symbol = "✓" if assertion["passed"] else "✗"
                expected = json.dumps(assertion["expected"], ensure_ascii=False, indent=2) \
                    if isinstance(assertion["expected"], dict) else str(assertion["expected"])
                actual = json.dumps(assertion["actual"], ensure_ascii=False, indent=2) \
                    if isinstance(assertion["actual"], dict) else str(assertion["actual"])
                assertion_html += (
                    f"<div class=\"assertion {assertion_class}\">"
                    f"<strong>{symbol} {html.escape(assertion['description'])}</strong>"
                    f"<div><b>Elvárt:</b> "
                    f"<code>{html.escape(expected)}</code></div>"
                    f"<div><b>Tényleges:</b> "
                    f"<code>{html.escape(actual)}</code></div>"
                    "</div>"
                )

        case_class = {
            "passed": "passed-case",
            "failed": "failed-case",
            "skipped": "skipped-case",
        }.get(result, "unknown-case")
        module, category = _test_tree_group(test_case)
        sections.append({
            "module": module,
            "category": category,
            "result": result,
            "html": (
                f"<details class=\"test-case {case_class}\"><summary>"
            f"<span class=\"status {result_class}\">{html.escape(result.upper())}</span> "
            f"<strong>{html.escape(test_case)}</strong></summary>"
            f"<div class=\"case-content\"><p>{html.escape(description)}</p>"
                f"{''.join(steps)}{assertion_html}</div></details>"
            ),
        })

    tree: dict[str, dict[str, list[dict]]] = {}
    for section in sections:
        tree.setdefault(section["module"], {}).setdefault(
            section["category"], []
        ).append(section)

    def _count_summary(items: list[dict]) -> str:
        counts = {
            "passed": sum(item["result"] == "passed" for item in items),
            "failed": sum(item["result"] == "failed" for item in items),
            "skipped": sum(item["result"] == "skipped" for item in items),
        }
        return (
            f"<span class=\"tree-count\">{len(items)} teszt</span>"
            f"<span class=\"tree-results\">"
            f"<span class=\"tree-passed\">✓ {counts['passed']}</span> "
            f"<span class=\"tree-failed\">✗ {counts['failed']}</span> "
            f"<span class=\"tree-skipped\">↷ {counts['skipped']}</span>"
            f"</span>"
        )

    tree_sections = []
    for module, categories in tree.items():
        module_items = [item for items in categories.values() for item in items]
        category_sections = []
        for category, items in categories.items():
            category_sections.append(
                f"<details class=\"test-group category-group\" open>"
                f"<summary><strong>{html.escape(category)}</strong> "
                f"{_count_summary(items)}</summary>"
                f"<div class=\"group-content\">"
                f"{''.join(item['html'] for item in items)}"
                f"</div></details>"
            )
        tree_sections.append(
            f"<details class=\"test-group module-group\" open>"
            f"<summary><strong>{html.escape(module)}</strong> "
            f"{_count_summary(module_items)}</summary>"
            f"<div class=\"group-content\">{''.join(category_sections)}</div>"
            f"</details>"
        )
    tree_html = f"<div class=\"test-tree\">{''.join(tree_sections)}</div>"

    document = f"""<!doctype html>
<html lang="hu">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>HeatingSystemController API tesztriport</title>
<style>
body {{ font-family: system-ui, sans-serif; line-height: 1.45; max-width: 1200px;
       margin: 2rem auto; padding: 0 1rem; color: #202124; background: #f6f8fa; }}
h1 {{ margin-bottom: .3rem; }}
.test-case, .step {{ background: white; border: 1px solid #d0d7de;
                     border-radius: 8px; margin: 1rem 0; overflow: hidden; }}
.test-group {{ background: #ffffff; border: 1px solid #d0d7de;
               border-radius: 8px; margin: .8rem 0; overflow: hidden; }}
.module-group {{ border-left: 5px solid #0969da; }}
.category-group {{ border-left: 4px solid #8250df; margin: .65rem .8rem; }}
.group-content {{ padding: 0 .8rem .35rem; }}
.tree-count {{ color: #57606a; font-size: .85rem; margin-left: .5rem; }}
.tree-results {{ float: right; font-size: .85rem; font-weight: 700; }}
.tree-passed {{ color: #1a7f37; }}
.tree-failed {{ color: #cf222e; }}
.tree-skipped {{ color: #9a6700; }}
summary {{ cursor: pointer; padding: .8rem 1rem; font-size: 1rem; }}
.case-content, .step-content {{ padding: 0 1rem 1rem; }}
.step {{ margin: .75rem 0; border-left: 4px solid #8c959f; }}
.step summary {{ font-size: .95rem; }}
.duration {{ float: right; color: #57606a; font-size: .85rem; }}
.status {{ border-radius: 999px; padding: .15rem .5rem; font-size: .75rem;
           font-weight: 700; margin-right: .4rem; }}
.passed {{ color: #116329; background: #dafbe1; }}
.failed {{ color: #a40e26; background: #ffebe9; }}
.skipped {{ color: #9a6700; background: #fff8c5; }}
.unknown {{ color: #57606a; background: #eaeef2; }}
.report-summary {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(130px, 1fr));
                   gap: .75rem; margin: 1rem 0 1.5rem; }}
.summary-card {{ background: white; border: 1px solid #d0d7de; border-radius: 8px;
                 padding: .75rem 1rem; }}
.summary-label {{ display: block; color: #57606a; font-size: .85rem; }}
.summary-value {{ display: block; font-size: 1.5rem; font-weight: 700; }}
.summary-card.passed {{ border-left: 4px solid #1a7f37; }}
.summary-card.failed {{ border-left: 4px solid #cf222e; }}
.summary-card.skipped {{ border-left: 4px solid #bf8700; }}
.status-2xx {{ color: #116329; }}
.status-4xx, .status-5xx {{ color: #a40e26; }}
.assertion {{ border-left: 4px solid #8c959f; padding: .6rem .8rem; margin: .5rem 0;
              background: #f6f8fa; border-radius: 4px; }}
.assertion.passed {{ border-left-color: #1a7f37; }}
.assertion.failed {{ border-left-color: #cf222e; background: #ffebe9; color: #a40e26; }}
.failed-case > summary {{ background: #ffebe9; color: #a40e26; }}
.passed-case > summary {{ background: #dafbe1; color: #116329; }}
.skipped-case > summary {{ background: #fff8c5; color: #9a6700; }}
.missing-value {{ margin: .6rem 0; padding: .6rem .8rem; border-radius: 4px; }}
.json-key {{ color: #8250df; }}
.json-match {{ color: #116329 !important; background: #dafbe1; font-weight: 700; }}
.json-mismatch {{ color: #a40e26 !important; background: #ffebe9; font-weight: 700; }}
.json-value {{ padding: 0 .15rem; border-radius: 3px; }}
pre {{ background: #f6f8fa; border: 1px solid #d8dee4; border-radius: 6px;
       padding: .75rem; overflow-x: auto; white-space: pre-wrap; word-break: break-word; }}
code {{ font-family: ui-monospace, SFMono-Regular, Consolas, monospace; }}
</style>
</head>
<body>
<h1>HeatingSystemController API tesztriport</h1>
<p>Generálva: {html.escape(datetime.now(timezone.utc).isoformat())}</p>
<div class="report-summary">
  <div class="summary-card"><span class="summary-label">Összes teszt</span><span class="summary-value">{result_counts['total']}</span></div>
  <div class="summary-card passed"><span class="summary-label">Sikeres</span><span class="summary-value">{result_counts['passed']}</span></div>
  <div class="summary-card failed"><span class="summary-label">Hibás</span><span class="summary-value">{result_counts['failed']}</span></div>
  <div class="summary-card skipped"><span class="summary-label">Kihagyott</span><span class="summary-value">{result_counts['skipped']}</span></div>
</div>
<p>A tesztesetek és az API lépések a címekre kattintva nyithatók ki és csukhatók össze.</p>
{tree_html}
</body>
</html>
"""
    path.write_text(document, encoding="utf-8")


@pytest.fixture(scope="session")
def api_base_url() -> str:
    return os.getenv("HEATING_CONTROLLER_URL", "http://192.168.0.106").rstrip("/")


@pytest.fixture(scope="session")
def api_verify_tls() -> bool:
    return os.getenv("HEATING_CONTROLLER_VERIFY_TLS", "0").lower() in {
        "1", "true", "yes"
    }


@pytest.fixture(scope="session")
def api_session(request, api_base_url: str, api_verify_tls: bool) -> requests.Session:
    session = TraceSession()
    session.verify = api_verify_tls
    try:
        response = session.get(f"{api_base_url}/state", timeout=(2, 15))
        response.raise_for_status()
    except requests.RequestException as exc:
        _write_trace(session, request.config.getoption("--api-trace"))
        _write_readable_report(session, request.config.getoption("--api-report"))
        _write_html_report(session, request.config.getoption("--api-html-report"))
        pytest.skip(f"API unavailable at {api_base_url}: {exc}")

    request.addfinalizer(
        lambda: (
            _write_trace(session, request.config.getoption("--api-trace")),
            _write_readable_report(session, request.config.getoption("--api-report")),
            _write_html_report(session, request.config.getoption("--api-html-report")),
        )
    )
    return session


@pytest.fixture
def set_sensor_values(api_session: requests.Session, api_base_url: str):
    def _set_sensor_values(values: dict[str, float]) -> dict:
        response = api_session.post(
            f"{api_base_url}/setTemp", json=values, timeout=(2, 15)
        )
        response.raise_for_status()
        return response.json()

    _set_sensor_values._trace_session = api_session
    return _set_sensor_values


@pytest.fixture
def read_state(api_session: requests.Session, api_base_url: str):
    def _read_state() -> dict:
        wait_seconds = getattr(api_session, "wait_before_next_state", 0.0)
        if wait_seconds > 0:
            time.sleep(wait_seconds)
            api_session.wait_before_next_state = 0.0
        response = api_session.get(f"{api_base_url}/state", timeout=(2, 15))
        response.raise_for_status()
        value = response.json()
        _read_state._last_trace_index = len(api_session.trace) - 1
        return value

    _read_state._trace_session = api_session
    return _read_state
