
#!/usr/bin/env python3
"""
NEBULA — Evidence-based project auditor
Python 3.10+

The auditor checks repository structure and searches for implementation evidence.
It does not pretend that source-code text proves audio quality or host compatibility.
"""

from __future__ import annotations

import ast
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
REPORTS = ROOT / "reports"
GOALS_FILE = ROOT / "AUTOPILOT_GOALS.yml"

SOURCE_EXTENSIONS = {".cpp", ".c", ".cc", ".cxx", ".h", ".hpp", ".hxx"}
DOC_EXTENSIONS = {".md", ".txt", ".rst"}
EXCLUDED_PARTS = {
    ".git", "build", "JUCE", "third_party", "node_modules",
    ".venv", "__pycache__", ".idea", ".vs"
}

# A check passes only when its evidence can be found in the repository
# or in an appropriate generated build/test report.
ALIASES: dict[str, tuple[str, ...]] = {
    "project_structure_valid": ("CMakeLists.txt", "Source", "Tests"),
    "cmake_configuration_valid": ("cmake_configure_success",),
    "juce_dependency_available": ("JUCE", "juce_add_plugin", "find_package(JUCE"),
    "shared_core_present": ("Source/Shared",),
    "documentation_present": ("README.md", "docs/"),
    "cmake_configure_success": ("cmake_configure_success",),
    "vst3_build_success": ("vst3_build_success",),
    "standalone_build_success": ("standalone_build_success",),
    "all_registered_tests_pass": ("all_registered_tests_pass",),
    "final_audit_pass": ("final_audit_pass",),
    "final_report_generated": ("final_report_generated",),
    "release_documentation": ("docs/BUILD.md", "docs/INSTALLATION.md"),
    "effect_tests": ("Tests/FX", "FXTests"),
    "synth_tests": ("Tests/Synth", "SynthTests"),
    "drum_tests": ("Tests/Drum", "DrumTests"),
    "mono_stereo_tests": ("mono_stereo", "MonoStereo"),
    "sample_rate_tests": ("sample_rate", "SampleRate"),
    "state_restore_tests": ("state_restore", "StateRestore"),
    "repeated_initialization_tests": ("repeated_initialization", "RepeatedInitialization"),
}

# Patterns are warnings for manual review, not automatic proof of a defect.
REALTIME_RISK_PATTERNS = {
    "dynamic allocation": re.compile(r"\b(new|malloc|calloc|realloc)\s*\("),
    "blocking sleep": re.compile(r"\b(sleep_for|sleep_until|usleep|Sleep)\s*\("),
    "mutex locking": re.compile(r"\b(lock|try_lock)\s*\("),
    "file access": re.compile(r"\b(fopen|ifstream|ofstream|filesystem::)\b"),
    "console output": re.compile(r"\b(printf|fprintf|std::cout|std::cerr)\b"),
}

PLACEHOLDER_PATTERN = re.compile(
    r"\b(TODO|FIXME|PLACEHOLDER|NOT_IMPLEMENTED|IMPLEMENT_ME)\b",
    re.IGNORECASE,
)


def now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def load_json_compatible(path: Path) -> dict[str, Any]:
    if not path.is_file():
        raise RuntimeError(f"Required file is missing: {path}")
    try:
        data = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, json.JSONDecodeError) as exc:
        raise RuntimeError(f"Cannot parse {path.name}: {exc}") from exc
    if not isinstance(data, dict):
        raise RuntimeError(f"{path.name} must contain a JSON object.")
    return data


def repo_files() -> list[Path]:
    result: list[Path] = []
    for path in ROOT.rglob("*"):
        if not path.is_file():
            continue
        rel = path.relative_to(ROOT)
        if any(part in EXCLUDED_PARTS for part in rel.parts):
            continue
        result.append(rel)
    return sorted(result)


def read_text(path: Path) -> str:
    try:
        return (ROOT / path).read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def normalize_identifier(value: str) -> str:
    return re.sub(r"[^a-z0-9]", "", value.lower())


def collect_goals(goals: dict[str, Any]) -> list[str]:
    checks: list[str] = []
    for phase in goals.get("phases", []):
        if not isinstance(phase, dict) or not phase.get("required", True):
            continue
        for check in phase.get("checks", []):
            if isinstance(check, str) and check not in checks:
                checks.append(check)
    return checks


