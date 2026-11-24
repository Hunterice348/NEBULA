#!/usr/bin/env python3
"""
NEBULA AUTOPILOT
Main project orchestration system.

Python 3.10+
Commands:
    python scripts/autopilot.py status
    python scripts/autopilot.py audit
    python scripts/autopilot.py build
    python scripts/autopilot.py test
    python scripts/autopilot.py run
    python scripts/autopilot.py report
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import hashlib
import subprocess
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from common import (
    ROOT,
    SCRIPTS_DIR,
    REPORTS_DIR,
    BUILD_DIR,
    AutopilotError,
    log,
    ensure_directories,
    load_config,
    save_json,
    write_report,
    environment_summary,
    redact_secrets,
)

VERSION = "0.2.0"

STATUS_FILE = REPORTS_DIR / "autopilot_status.json"
REPORT_FILE = REPORTS_DIR / "autopilot_report.md"

REQUIRED_FILES = [
    "AUTOPILOT_CONFIG.yml",
    "AUTOPILOT_GOALS.yml",
    "scripts/common.py",
    "scripts/repair.py",
    "scripts/test.py",
]

OPTIONAL_MODULES = {
    "audit": SCRIPTS_DIR / "audit.py",
    "build": SCRIPTS_DIR / "build.py",
    "test": SCRIPTS_DIR / "test.py",
}

def now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")

def execute(
    command: list[str],
    timeout: int = 900,
) -> dict[str, Any]:
    """Execute a command and return a structured result."""

    log("EXEC: " + " ".join(command))

    started = datetime.now(timezone.utc)

    try:
        process = subprocess.run(
            command,
            cwd=ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
            check=False,
        )

        output = redact_secrets(process.stdout or "")

        return {
            "success": process.returncode == 0,
            "returncode": process.returncode,
            "output": output[-30000:],
            "duration_seconds": (
                datetime.now(timezone.utc) - started
            ).total_seconds(),
            "command": command,
        }

    except subprocess.TimeoutExpired as exc:
        output = exc.stdout or ""

        if isinstance(output, bytes):
            output = output.decode("utf-8", errors="replace")

        return {
            "success": False,
            "returncode": -1,
            "output": redact_secrets(str(output)),
            "error": f"Timeout after {timeout} seconds",
            "duration_seconds": (
                datetime.now(timezone.utc) - started
            ).total_seconds(),
            "command": command,
        }

    except FileNotFoundError as exc:
        return {
            "success": False,
            "returncode": -1,
            "output": "",
            "error": str(exc),
            "duration_seconds": (
                datetime.now(timezone.utc) - started
            ).total_seconds(),
            "command": command,
        }

def load_goals_safely() -> dict[str, Any]:
    """
    Load goals JSON.

    Also normalizes non-breaking spaces that can appear
    when JSON-compatible YAML is copied through a browser.
    """

    path = ROOT / "AUTOPILOT_GOALS.yml"

    if not path.is_file():
        raise AutopilotError("AUTOPILOT_GOALS.yml is missing.")

    content = path.read_text(encoding="utf-8-sig")
    content = content.replace("\u00a0", " ")

    try:
        data = json.loads(content)
    except json.JSONDecodeError as exc:
        raise AutopilotError(
            f"Invalid goals file: {exc}"
        ) from exc

    if not isinstance(data, dict):
        raise AutopilotError("Goals file must contain a JSON object.")

    return data

def check_repository() -> dict[str, Any]:
    """Check repository structure and essential files."""

    results = []

    for relative in REQUIRED_FILES:
        path = ROOT / relative

        results.append({
            "file": relative,
            "exists": path.is_file(),
        })

    missing = [
        item["file"]
        for item in results
        if not item["exists"]
    ]

    cmake_exists = (ROOT / "CMakeLists.txt").is_file()

    return {
        "success": not missing and cmake_exists,
        "required_files": results,
        "missing_files": missing,
        "cmake_exists": cmake_exists,
    }

def check_tools() -> dict[str, Any]:
    """Check the local build environment."""

    import shutil

    tools = {
        "python": sys.executable,
        "cmake": shutil.which("cmake"),
        "ctest": shutil.which("ctest"),
        "git": shutil.which("git"),
        "compiler": (
            shutil.which("clang++")
            or shutil.which("g++")
            or shutil.which("cl")
        ),
    }

    return {
        "tools": tools,
        "cmake_available": tools["cmake"] is not None,
        "compiler_available": tools["compiler"] is not None,
    }

def run_module(
    name: str,
    timeout: int,
    extra_args: list[str] | None = None,
) -> dict[str, Any]:
    """Run one project module if it exists."""

    module = OPTIONAL_MODULES.get(name)

    if module is None:
        return {
            "success": False,
            "status": "unknown_module",
            "error": f"Unknown module: {name}",
        }

    if not module.is_file():
        return {
            "success": False,
            "status": "missing",
            "error": f"Missing module: {module.relative_to(ROOT)}",
        }

    command = [
        sys.executable,
        str(module),
    ]

    if extra_args:
        command.extend(extra_args)

    result = execute(command, timeout=timeout)

    result["status"] = (
        "passed" if result["success"] else "failed"
    )

    return result

def run_audit(config: dict[str, Any]) -> dict[str, Any]:
    audit_config = config.get("audit", {})

    if not audit_config.get("enabled", True):
        return {
            "success": False,
            "status": "disabled",
            "error": "Audit is disabled in configuration.",
        }

    result = run_module("audit", timeout=900)

    if result.get("status") == "missing":
        result["error"] = (
            "scripts/audit.py has not been created yet."
        )

    return result

def run_build(config: dict[str, Any]) -> dict[str, Any]:
    build_config = config.get("build", {})

    if not build_config.get("enabled", True):
        return {
            "success": False,
            "status": "disabled",
            "error": "Build is disabled in configuration.",
        }

    if not (ROOT / "CMakeLists.txt").is_file():
        return {
            "success": False,
            "status": "blocked",
            "error": (
                "Root CMakeLists.txt is missing. "
                "The project cannot be configured yet."
            ),
        }

    timeout = int(
        build_config.get("timeout_seconds", 1800)
    )

    result = run_module("build", timeout=timeout)

    if result.get("status") == "missing":
        result["status"] = "blocked"
        result["error"] = (
            "scripts/build.py has not been created yet."
        )

    return result

def run_tests(config: dict[str, Any]) -> dict[str, Any]:
    test_config = config.get("tests", {})

    if not test_config.get("enabled", True):
        return {
            "success": False,
            "status": "disabled",
            "error": "Tests are disabled in configuration.",
        }

    if not BUILD_DIR.is_dir():
        return {
            "success": False,
            "status": "blocked",
            "error": (
                "Build directory does not exist. "
                "A successful project build is required first."
            ),
        }

    timeout = int(
        test_config.get("timeout_seconds", 900)
    )

    result = run_module("test", timeout=timeout)

    if result.get("status") == "missing":
        result["status"] = "blocked"

    return result

def fingerprint(value: str) -> str:
    return hashlib.sha256(
        value.encode("utf-8", errors="replace")
    ).hexdigest()[:16]

def load_previous_status() -> dict[str, Any]:
    if not STATUS_FILE.is_file():
        return {}

    try:
        return json.loads(
            STATUS_FILE.read_text(encoding="utf-8")
        )
    except (json.JSONDecodeError, OSError):
        return {}

def calculate_completion(
    repository: dict[str, Any],
    audit: dict[str, Any],
    build: dict[str, Any],
    tests: dict[str, Any],
    goals: dict[str, Any],
) -> dict[str, Any]:
    """
    Do not mark product features complete based only
    on the existence of files or successful execution.

    Feature-level verification will require a future
    machine-readable audit result.
    """

    phases = goals.get("phases", [])

    required_phases = [
        phase for phase in phases
        if phase.get("required", False)
    ]

    audit_results_path = REPORTS_DIR / "audit_results.json"
    verified_checks: set[str] = set()

    if audit_results_path.is_file():
        try:
            audit_data = json.loads(
                audit_results_path.read_text(encoding="utf-8")
            )

            raw_checks = audit_data.get("checks", {})

            if isinstance(raw_checks, dict):
                verified_checks = {
                    key
                    for key, value in raw_checks.items()
                    if value is True
                }

        except (json.JSONDecodeError, OSError):
            verified_checks = set()

    phase_results = []

    for phase in required_phases:
        checks = phase.get("checks", [])

        missing_checks = [
            check
            for check in checks
            if check not in verified_checks
        ]

        phase_results.append({
            "id": phase.get("id", "unknown"),
            "name": phase.get("name", "Unnamed phase"),
            "required_checks": len(checks),
            "verified_checks": len(checks) - len(missing_checks),
            "missing_checks": missing_checks,
            "success": len(missing_checks) == 0,
        })

    all_phases_verified = all(
        phase["success"] for phase in phase_results
    )

    build_passed = build.get("success") is True
    tests_passed = tests.get("success") is True
    audit_passed = audit.get("success") is True
    repository_valid = repository.get("success") is True

    complete = (
        repository_valid
        and all_phases_verified
        and build_passed
        and tests_passed
        and audit_passed
    )

    return {
        "complete": complete,
        "repository_valid": repository_valid,
        "all_required_phases_verified": all_phases_verified,
        "build_passed": build_passed,
        "tests_passed": tests_passed,
        "audit_passed": audit_passed,
        "verified_checks": len(verified_checks),
        "required_phases": len(required_phases),
        "phases": phase_results,
    }

def create_markdown_report(
    status: dict[str, Any],
) -> str:
    lines = [
        "# NEBULA AUTOPILOT REPORT",
        "",
        f"Generated: {status.get('timestamp', 'unknown')}",
        "",
        "## Overall status",
        "",
        (
            "**PROJECT COMPLETE**"
            if status.get("completion", {}).get("complete")
            else "**PROJECT NOT COMPLETE**"
        ),
        "",
        "## Repository",
        "",
        f"- Valid: {status['repository']['success']}",
        f"- CMakeLists.txt: {status['repository']['cmake_exists']}",
        "",
        "## Build",
        "",
        f"- Status: {status['build'].get('status', 'unknown')}",
        f"- Success: {status['build'].get('success', False)}",
        "",
        "## Tests",
        "",
        f"- Status: {status['tests'].get('status', 'unknown')}",
        f"- Success: {status['tests'].get('success', False)}",
        "",
        "## Audit",
        "",
        f"- Status: {status['audit'].get('status', 'unknown')}",
        f"- Success: {status['audit'].get('success', False)}",
        "",
        "## Required phases",
        "",
    ]

    for phase in status["completion"]["phases"]:
        marker = "PASS" if phase["success"] else "INCOMPLETE"

        lines.append(
            f"### {phase['name']} — {marker}"
        )

        lines.append(
            f"Verified checks: {phase['verified_checks']}/"
            f"{phase['required_checks']}"
        )

        if phase["missing_checks"]:
            lines.append("Missing checks:")

            for check in phase["missing_checks"]:
                lines.append(f"- `{check}`")

        lines.append("")

    lines.extend([
        "## Missing repository files",
        "",
    ])

    for relative in status["repository"]["missing_files"]:
        lines.append(f"- `{relative}`")

    if not status["repository"]["missing_files"]:
        lines.append("- None")

    lines.extend([
        "",
        "## Environment",
        "",
        "```json",
        json.dumps(
            status["environment"],
            indent=2,
            ensure_ascii=False,
        ),
        "```",
        "",
        "## Errors and blockers",
        "",
    ])

    for stage in ("audit", "build", "tests"):
        result = status[stage]

        if not result.get("success"):
            lines.append(
                f"- **{stage.upper()}**: "
                f"{result.get('error', result.get('status', 'failed'))}"
            )

    lines.extend([
        "",
        "---",
        "",
        "Generated by NEBULA AUTOPILOT.",
    ])

    return "\n".join(lines) + "\n"

def save_status(status: dict[str, Any]) -> None:
    ensure_directories()

    save_json(STATUS_FILE, status)

    write_report(
        REPORT_FILE.name,
        create_markdown_report(status),
    )

def run_pipeline(
    selected_stages: list[str] | None = None,
) -> dict[str, Any]:
    """Run selected stages and save the result."""

    ensure_directories()

    config = load_config()
    goals = load_goals_safely()

    selected = selected_stages or [
        "audit",
        "build",
        "test",
    ]

    log("NEBULA AUTOPILOT STARTED")
    log(f"Version: {VERSION}")
    log(f"Stages: {', '.join(selected)}")

    repository = check_repository()
    environment = environment_summary()
    tools = check_tools()

    results = {
        "audit": {
            "success": False,
            "status": "not_run",
        },
        "build": {
            "success": False,
            "status": "not_run",
        },
        "tests": {
            "success": False,
            "status": "not_run",
        },
    }

    if "audit" in selected:
        log("STAGE 1: AUDIT")
        results["audit"] = run_audit(config)

    if "build" in selected:
        log("STAGE 2: BUILD")
        results["build"] = run_build(config)

    if "test" in selected:
        log("STAGE 3: TEST")
        results["tests"] = run_tests(config)

    completion = calculate_completion(
        repository,
        results["audit"],
        results["build"],
        results["tests"],
        goals,
    )

    status = {
        "timestamp": now(),
        "version": VERSION,
        "environment": environment,
        "tools": tools,
        "repository": repository,
        "audit": results["audit"],
        "build": results["build"],
        "tests": results["tests"],
        "completion": completion,
    }

    previous = load_previous_status()

    errors = []

    for stage in ("audit", "build", "tests"):
        result = status[stage]

        if not result.get("success"):
            errors.append(
                result.get("error")
                or result.get("status")
                or "unknown error"
            )

    current_fingerprint = fingerprint(
        "\n".join(errors)
    ) if errors else None

    previous_fingerprint = previous.get("error_fingerprint")
    repeated_count = previous.get("repeated_error_count", 0)

    if current_fingerprint and current_fingerprint == previous_fingerprint:
        repeated_count += 1
    elif current_fingerprint:
        repeated_count = 1
    else:
        repeated_count = 0

    status["error_fingerprint"] = current_fingerprint
    status["repeated_error_count"] = repeated_count

    save_status(status)

    log("AUTOPILOT FINISHED")
    log(
        "Completion: "
        + ("100% verified" if completion["complete"] else "incomplete")
    )

    return status

def print_status(status: dict[str, Any]) -> None:
    print("\n========== NEBULA AUTOPILOT ==========")

    print(
        "PROJECT:",
        "COMPLETE" if status["completion"]["complete"]
        else "NOT COMPLETE",
    )

    print(
        "REPOSITORY:",
        "PASS" if status["repository"]["success"]
        else "INCOMPLETE",
    )

    for stage in ("audit", "build", "tests"):
        result = status[stage]

        print(
            f"{stage.upper():8}: "
            f"{result.get('status', 'unknown')}"
        )

    print(
        "VERIFIED CHECKS:",
        status["completion"]["verified_checks"],
    )

    print(
        "REQUIRED PHASES:",
        status["completion"]["required_phases"],
    )

    print("\nREPORT:")
    print("reports/autopilot_report.md")
    print("reports/autopilot_status.json")

    print("======================================\n")

def load_saved_status() -> dict[str, Any]:
    if not STATUS_FILE.is_file():
        raise AutopilotError(
            "No report exists yet. Run AUTOPILOT first."
        )

    return json.loads(
        STATUS_FILE.read_text(encoding="utf-8")
    )

def main() -> int:
    parser = argparse.ArgumentParser(
        description="NEBULA autonomous development orchestrator."
    )

    parser.add_argument(
        "command",
        choices=[
            "status",
            "audit",
            "build",
            "test",
            "run",
            "report",
        ],
        help="Operation to execute.",
    )

    args = parser.parse_args()

    try:
        if args.command == "status":
            status = load_saved_status()
            print_status(status)
            return 0

        if args.command == "report":
            status = load_saved_status()

            print(
                create_markdown_report(status)
            )

            return 0

        stage_map = {
            "audit": ["audit"],
            "build": ["build"],
            "test": ["test"],
            "run": ["audit", "build", "test"],
        }

        status = run_pipeline(
            stage_map[args.command]
        )

        print_status(status)

        return (
            0
            if status["completion"]["complete"]
            else 1
        )

    except (AutopilotError, OSError, ValueError) as exc:
        log(str(exc), "ERROR")
        return 1

    except KeyboardInterrupt:
        log("Interrupted by user.", "WARNING")
        return 130

if __name__ == "__main__":
    raise SystemExit(main())
