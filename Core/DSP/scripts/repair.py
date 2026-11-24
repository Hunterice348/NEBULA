
#!/usr/bin/env python3
"""AI-assisted, guarded patch generation for NEBULA."""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAX_FILE_BYTES = 80_000
MAX_LOG_BYTES = 40_000
MAX_PATCH_BYTES = 200_000


def git(*args: str) -> subprocess.CompletedProcess:
    return subprocess.run(
        ["git", *args],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )


def check_git_safety() -> None:
    if not (ROOT / ".git").exists():
        raise RuntimeError("Not a Git repository.")

    branch = git("branch", "--show-current")
    if branch.returncode != 0:
        raise RuntimeError(branch.stderr)

    if branch.stdout.strip() in ("main", "master"):
        raise RuntimeError(
            "AI repair is forbidden on main/master. "
            "Create a dedicated working branch first."
        )

    status = git("status", "--porcelain")
    if status.returncode != 0:
        raise RuntimeError(status.stderr)

    if status.stdout.strip():
        raise RuntimeError(
            "Working tree is not clean. Commit or stash your changes "
            "before requesting an AI patch."
        )


def safe_file(relative: str) -> Path:
    candidate = (ROOT / relative).resolve()

    try:
        candidate.relative_to(ROOT.resolve())
    except ValueError:
        raise RuntimeError(f"Path escapes repository: {relative}")

    if not candidate.is_file():
        raise RuntimeError(f"File not found: {relative}")

    if candidate.stat().st_size > MAX_FILE_BYTES:
        raise RuntimeError(f"File too large for this request: {relative}")

    return candidate


def read_file(relative: str) -> str:
    path = safe_file(relative)
    return path.read_text(encoding="utf-8", errors="replace")


def request_patch(
    base_url: str,
    api_key: str,
    model: str,
    prompt: str
) -> str:
    endpoint = base_url.rstrip("/") + "/chat/completions"

    payload = {
        "model": model,
        "temperature": 0.1,
        "messages": [
            {
                "role": "system",
                "content": (
                    "You are a senior C++17, JUCE 8 and CMake engineer. "
                    "Return ONLY a valid unified diff. Do not use Markdown "
                    "fences. Do not delete unrelated functionality. "
                    "Do not modify build artifacts, secrets or workflow "
                    "permissions. Do not invent APIs."
                )
            },
            {"role": "user", "content": prompt}
        ]
    }

    data = json.dumps(payload).encode("utf-8")
    request = urllib.request.Request(
        endpoint,
        data=data,
        headers={
            "Authorization": f"Bearer {api_key}",
            "Content-Type": "application/json",
        },
        method="POST",
    )

    try:
        with urllib.request.urlopen(request, timeout=120) as response:
            result = json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        detail = exc.read().decode("utf-8", errors="replace")
        raise RuntimeError(f"AI API HTTP {exc.code}: {detail[:2000]}")
    except urllib.error.URLError as exc:
        raise RuntimeError(f"AI API connection failed: {exc}")

    try:
        content = result["choices"][0]["message"]["content"]
    except (KeyError, IndexError, TypeError):
        raise RuntimeError("Unexpected AI API response format.")

    if not isinstance(content, str):
        raise RuntimeError("AI response content is not text.")

    return content.strip()


def validate_patch(patch: str) -> None:
    if not patch or len(patch.encode("utf-8")) > MAX_PATCH_BYTES:
        raise RuntimeError("Patch is empty or exceeds the size limit.")

    if "```" in patch:
        raise RuntimeError("Response contains Markdown fences, not a raw diff.")

    if not patch.startswith("--- ") and not patch.startswith("diff --git "):
        raise RuntimeError("AI response is not a unified diff.")

    if re.search(r"(?m)^\+\+\+\s+/|(?m)^---\s+/", patch):
        raise RuntimeError("Absolute paths are forbidden in patches.")

    forbidden = (".git/", "build/", "JUCE/", ".env", "secrets")
    for line in patch.splitlines():
        if line.startswith(("+++ ", "--- ")):
            path = line[4:].strip().split("\t", 1)[0]
            path = path.removeprefix("a/").removeprefix("b/")
            if path == "/dev/null":
                continue
            if path.startswith(forbidden):
                raise RuntimeError(f"Patch touches protected path: {path}")

    if re.search(r"(?m)^deleted file mode ", patch):
        raise RuntimeError("File deletion is not allowed by this repair tool.")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate and optionally apply a guarded AI patch."
    )
    parser.add_argument("--files", nargs="+", required=True)
    parser.add_argument("--log", type=Path, required=True)
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--output", default="reports/ai-repair.patch")
    args = parser.parse_args()

    try:
        check_git_safety()

        api_key = os.getenv("NEBULA_AI_API_KEY", "")
        base_url = os.getenv(
            "NEBULA_AI_BASE_URL",
            "https://api.openai.com/v1"
        )
        model = os.getenv("NEBULA_AI_MODEL", "")

        if not api_key or not model:
            raise RuntimeError(
                "Set NEBULA_AI_API_KEY and NEBULA_AI_MODEL."
            )

        log_path = args.log.resolve()
        if not log_path.is_file():
            raise RuntimeError(f"Log file not found: {log_path}")

        if log_path.stat().st_size > MAX_LOG_BYTES:
            raise RuntimeError("Error log exceeds the size limit.")

        log_text = log_path.read_text(
            encoding="utf-8",
            errors="replace"
        )

        file_sections = []
        for relative in args.files:
            content = read_file(relative)
            file_sections.append(
                f"\n===== FILE: {relative} =====\n{content}\n"
            )

        prompt = (
            "Diagnose the supplied build or test failure. "
            "Prepare the smallest compatible patch. Preserve existing "
            "behavior. Do not add placeholder implementations. "
            "Return a unified diff only.\n\n"
            "===== ERROR LOG =====\n"
            f"{log_text}\n"
            + "\n".join(file_sections)
        )

        patch = request_patch(base_url, api_key, model, prompt)
        validate_patch(patch)

        output = (ROOT / args.output).resolve()
        output.relative_to(ROOT.resolve())
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(patch + "\n", encoding="utf-8")

        print(f"[REPAIR] Patch saved: {output.relative_to(ROOT)}")

        check = git("apply", "--check", str(output))
        if check.returncode != 0:
            raise RuntimeError(
                "Git rejected the patch:\n" + check.stderr
            )

        print("[REPAIR] Git patch validation passed.")

        if not args.apply:
            print("[REPAIR] Review the patch. Nothing was modified.")
            print("Apply only after review with the --apply option.")
            return 0

        confirmation = input(
            "Apply this AI-generated patch? Type APPLY: "
        ).strip()

        if confirmation != "APPLY":
            print("[REPAIR] Cancelled.")
            return 0

        applied = git("apply", str(output))
        if applied.returncode != 0:
            raise RuntimeError(applied.stderr)

        print("[REPAIR] Patch applied. Build and tests are still required.")
        return 0

    except Exception as exc:
        print(f"[REPAIR ERROR] {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