def evidence_for(
    check: str,
    files: list[Path],
    contents: dict[str, str],
) -> list[str]:
    evidence: list[str] = []
    aliases = ALIASES.get(check, ())
    terms = aliases or (check, normalize_identifier(check))

    for rel in files:
        rel_text = rel.as_posix()
        content = contents.get(rel_text, "")
        for term in terms:
            if not term:
                continue
            if term.lower() in rel_text.lower():
                evidence.append(f"path:{rel_text}")
                break
            if term.lower() in content.lower():
                evidence.append(f"content:{rel_text}")
                break

    # Build/test checks require explicit successful machine-generated evidence.
    report_requirements = {
        "cmake_configure_success": ("build_results.json", ("configure_success", "configured")),
        "vst3_build_success": ("build_results.json", ("vst3_success", "vst3_built")),
        "standalone_build_success": ("build_results.json", ("standalone_success", "standalone_built")),
        "all_registered_tests_pass": ("test_results.json", ("success", "all_passed")),
        "final_audit_pass": ("audit_results.json", ("status",)),
        "final_report_generated": ("final_report.json", ("generated",)),
    }
    if check in report_requirements:
        filename, keys = report_requirements[check]
        report_path = REPORTS / filename
        if report_path.is_file():
            try:
                report = json.loads(report_path.read_text(encoding="utf-8"))
                if check == "final_audit_pass":
                    if report.get("status") == "PASS":
                        evidence.append(f"report:{filename}")
                elif any(report.get(key) is True for key in keys):
                    evidence.append(f"report:{filename}")
            except (OSError, json.JSONDecodeError):
                pass

    return sorted(set(evidence))


def check_python_syntax(files: list[Path]) -> tuple[list[str], list[str]]:
    passed: list[str] = []
    failed: list[str] = []
    for rel in files:
        if rel.suffix != ".py":
            continue
        try:
            ast.parse(read_text(rel), filename=rel.as_posix())
            passed.append(rel.as_posix())
        except SyntaxError as exc:
            failed.append(f"{rel.as_posix()}:{exc.lineno}: {exc.msg}")
    return passed, failed


def scan_risks(files: list[Path], contents: dict[str, str]) -> list[dict[str, str]]:
    findings: list[dict[str, str]] = []
    for rel in files:
        if rel.suffix.lower() not in SOURCE_EXTENSIONS:
            continue
        text = contents.get(rel.as_posix(), "")
        for label, pattern in REALTIME_RISK_PATTERNS.items():
            for match in pattern.finditer(text):
                line = text.count("\n", 0, match.start()) + 1
                findings.append({
                    "file": rel.as_posix(),
                    "line": str(line),
                    "risk": label,
                    "match": match.group(0),
                    "severity": "REVIEW",
                })
    return findings


def scan_placeholders(files: list[Path], contents: dict[str, str]) -> list[str]:
    findings: list[str] = []
    for rel in files:
        if rel.suffix.lower() not in SOURCE_EXTENSIONS | DOC_EXTENSIONS:
            continue
        text = contents.get(rel.as_posix(), "")
        for match in PLACEHOLDER_PATTERN.finditer(text):
            line = text.count("\n", 0, match.start()) + 1
            findings.append(f"{rel.as_posix()}:{line}:{match.group(0)}")
    return findings


