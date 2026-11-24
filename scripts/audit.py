#!/usr/bin/env python3
"""
NEBULA STATIC AUDIT ENGINE

Python 3.10+

Checks:
- Repository structure
- CMake and JUCE presence
- C++ source structure
- Python syntax
- Basic real-time audio safety patterns
- TODO / FIXME markers
- Documentation
- Goal checklist coverage

IMPORTANT:
This is a static audit, not a substitute for compilation,
automated DSP tests, code review or audio validation.
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import sys

from datetime import datetime, timezone
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / "scripts"
REPORTS = ROOT / "reports"

GOALS_FILE = ROOT / "AUTOPILOT_GOALS.yml"
CONFIG_FILE = ROOT / "AUTOPILOT_CONFIG.yml"

RESULT_FILE = REPORTS / "audit_results.json"
MARKDOWN_FILE = REPORTS / "audit_report.md"

VERSION = "0.1.0"

CPP_EXTENSIONS = {
    ".cpp",
    ".cc",
    ".c",
    ".h",
    ".hpp",
    ".hh",
    ".hxx",
}

EXCLUDED_DIRECTORIES = {
    ".git",
    "build",
    "JUCE",
    "third_party",
    "node_modules",
    ".venv",
    "__pycache__",
}

MAX_SOURCE_BYTES = 2_000_000

class AuditError(RuntimeError):
    """Audit execution error."""

def timestamp() -> str:
    return datetime.now(timezone.utc).isoformat(
        timespec="seconds"
    )

def log(message: str, level: str = "INFO") -> None:
    print(
        f"[{level.upper()}] {message}",
        flush=True,
    )

def ensure_reports() -> None:
    REPORTS.mkdir(
        parents=True,
        exist_ok=True,
    )

def load_json_compatible_yaml(path: Path) -> dict[str, Any]:
    """
    Current NEBULA configuration files use JSON syntax,
    which is also valid YAML syntax.
    """

    if not path.is_file():
        raise AuditError(
            f"Required configuration file is missing: {path.name}"
        )

    try:
        result = json.loads(
            path.read_text(
                encoding="utf-8-sig"
            ).replace("\u00a0", " ")
        )
    except json.JSONDecodeError as exc:
        raise AuditError(
            f"Invalid configuration {path.name}: {exc}"
        ) from exc

    if not isinstance(result, dict):
        raise AuditError(
            f"{path.name} must contain a JSON object."
        )

    return result

def collect_files(
    extensions: set[str],
) -> list[Path]:

    result = []

    for path in ROOT.rglob("*"):

        if not path.is_file():
            continue

        relative = path.relative_to(ROOT)

        if any(
            part in EXCLUDED_DIRECTORIES
            for part in relative.parts
        ):
            continue

        if path.suffix.lower() not in extensions:
            continue

        try:
            if path.stat().st_size > MAX_SOURCE_BYTES:
                continue
        except OSError:
            continue

        result.append(path)

    return sorted(result)

def relative(path: Path) -> str:
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return str(path)

class AuditEngine:

    def __init__(self) -> None:

        self.issues: list[dict[str, Any]] = []
        self.warnings: list[dict[str, Any]] = []
        self.checks: dict[str, bool] = {}
        self.evidence: dict[str, list[str]] = {}

        self.cpp_files: list[Path] = []
        self.python_files: list[Path] = []
        self.cmake_files: list[Path] = []

        self.source_text: dict[str, str] = {}

        self.goals: dict[str, Any] = {}
        self.config: dict[str, Any] = {}

    def issue(
        self,
        code: str,
        message: str,
        severity: str = "error",
        file: str | None = None,
    ) -> None:

        item = {
            "code": code,
            "message": message,
            "severity": severity,
            "file": file,
        }

        if severity == "warning":
            self.warnings.append(item)
        else:
            self.issues.append(item)

        log(
            f"{code}: {message}",
            severity,
        )

    def set_check(
        self,
        name: str,
        passed: bool,
        evidence: list[str] | None = None,
    ) -> None:

        self.checks[name] = bool(passed)
        self.evidence[name] = evidence or []

    def discover_files(self) -> None:

        self.cpp_files = collect_files(CPP_EXTENSIONS)
        self.python_files = collect_files({".py"})
        self.cmake_files = collect_files(
            {".cmake", ".txt"}
        )

        for path in self.cpp_files:

            try:
                self.source_text[relative(path)] = (
                    path.read_text(
                        encoding="utf-8",
                        errors="replace",
                    )
                )
            except OSError as exc:
                self.issue(
                    "SOURCE_READ_ERROR",
                    str(exc),
                    file=relative(path),
                )

        log(
            f"C++ files discovered: {len(self.cpp_files)}"
        )

        log(
            f"Python files discovered: {len(self.python_files)}"
        )

    def check_repository_structure(self) -> None:

        required = [
            "AUTOPILOT_CONFIG.yml",
            "AUTOPILOT_GOALS.yml",
            "scripts/common.py",
            "scripts/repair.py",
            "scripts/test.py",
            "scripts/autopilot.py",
            "scripts/audit.py",
        ]

        missing = []

        for item in required:

            if not (ROOT / item).is_file():
                missing.append(item)

        if missing:

            for item in missing:
                self.issue(
                    "MISSING_REQUIRED_FILE",
                    f"Required file is missing: {item}",
                    "critical",
                )

        source_directories = [
            ROOT / "Source",
            ROOT / "source",
            ROOT / "src",
        ]

        has_source_directory = any(
            path.is_dir()
            for path in source_directories
        )

        self.set_check(
            "project_structure_valid",
            not missing and has_source_directory,
            [
                relative(path)
                for path in source_directories
                if path.is_dir()
            ],
        )

        if not has_source_directory:
            self.issue(
                "SOURCE_DIRECTORY_MISSING",
                "No Source/, source/ or src/ directory found.",
                "critical",
            )

    def check_cmake(self) -> None:

        cmake_file = ROOT / "CMakeLists.txt"
        cmake = shutil.which("cmake")

        exists = cmake_file.is_file()

        if not exists:
            self.issue(
                "CMAKE_FILE_MISSING",
                "Root CMakeLists.txt does not exist.",
                "critical",
            )

        if not cmake:
            self.issue(
                "CMAKE_NOT_INSTALLED",
                "CMake executable was not found.",
                "warning",
            )

        self.set_check(
            "cmake_configuration_valid",
            False,
            [
                "CMake configuration has not been executed "
                "by this static audit."
            ],
        )

        if exists and cmake:
            log(
                "CMakeLists.txt and CMake executable detected. "
                "Actual configuration must be tested by build.py."
            )

    def check_juce(self) -> None:

        juce_locations = [
            ROOT / "JUCE",
            ROOT / "juce",
            ROOT / "third_party" / "JUCE",
            ROOT / "modules",
        ]

        juce_paths = [
            path
            for path in juce_locations
            if path.is_dir()
        ]

        cmake_text = ""

        cmake_file = ROOT / "CMakeLists.txt"

        if cmake_file.is_file():
            cmake_text = cmake_file.read_text(
                encoding="utf-8",
                errors="replace",
            )

        juce_reference = bool(
            re.search(
                r"find_package\s*\(\s*JUCE|"
                r"add_subdirectory\s*\([^)]*JUCE|"
                r"juce_add_plugin|"
                r"JUCE_DIR",
                cmake_text,
                re.IGNORECASE,
            )
        )

        found = bool(juce_paths) or juce_reference

        self.set_check(
            "juce_dependency_available",
            bool(juce_paths),
            [
                relative(path)
                for path in juce_paths
            ],
        )

        if not found:
            self.issue(
                "JUCE_NOT_DETECTED",
                "JUCE directory or CMake dependency reference not found.",
                "critical",
            )

        elif not juce_paths:
            self.issue(
                "JUCE_DEPENDENCY_UNVERIFIED",
                "JUCE is referenced in CMake, but its local "
                "availability has not been verified.",
                "warning",
            )

    def check_source_presence(self) -> None:

        has_cpp = bool(self.cpp_files)

        self.set_check(
            "shared_core_present",
            False,
            [
                relative(path)
                for path in self.cpp_files[:30]
            ],
        )

        if not has_cpp:
            self.issue(
                "CPP_SOURCE_MISSING",
                "No C++ source or header files were found.",
                "critical",
            )

        else:
            log(
                f"Found {len(self.cpp_files)} C++ source/header files."
            )

    def check_documentation(self) -> None:

        candidates = [
            ROOT / "README.md",
            ROOT / "README",
            ROOT / "docs",
            ROOT / "documentation",
        ]

        found = [
            relative(path)
            for path in candidates
            if path.exists()
        ]

        self.set_check(
            "documentation_present",
            bool(found),
            found,
        )

        if not found:
            self.issue(
                "DOCUMENTATION_MISSING",
                "README or documentation directory was not found.",
                "warning",
            )

    def check_python_syntax(self) -> None:

        failed = []

        for path in self.python_files:

            try:
                source = path.read_text(
                    encoding="utf-8"
                )

                compile(
                    source,
                    str(path),
                    "exec",
                )

            except (SyntaxError, UnicodeError, OSError) as exc:

                failed.append({
                    "file": relative(path),
                    "error": str(exc),
                })

                self.issue(
                    "PYTHON_SYNTAX_ERROR",
                    str(exc),
                    "error",
                    relative(path),
                )

        self.set_check(
            "autopilot_python_syntax",
            not failed,
            [
                relative(path)
                for path in self.python_files
            ],
        )

        if not failed:
            log("Python syntax checks passed.")

    def check_cpp_patterns(self) -> None:

        todo_items = []
        realtime_items = []

        todo_pattern = re.compile(
            r"\b(TODO|FIXME|HACK|XXX)\b"
        )

        realtime_patterns = {
            "heap_allocation": re.compile(
                r"\b(new|delete|malloc|calloc|realloc|free)\b"
            ),
            "mutex_lock": re.compile(
                r"\b(lock_guard|unique_lock|scoped_lock)\b"
            ),
            "blocking_wait": re.compile(
                r"\b(sleep_for|sleep_until|wait_for|wait_until)\b"
            ),
        }

        for filename, source in self.source_text.items():

            lines = source.splitlines()

            for number, line in enumerate(lines, start=1):

                if todo_pattern.search(line):

                    todo_items.append({
                        "file": filename,
                        "line": number,
                        "text": line.strip()[:240],
                    })

                for category, pattern in realtime_patterns.items():

                    if pattern.search(line):

                        realtime_items.append({
                            "category": category,
                            "file": filename,
                            "line": number,
                            "text": line.strip()[:240],
                        })

        if todo_items:

            self.issue(
                "TODO_MARKERS_FOUND",
                f"Found {len(todo_items)} TODO/FIXME/HACK markers.",
                "warning",
            )

        if realtime_items:

            self.issue(
                "REALTIME_SAFETY_REVIEW",
                (
                    f"Found {len(realtime_items)} potentially "
                    "unsafe real-time code patterns. "
                    "Manual context review is required."
                ),
                "warning",
            )

        self.set_check(
            "no_known_critical_errors",
            not any(
                item["severity"] == "critical"
                for item in self.issues
            ),
            [],
        )

        self.set_check(
            "no_audio_thread_blocking",
            False,
            [
                "Static pattern scan is not sufficient "
                "to prove audio-thread safety."
            ],
        )

        self.set_check(
            "no_audio_thread_allocations",
            False,
            [
                "Static pattern scan is not sufficient "
                "to prove allocation-free audio processing."
            ],
        )

        self.set_check(
            "todo_review",
            not todo_items,
            [
                f"{item['file']}:{item['line']}"
                for item in todo_items[:100]
            ],
        )

        self.evidence["realtime_review"] = realtime_items
        self.evidence["todo_markers"] = todo_items

    def check_goal_file(self) -> None:

        phases = self.goals.get("phases", [])

        if not isinstance(phases, list):
            self.issue(
                "INVALID_GOALS_FORMAT",
                "The phases property must be an array.",
                "critical",
            )
            return

        all_checks = []

        for phase in phases:

            if not isinstance(phase, dict):
                continue

            checks = phase.get("checks", [])

            if isinstance(checks, list):
                all_checks.extend(
                    item
                    for item in checks
                    if isinstance(item, str)
                )

        if not all_checks:
            self.issue(
                "EMPTY_GOALS",
                "No acceptance checks were found in the goals file.",
                "critical",
            )

        # Feature-level checks are deliberately not marked
        # as passed by keyword matching alone.
        for check in all_checks:
            self.checks.setdefault(check, False)

        self.evidence["goal_checks"] = all_checks

        log(
            f"Acceptance checks loaded: {len(all_checks)}"
        )

    def check_cmake_files(self) -> None:

        for path in self.cmake_files:

            if path.name != "CMakeLists.txt" and path.suffix != ".cmake":
                continue

            try:
                content = path.read_text(
                    encoding="utf-8",
                    errors="replace",
                )
            except OSError:
                continue

            if "\x00" in content:
                self.issue(
                    "INVALID_CMAKE_CONTENT",
                    "Unexpected binary null byte found.",
                    "error",
                    relative(path),
                )

    def build_result(self) -> dict[str, Any]:

        critical_count = sum(
            1
            for item in self.issues
            if item["severity"] == "critical"
        )

        error_count = sum(
            1
            for item in self.issues
            if item["severity"] == "error"
        )

        warning_count = len(self.warnings)

        success = critical_count == 0 and error_count == 0

        return {
            "schema_version": "1.0",
            "audit_version": VERSION,
            "timestamp": timestamp(),
            "success": success,
            "summary": {
                "critical": critical_count,
                "errors": error_count,
                "warnings": warning_count,
                "cpp_files": len(self.cpp_files),
                "python_files": len(self.python_files),
                "cmake_files": len(self.cmake_files),
                "goal_checks": len(self.checks),
                "verified_goal_checks": sum(
                    1 for value in self.checks.values()
                    if value
                ),
            },
            "checks": self.checks,
            "evidence": self.evidence,
            "issues": self.issues,
            "warnings": self.warnings,
        }

    def run(self) -> dict[str, Any]:

        log("NEBULA AUDIT STARTED")
        log(f"Audit version: {VERSION}")

        self.config = load_json_compatible_yaml(
            CONFIG_FILE
        )

        self.goals = load_json_compatible_yaml(
            GOALS_FILE
        )

        self.discover_files()

        self.check_repository_structure()
        self.check_cmake()
        self.check_juce()
        self.check_source_presence()
        self.check_documentation()
        self.check_python_syntax()
        self.check_cpp_patterns()
        self.check_goal_file()
        self.check_cmake_files()

        result = self.build_result()

        ensure_reports()

        RESULT_FILE.write_text(
            json.dumps(
                result,
                indent=2,
                ensure_ascii=False,
            ) + "\n",
            encoding="utf-8",
        )

        MARKDOWN_FILE.write_text(
            create_markdown_report(result),
            encoding="utf-8",
        )

        log(
            f"Audit finished: {result['summary']}"
        )

        log(
            "JSON report: reports/audit_results.json"
        )

        log(
            "Markdown report: reports/audit_report.md"
        )

        return result

def create_markdown_report(
    result: dict[str, Any],
) -> str:

    summary = result["summary"]

    lines = [
        "# NEBULA STATIC AUDIT",
        "",
        f"Date: {result['timestamp']}",
        f"Version: {result['audit_version']}",
        "",
        "## Result",
        "",
        (
            "**AUDIT PASSED**"
            if result["success"]
            else "**AUDIT FAILED**"
        ),
        "",
        "## Summary",
        "",
        f"- Critical: {summary['critical']}",
        f"- Errors: {summary['errors']}",
        f"- Warnings: {summary['warnings']}",
        f"- C++ files: {summary['cpp_files']}",
        f"- Python files: {summary['python_files']}",
        f"- CMake files: {summary['cmake_files']}",
        f"- Acceptance checks: {summary['goal_checks']}",
        f"- Verified checks: {summary['verified_goal_checks']}",
        "",
        "## Acceptance checks",
        "",
    ]

    for name, passed in result["checks"].items():

        status = "PASS" if passed else "NOT VERIFIED"

        lines.append(
            f"- `{name}`: {status}"
        )

    lines.extend([
        "",
        "## Critical issues and errors",
        "",
    ])

    if not result["issues"]:
        lines.append("No critical issues or errors detected.")
    else:
        for item in result["issues"]:
            lines.append(
                f"- [{item['severity'].upper()}] "
                f"{item['code']}: {item['message']}"
            )

    lines.extend([
        "",
        "## Warnings",
        "",
    ])

    if not result["warnings"]:
        lines.append("No warnings.")
    else:
        for item in result["warnings"]:
            lines.append(
                f"- {item['code']}: {item['message']}"
            )

    lines.extend([
        "",
        "## Important limitations",
        "",
        "- Static scanning does not prove DSP correctness.",
        "- Static scanning does not prove thread safety.",
        "- Static scanning does not replace CMake configuration.",
        "- Static scanning does not replace VST3 compilation.",
        "- Static scanning does not replace audio tests.",
        "- Feature checks require stronger verification.",
        "",
        "Generated by NEBULA AUTOPILOT.",
    ])

    return "\n".join(lines) + "\n"

def main() -> int:

    parser = argparse.ArgumentParser(
        description="Run NEBULA static code and architecture audit."
    )

    parser.add_argument(
        "--strict",
        action="store_true",
        help="Return non-zero if warnings are present.",
    )

    args = parser.parse_args()

    try:

        engine = AuditEngine()
        result = engine.run()

        print(
            "\n========== NEBULA AUDIT =========="
        )

        print(
            "Result:",
            "PASS" if result["success"] else "FAIL",
        )

        print(
            "Critical:",
            result["summary"]["critical"],
        )

        print(
            "Errors:",
            result["summary"]["errors"],
        )

        print(
            "Warnings:",
            result["summary"]["warnings"],
        )

        print(
            "Verified goal checks:",
            result["summary"]["verified_goal_checks"],
        )

        print(
            "==================================\n"
        )

        if not result["success"]:
            return 1

        if args.strict and result["warnings"]:
            return 1

        return 0

    except (AuditError, OSError, ValueError) as exc:

        log(str(exc), "ERROR")
        return 1

    except KeyboardInterrupt:

        log("Audit interrupted.", "WARNING")
        return 130

if __name__ == "__main__":
    raise SystemExit(main())

