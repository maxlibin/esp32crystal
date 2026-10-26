"""Crystal's brain: the official Codex CLI signed in with Max's ChatGPT account."""

import json
import subprocess
import sys
from pathlib import Path
from typing import List, Optional

ATTEMPTS = 3
# Codex runs as a pure chat partner: no user config, MCP servers or rules,
# a read-only sandbox, and an empty scratch directory as its workspace.
COMMON_FLAGS = [
    "--json",
    "--skip-git-repo-check",
    "--ignore-user-config",
    "--ignore-rules",
    "-c",
    'sandbox_mode="read-only"',
    "-c",
    'model_reasoning_effort="low"',
]


class CodexError(RuntimeError):
    pass


def codex_path(project_root: Path) -> Path:
    path = project_root / "node_modules" / ".bin" / "codex"
    if not path.exists():
        raise CodexError(f"Codex CLI not found at {path}. Run `npm install` in {project_root}.")
    return path


def require_login(codex: Path) -> None:
    result = subprocess.run([str(codex), "login", "status"], capture_output=True, text=True)
    if result.returncode != 0:
        raise CodexError(
            "Codex is not signed in to ChatGPT. Run `make login` once, then start the chat again. "
            f"(codex login status exit={result.returncode} output={(result.stdout + result.stderr).strip()!r})"
        )


def thread_id_from_events(stdout: str) -> Optional[str]:
    for line in stdout.splitlines():
        try:
            event = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(event, dict) and event.get("type") == "thread.started" and isinstance(event.get("thread_id"), str):
            return event["thread_id"]
    return None


class CodexBrain:
    """One Codex conversation that lives as long as the chat app."""

    def __init__(self, codex: Path, workdir: Path) -> None:
        self._codex = codex
        self._workdir = workdir
        self._reply_file = workdir / "last-reply.txt"
        self._thread_id: Optional[str] = None

    def reply(self, prompt: str) -> str:
        last_error: Optional[CodexError] = None
        for attempt in range(1, ATTEMPTS + 1):
            try:
                return self._run(prompt)
            except CodexError as exc:
                last_error = exc
                if attempt < ATTEMPTS:
                    print(f"warning: codex attempt {attempt}/{ATTEMPTS} failed, retrying: {exc}", file=sys.stderr)
        raise last_error

    def _command(self) -> List[str]:
        output = ["-o", str(self._reply_file)]
        if self._thread_id is None:
            return [str(self._codex), "exec", *COMMON_FLAGS, *output, "-"]
        return [str(self._codex), "exec", "resume", *COMMON_FLAGS, *output, self._thread_id, "-"]

    def _run(self, prompt: str) -> str:
        command = self._command()
        self._reply_file.unlink(missing_ok=True)
        result = subprocess.run(command, input=prompt, cwd=self._workdir, capture_output=True, text=True)
        if result.returncode != 0:
            raise CodexError(
                f"codex exited with {result.returncode}: command={command} "
                f"stderr={result.stderr.strip()[-800:]!r} stdout={result.stdout.strip()[-400:]!r}"
            )
        if self._thread_id is None:
            self._thread_id = thread_id_from_events(result.stdout)
            if self._thread_id is None:
                raise CodexError(f"codex did not report a thread id: stdout={result.stdout.strip()[-400:]!r}")
        if not self._reply_file.exists() or not self._reply_file.read_text().strip():
            raise CodexError(f"codex finished without a reply: stdout={result.stdout.strip()[-400:]!r}")
        return self._reply_file.read_text().strip()
