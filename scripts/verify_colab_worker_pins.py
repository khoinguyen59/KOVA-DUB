#!/usr/bin/env python3
"""Verify immutable Colab worker references before a build or push.

The generated notebooks deliberately download worker templates from an
immutable Git commit.  This verifier checks the complete chain: the commit
exists, every raw URL is reachable, the remote bytes match the locked digest,
the checked-in worker matches the same digest, and the generated notebook is
in sync with its generator.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import re
import sys
import time
from dataclasses import asdict, dataclass
from pathlib import Path
from types import ModuleType
from typing import Callable
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


ROOT = Path(__file__).resolve().parents[1]
GENERATOR = ROOT / "scripts" / "generate_spleeter_safe_colab_notebook.py"
NOTEBOOK = ROOT / "notebooks" / "voice_separation" / "LA_STUDIO_SEPARATION_SPLEETER_2STEMS_GPU.ipynb"
RETRYABLE_HTTP_CODES = frozenset({408, 425, 429, 500, 502, 503, 504})
SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")
COMMIT_PATTERN = re.compile(r"^[0-9a-f]{40}$")
FetchPayload = Callable[[str, float], bytes]
Sleep = Callable[[float], None]


@dataclass(frozen=True)
class WorkerPin:
    destination: str
    repository: str
    commit: str
    relative_path: str
    expected_sha256: str


@dataclass(frozen=True)
class PinValidationResult:
    destination: str
    url: str
    ok: bool
    message: str
    actual_sha256: str = ""


def worker_url(pin: WorkerPin) -> str:
    return (
        f"https://raw.githubusercontent.com/{pin.repository}/"
        f"{pin.commit}/{pin.relative_path}"
    )


def validate_pin(
    pin: WorkerPin,
    *,
    fetcher: FetchPayload,
    sleep: Sleep = time.sleep,
    attempts: int = 3,
    timeout: float = 60.0,
) -> PinValidationResult:
    """Fetch and validate one worker pin with bounded transient retries."""

    url = worker_url(pin)
    if not COMMIT_PATTERN.fullmatch(pin.commit):
        return PinValidationResult(pin.destination, url, False, "commit is not a 40-character SHA-1")
    if not SHA256_PATTERN.fullmatch(pin.expected_sha256):
        return PinValidationResult(pin.destination, url, False, "expected SHA-256 is not 64 lowercase hex characters")
    if not pin.repository or not pin.relative_path or pin.relative_path.startswith("/"):
        return PinValidationResult(pin.destination, url, False, "repository or worker path is invalid")
    if ".." in Path(pin.relative_path).parts:
        return PinValidationResult(pin.destination, url, False, "worker path must not escape its repository")

    last_error = ""
    for attempt in range(1, max(1, attempts) + 1):
        try:
            payload = fetcher(url, timeout)
            actual = hashlib.sha256(payload).hexdigest()
            if actual != pin.expected_sha256:
                return PinValidationResult(
                    pin.destination,
                    url,
                    False,
                    f"SHA-256 mismatch: expected {pin.expected_sha256}, got {actual}",
                    actual,
                )
            return PinValidationResult(pin.destination, url, True, "remote payload and SHA-256 match", actual)
        except HTTPError as error:
            last_error = f"HTTP {error.code} {error.reason}"
            if error.code not in RETRYABLE_HTTP_CODES or attempt >= max(1, attempts):
                break
        except (URLError, TimeoutError, OSError) as error:
            last_error = f"network error: {error}"
            if attempt >= max(1, attempts):
                break
        sleep(float(attempt))
    return PinValidationResult(pin.destination, url, False, last_error or "remote fetch failed")


def fetch_remote(url: str, timeout: float) -> bytes:
    request = Request(url, headers={"User-Agent": "LA-Studio-Colab-worker-pin-check"})
    with urlopen(request, timeout=timeout) as response:
        if response.status != 200:
            raise RuntimeError(f"HTTP {response.status} {response.reason}")
        return response.read()


def local_sha256(path: Path) -> str:
    """Hash a checked-in text worker independent of Git's Windows EOL mode."""
    payload = path.read_bytes()
    if path.suffix.lower() in {".py", ".txt", ".md"}:
        payload = payload.replace(b"\r\n", b"\n").replace(b"\r", b"\n")
    return hashlib.sha256(payload).hexdigest()