def main() -> int:
    REPORTS.mkdir(parents=True, exist_ok=True)

    try:
        goals = load_json_compatible(GOALS_FILE)
    except RuntimeError as exc:
        print(f"[AUDIT ERROR] {exc}", file=sys.stderr)
        return 2

    files = repo_files()
    contents = {
        rel.as_posix(): read_text(rel)
        for rel in files
        if rel.suffix.lower() in SOURCE_EXTENSIONS | DOC_EXTENSIONS | {".py", ".yml", ".yaml", ".txt", ".cmake"}
        or rel.name == "CMakeLists.txt"
    }

    required_checks = collect_goals(goals)
    checks: dict[str, bool] = {}
    check_evidence: dict[str, list[str]] = {}

    for check in required_checks:
        found = evidence_for(check, files, contents)
        checks[check] = bool(found)
        check_evidence[check] = found

    python_files = [p for p in files if p.suffix == ".py"]
    python_ok, python_errors = check_python_syntax(python_files)

    cmake_exists = (ROOT / "CMakeLists.txt").is_file()
    source_exists = any(p.suffix.lower() in SOURCE_EXTENSIONS for p in files)
    docs_exists = any(p.suffix.lower() in DOC_EXTENSIONS for p in files)
    workflow_exists = any(
        p.as_posix().startswith(".github/workflows/") for p in files
    )

    risks = scan_risks(files, contents)
    placeholders = scan_placeholders(files, contents)

    critical_errors: list[str] = []
    warnings: list[str] = []

    if not cmake_exists:
        critical_errors.append("Root CMakeLists.txt is missing.")
    if not source_exists:
        critical_errors.append("No C/C++ source files were found.")
    if python_errors:
        critical_errors.extend(f"Python syntax error: {item}" for item in python_errors)
    if not docs_exists:
        warnings.append("No Markdown/text documentation files were found.")
    if not workflow_exists:
        warnings.append("GitHub Actions workflow is missing.")
    if risks:
        warnings.append(
            f"{len(risks)} possible real-time audio risks need manual review."
        )
    if placeholders:
        warnings.append(
            f"{len(placeholders)} TODO/FIXME/placeholder markers were found."
        )

    total = len(checks)
    passed = sum(1 for value in checks.values() if value)
    failed = total - passed

    status = "PASS" if failed == 0 and not critical_errors else "INCOMPLETE"

    result = {
        "project": "NEBULA",
        "audit_version": "2.0.0",
        "timestamp": now(),
        "repository_root": str(ROOT),
        "status": status,
        "summary": {
            "required_checks": total,
            "passed_checks": passed,
            "failed_checks": failed,
            "critical_errors": len(critical_errors),
            "warnings": len(warnings),
            "source_files": sum(1 for p in files if p.suffix.lower() in SOURCE_EXTENSIONS),
            "python_files": len(python_files),
        },
        "checks": checks,
        "check_evidence": check_evidence,
        "critical_errors": critical_errors,
        "warnings": warnings,
        "python_syntax": {
            "passed_files": python_ok,
            "errors": python_errors,
        },
        "realtime_risk_review": risks,
        "placeholder_markers": placeholders,
        "structure": {
            "cmake_lists_present": cmake_exists,
            "cpp_sources_present": source_exists,
            "documentation_present": docs_exists,
            "github_workflow_present": workflow_exists,
        },
        "limitations": [
            "Static text scanning does not prove that DSP algorithms work correctly.",
            "Static scanning cannot prove sound quality or absence of audio glitches.",
            "FL Studio compatibility requires an actual VST3 load test in the host.",
            "Portability requires installing and testing from multiple drive paths.",
            "CPU and memory budgets require runtime profiling on target systems.",
            "3D visual quality requires visual inspection of the running application.",
            "A keyword match is evidence of code presence, not proof of feature completeness.",
        ],
    }

    json_path = REPORTS / "audit_results.json"
    json_path.write_text(
        json.dumps(result, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )

    md_lines = [
        "# NEBULA Audit Report",
        "",
        f"- **Date:** {result['timestamp']}",
        f"- **Status:** {status}",
        f"- **Checks:** {passed}/{total} passed",
        f"- **Critical errors:** {len(critical_errors)}",
        f"- **Warnings:** {len(warnings)}",
        "",
        "## Required checks",
        "",
        "| Check | Result | Evidence |",
        "|---|---:|---|",
    ]
    for check, ok in checks.items():
        evidence = ", ".join(check_evidence[check]) or "No evidence"
        md_lines.append(
            f"| `{check}` | {'PASS' if ok else 'MISSING'} | {evidence} |"
        )

    md_lines += ["", "## Critical errors", ""]
    md_lines += [f"- {item}" for item in critical_errors] or ["- None"]
    md_lines += ["", "## Warnings", ""]
    md_lines += [f"- {item}" for item in warnings] or ["- None"]
    md_lines += ["", "## Real-time review findings", ""]
    md_lines += [
        f"- `{item['file']}:{item['line']}` — {item['risk']} (`{item['match']}`)"
        for item in risks[:200]
    ] or ["- No heuristic matches."]
    md_lines += [
        "",
        "## Limitations",
        "",
        *[f"- {item}" for item in result["limitations"]],
        "",
    ]

    md_path = REPORTS / "audit_results.md"
    md_path.write_text("\n".join(md_lines), encoding="utf-8")

    print(f"[NEBULA AUDIT] {status}")
    print(f"[NEBULA AUDIT] Checks: {passed}/{total}")
    print(f"[NEBULA AUDIT] Critical errors: {len(critical_errors)}")
    print(f"[NEBULA AUDIT] JSON: {json_path.relative_to(ROOT)}")
    print(f"[NEBULA AUDIT] Markdown: {md_path.relative_to(ROOT)}")

    return 0 if status == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
