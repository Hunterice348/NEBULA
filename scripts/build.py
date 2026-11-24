#!/usr/bin/env python3
"""
NEBULA BUILD ENGINE

CMake configure/build runner for NEBULA.
Python 3.10+

Usage:
    python scripts/build.py
    python scripts/build.py --config Release
    python scripts/build.py --config Debug
    python scripts/build.py --clean
    python scripts/build.py --target MyPlugin_VST3
    python scripts/build.py --skip-configure
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import time

from datetime import datetime, timezone
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
CONFIG_FILE = ROOT / "AUTOPILOT_CONFIG.yml"
REPORT_DIR = ROOT / "reports"
REPORT_FILE = REPORT_DIR / "build_results.json"

def timestamp() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")

def log(message: str, level: str = "INFO") -> None:
    print(f"[{timestamp()}] [{level}] {message}", flush=True)

def load_config() -> dict[str, Any]:
    if not CONFIG_FILE.is_file():
        raise RuntimeError("AUTOPILOT_CONFIG.yml not found.")

    try:
        data = json.loads(CONFIG_FILE.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise RuntimeError(
            f"Invalid JSON-compatible YAML configuration: {exc}"
        ) from exc

    if not isinstance(data, dict):
        raise RuntimeError("Configuration root must be an object.")

    return data

def save_report(data: dict[str, Any]) -> None:
    REPORT_DIR.mkdir(parents=True, exist_ok=True)

    temporary = REPORT_FILE.with_suffix(".json.tmp")

    temporary.write_text(
        json.dumps(data, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )

    temporary.replace(REPORT_FILE)

    log(f"Report saved: {REPORT_FILE.relative_to(ROOT)}")

def safe_build_directory(relative: str) -> Path:
    candidate = (ROOT / relative).resolve()

    try:
        candidate.relative_to(ROOT.resolve())
    except ValueError as exc:
        raise RuntimeError(
            "Build directory must remain inside the repository."
        ) from exc

    if candidate == ROOT.resolve():
        raise RuntimeError(
            "Build directory cannot be the repository root."
        )

    return candidate

def execute(
    command: list[str],
    timeout: int,
    environment: dict[str, str] | None = None,
) -> dict[str, Any]:
    log("EXEC: " + " ".join(command))

    started = time.monotonic()

    try:
        process = subprocess.run(
            command,
            cwd=ROOT,
            env=environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
            check=False,
        )

        output = process.stdout or ""

        if output:
            print(output[-30000:], flush=True)

        return {
            "success": process.returncode == 0,
            "returncode": process.returncode,
            "duration_seconds": round(
                time.monotonic() - started, 2
            ),
            "output": output[-30000:],
            "command": command,
        }

    except subprocess.TimeoutExpired as exc:
        output = exc.stdout or ""

        if isinstance(output, bytes):
            output = output.decode("utf-8", errors="replace")

        return {
            "success": False,
            "returncode": -1,
            "duration_seconds": round(
                time.monotonic() - started, 2
            ),
            "output": str(output)[-30000:],
            "error": f"Command timed out after {timeout} seconds.",
            "command": command,
        }

    except OSError as exc:
        return {
            "success": False,
            "returncode": -1,
            "duration_seconds": round(
                time.monotonic() - started, 2
            ),
            "output": "",
            "error": str(exc),
            "command": command,
        }

def detect_environment() -> dict[str, Any]:
    return {
        "python": sys.version.split()[0],
        "platform": sys.platform,
        "cmake": shutil.which("cmake"),
        "ctest": shutil.which("ctest"),
        "ninja": shutil.which("ninja"),
        "git": shutil.which("git"),
        "compiler": (
            shutil.which("clang++")
            or shutil.which("g++")
            or shutil.which("cl")
        ),
    }

def make_result(
    *,
    success: bool,
    status: str,
    configuration: str,
    build_directory: Path,
    environment: dict[str, Any],
    configure: dict[str, Any] | None = None,
    build: dict[str, Any] | None = None,
    error: str | None = None,
) -> dict[str, Any]:
    return {
        "timestamp": timestamp(),
        "project": "NEBULA",
        "success": success,
        "status": status,
        "configuration": configuration,
        "build_directory": str(
            build_directory.relative_to(ROOT)
        ),
        "environment": environment,
        "configure": configure,
        "build": build,
        "error": error,
    }

def run_build(args: argparse.Namespace) -> int:
    config = load_config()
    build_config = config.get("build", {})

    if not build_config.get("enabled", True):
        raise RuntimeError("Build is disabled in AUTOPILOT_CONFIG.yml.")

    configuration = args.config or build_config.get(
        "configuration", "Release"
    )

    if configuration not in ("Debug", "Release", "RelWithDebInfo", "MinSizeRel"):
        raise RuntimeError(
            f"Unsupported build configuration: {configuration}"
        )

    build_directory = safe_build_directory(
        args.build_dir or build_config.get(
            "build_directory", "build"
        )
    )

    timeout = int(
        build_config.get("timeout_seconds", 1800)
    )

    environment = detect_environment()

    log("NEBULA BUILD ENGINE STARTED")
    log(f"Configuration: {configuration}")
    log(f"Build directory: {build_directory}")

    def finish(
        status: str,
        success: bool = False,
        configure_result: dict[str, Any] | None = None,
        build_result: dict[str, Any] | None = None,
        error: str | None = None,
    ) -> int:
        result = make_result(
            success=success,
            status=status,
            configuration=configuration,
            build_directory=build_directory,
            environment=environment,
            configure=configure_result,
            build=build_result,
            error=error,
        )

        save_report(result)

        log(
            "BUILD SUCCESSFUL" if success else f"BUILD FAILED: {status}",
            "INFO" if success else "ERROR",
        )

        return 0 if success else 1

    cmake = environment["cmake"]

    if not cmake:
        return finish(
            "cmake_missing",
            error="CMake was not found. Install CMake and add it to PATH.",
        )

    if not environment["compiler"]:
        log(
            "No compiler was detected in PATH. "
            "CMake will attempt to detect the configured toolchain.",
            "WARNING",
        )

    cmake_lists = ROOT / "CMakeLists.txt"

    if not cmake_lists.is_file():
        return finish(
            "cmake_project_missing",
            error=(
                "Root CMakeLists.txt is missing. "
                "A real CMake project must be added before compilation."
            ),
        )

    if args.clean and build_directory.exists():
        log("Removing previous build directory.")
        shutil.rmtree(build_directory)

    build_directory.mkdir(parents=True, exist_ok=True)

    configure_result = None

    if not args.skip_configure:
        configure_command = [
            cmake,
            "-S", str(ROOT),
            "-B", str(build_directory),
        ]

        generator = args.generator or build_config.get("generator")

        if generator:
            configure_command.extend(["-G", generator])

        if not generator or "Visual Studio" not in generator:
            configure_command.append(
                f"-DCMAKE_BUILD_TYPE={configuration}"
            )

        configure_command.extend(
            map(str, build_config.get("cmake_args", []))
        )

        log("STEP 1/2: CMAKE CONFIGURATION")

        configure_result = execute(
            configure_command,
            timeout=min(timeout, 600),
        )

        if not configure_result["success"]:
            return finish(
                "configure_failed",
                configure_result=configure_result,
                error="CMake configuration failed. See configure output.",
            )

    else:
        cache = build_directory / "CMakeCache.txt"

        if not cache.is_file():
            return finish(
                "missing_cmake_cache",
                error=(
                    "--skip-configure was requested, "
                    "but CMakeCache.txt does not exist."
                ),
            )

    build_command = [
        cmake,
        "--build",
        str(build_directory),
        "--config",
        configuration,
    ]

    if args.target:
        build_command.extend(["--target", args.target])

    parallel = args.jobs

    if parallel is None:
        parallel = os.cpu_count() or 2

    if parallel < 1:
        return finish(
            "invalid_parallel_jobs",
            configure_result=configure_result,
            error="Number of parallel build jobs must be at least 1.",
        )

    build_command.extend(["--parallel", str(parallel)])

    log("STEP 2/2: COMPILATION")

    build_result = execute(
        build_command,
        timeout=timeout,
    )

    if not build_result["success"]:
        return finish(
            "build_failed",
            configure_result=configure_result,
            build_result=build_result,
            error="Compilation failed. See build output.",
        )

    return finish(
        "passed",
        success=True,
        configure_result=configure_result,
        build_result=build_result,
    )

def main() -> int:
    parser = argparse.ArgumentParser(
        description="NEBULA CMake build system."
    )

    parser.add_argument(
        "--config",
        choices=["Debug", "Release", "RelWithDebInfo", "MinSizeRel"],
    )

    parser.add_argument("--build-dir")
    parser.add_argument("--generator")
    parser.add_argument("--target")
    parser.add_argument("--jobs", type=int)
    parser.add_argument("--clean", action="store_true")
    parser.add_argument("--skip-configure", action="store_true")

    args = parser.parse_args()

    try:
        return run_build(args)

    except KeyboardInterrupt:
        log("Build interrupted by user.", "WARNING")
        return 130

    except (RuntimeError, OSError, ValueError) as exc:
        log(str(exc), "ERROR")
        return 1

if __name__ == "__main__":
    raise SystemExit(main())