def load_generator(path: Path = GENERATOR) -> ModuleType:
    spec = importlib.util.spec_from_file_location("lastudio_spleeter_generator", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Cannot import worker generator: {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    for name in ("WORKER_REPOSITORY", "WORKER_COMMIT", "WORKERS"):
        if not hasattr(module, name):
            raise RuntimeError(f"Worker generator is missing {name}: {path}")
    return module


def load_pins(path: Path = GENERATOR) -> list[WorkerPin]:
    generator = load_generator(path)
    repository = str(generator.WORKER_REPOSITORY).strip()
    commit = str(generator.WORKER_COMMIT).strip().lower()
    return [
        WorkerPin(
            destination=str(destination),
            repository=repository,
            commit=commit,
            relative_path=str(relative_path),
            expected_sha256=str(checksum).strip().lower(),
        )
        for destination, (relative_path, checksum) in generator.WORKERS.items()
    ]


def notebook_matches_generator(path: Path, pins: list[WorkerPin]) -> list[str]:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        return [f"cannot read generated notebook {path}: {error}"]
    source = "\n".join(
        "".join(cell.get("source", []))
        for cell in document.get("cells", [])
        if cell.get("cell_type") == "code"
    )
    issues: list[str] = []
    if not re.search(r'WORKER_REPOSITORY\s*=\s*"[^"]+"', source):
        issues.append("generated notebook has no WORKER_REPOSITORY")
    for pin in pins:
        expected_markers = (
            f'WORKER_REPOSITORY = "{pin.repository}"',
            f'WORKER_COMMIT = "{pin.commit}"',
            f'"{pin.relative_path}"',
            f'"{pin.expected_sha256}"',
        )
        for marker in expected_markers:
            if marker not in source:
                issues.append(f"generated notebook is stale or missing marker: {marker}")
        if "https://raw.githubusercontent.com/{WORKER_REPOSITORY}/{WORKER_COMMIT}/{relative_path}" not in source:
            issues.append("generated notebook does not build raw URL from repository, commit, and path")
    return issues


def validate_project(
    root: Path = ROOT,
    *,
    fetcher: FetchPayload = fetch_remote,
    sleep: Sleep = time.sleep,
    attempts: int = 3,
    timeout: float = 60.0,
) -> tuple[list[PinValidationResult], list[str]]:
    pins = load_pins(root / "scripts" / "generate_spleeter_safe_colab_notebook.py")
    results: list[PinValidationResult] = []
    issues = notebook_matches_generator(root / "notebooks" / "voice_separation" / NOTEBOOK.name, pins)
    for pin in pins:
        local_path = root / pin.relative_path
        if not local_path.is_file():
            issues.append(f"local worker is missing: {pin.relative_path}")
        else:
            local_sha = local_sha256(local_path)
            if local_sha != pin.expected_sha256:
                issues.append(
                    f"local SHA-256 mismatch for {pin.relative_path}: "
                    f"expected {pin.expected_sha256}, got {local_sha} "
                    "(after normalizing text line endings)"
                )
        results.append(validate_pin(pin, fetcher=fetcher, sleep=sleep, attempts=attempts, timeout=timeout))
    return results, issues


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--timeout", type=float, default=60.0)
    parser.add_argument("--attempts", type=int, default=3)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args(argv)

    root = args.root.resolve()
    results, issues = validate_project(root, attempts=args.attempts, timeout=args.timeout)
    issues.extend(result.message for result in results if not result.ok)
    report = {
        "schemaVersion": 1,
        "status": "PASS" if not issues else "FAIL",
        "repositoryRoot": str(root),
        "checks": [asdict(result) for result in results],
        "issues": issues,
    }
    report_path = args.report or root / "out" / "prebuild-gate" / "colab-worker-pins.json"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    if issues:
        print("Colab worker pin verification failed:", file=sys.stderr)
        for issue in issues:
            print(f"- {issue}", file=sys.stderr)
        print(f"Evidence: {report_path}", file=sys.stderr)
        return 1
    print(f"Colab worker pins verified: {len(results)}/{len(results)} remote payloads and local files match.")
    print(f"Evidence: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
