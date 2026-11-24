#!/usr/bin/env python3
# NEBULA AUTOPILOT — Shared Core
# Python 3.10+

from __future__ import annotations

import json
import os
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Sequence

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS_DIR = ROOT / "scripts"
REPORTS_DIR = ROOT / "reports"
BUILD_DIR = ROOT / "build"

CONFIG_FILE = ROOT / "AUTOPILOT_CONFIG.yml"
GOALS_FILE = ROOT / "AUTOPILOT_GOALS.yml"

AUTOPILOT_VERSION = "0.1.0"

class AutopilotError(RuntimeError):
    """Base exception for AUTOPILOT errors."""

def timestamp() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")

def log(message: str, level: str = "INFO") -> None:
    print(
        f"[{timestamp()}] [{level.upper()}] {message}",
        flush=True
    )

def ensure_directories() -> None:
    REPORTS_DIR.mkdir(parents=True, exist_ok=True)

def load_yaml_json(path: Path) -> dict[str, Any]:
    """
    Read JSON-compatible YAML.
    This keeps the first AUTOPILOT version dependency-free.
    """
    if not path.is_file():
        raise AutopilotError(f"Configuration not found: {path}")

    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise AutopilotError(
            f"{path.name} must contain valid JSON-compatible YAML: {exc}"
        ) from exc

    if not isinstance(data, dict):
        raise AutopilotError(f"{path.name} must contain an object.")

    return data

def load_config() -> dict[str, Any]:
    return load_yaml_json(CONFIG_FILE)

def load_goals() -> dict[str, Any]:
    return load_yaml_json(GOALS_FILE)

def save_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")

    temporary.write_text(
        json.dumps(data, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8"
    )

    temporary.replace(path)

def run_command(
    command: Sequence[str],
    *,
    cwd: Path = ROOT,
    timeout: int = 900,
    env: dict[str, str] | None = None,
    check: bool = False
) -> subprocess.CompletedProcess[str]:
    if not command:
        raise AutopilotError("Cannot execute an empty command.")

    log("EXEC: " + " ".join(map(str, command)))

    try:
        result = subprocess.run(
            list(map(str, command)),
            cwd=cwd,
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=timeout,
            check=False
        )
    except FileNotFoundError as exc:
        raise AutopilotError(
            f"Executable not found: {command[0]}"
        ) from exc
    except subprocess.TimeoutExpired as exc:
        raise AutopilotError(
            f"Command timed out after {timeout} seconds."
        ) from exc

    if result.stdout:
        print(result.stdout[-20000:], flush=True)

    if check and result.returncode != 0:
        raise AutopilotError(
            f"Command failed with exit code {result.returncode}."
        )

    return result

def git(*args: str, check: bool = True) -> str:
    result = run_command(
        ["git", *args],
        timeout=60,
        check=check
    )
    return result.stdout.strip()

def git_branch() -> str:
    return git("branch", "--show-current")

def git_is_clean() -> bool:
    return not git("status", "--porcelain")

def ensure_safe_branch() -> None:
    branch = git_branch()

    if branch in ("main", "master"):
        raise AutopilotError(
            "AUTOPILOT refuses to modify main/master. "
            "Use a dedicated working branch."
        )

    if not git_is_clean():
        raise AutopilotError(
            "Git working tree is not clean. "
            "Commit or stash changes before automated repair."
        )

def safe_repo_path(relative_path: str) -> Path:
    candidate = (ROOT / relative_path).resolve()

    try:
        candidate.relative_to(ROOT.resolve())
    except ValueError as exc:
        raise AutopilotError(
            f"Path escapes repository: {relative_path}"
        ) from exc

    return candidate

def collect_source_files(
    extensions: tuple[str, ...] = (".cpp", ".h", ".hpp", ".c", ".cc"),
    excluded: tuple[str, ...] = (
        ".git",
        "build",
        "JUCE",
        "third_party",
        "node_modules",
        ".venv"
    )
) -> list[Path]:
    files: list[Path] = []

    for path in ROOT.rglob("*"):
        if not path.is_file():
            continue

        relative = path.relative_to(ROOT)

        if any(part in excluded for part in relative.parts):
            continue

        if path.suffix.lower() in extensions:
            files.append(relative)

    return sorted(files)

def redact_secrets(value: str) -> str:
    """
    Prevent common secret values from appearing in reports.
    """
    secret_names = (
        "NEBULA_AI_API_KEY",
        "GITHUB_TOKEN",
        "GH_TOKEN"
    )

    result = value

    for name in secret_names:
        secret = os.getenv(name)
        if secret:
            result = result.replace(secret, "[REDACTED]")

    return result

def write_report(name: str, content: str) -> Path:
    if Path(name).name != name:
        raise AutopilotError("Report name must not contain directories.")

    ensure_directories()

    path = REPORTS_DIR / name
    path.write_text(
        redact_secrets(content),
        encoding="utf-8"
    )

    log(f"Report saved: {path.relative_to(ROOT)}")
    return path

def environment_summary() -> dict[str, str]:
    return {
        "python": sys.version.split()[0],
        "platform": sys.platform,
        "repository": str(ROOT),
        "branch": git_branch() if (ROOT / ".git").exists() else "unknown",
        "autopilot_version": AUTOPILOT_VERSION
    }

def require_file(path: Path, description: str) -> None:
    if not path.is_file():
        raise AutopilotError(
            f"Required {description} is missing: "
            f"{path.relative_to(ROOT)}"
        )

