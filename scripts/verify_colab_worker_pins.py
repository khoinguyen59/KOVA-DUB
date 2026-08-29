#!/usr/bin/env python3
"""Verify the self-contained Colab worker bundle before build or push.

The Spleeter notebook must contain the exact application-owned worker and
launcher sources generated from the local templates. This check deliberately
does not fetch the project from GitHub: the model comes from the official
upstream Spleeter release, and no LA Studio application code is downloaded at
runtime.
"""

from __future__ import annotations

import argparse
import ast
import hashlib
import importlib.util
import json
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from types import ModuleType


ROOT = Path(__file__).resolve().parents[1]
GENERATOR = ROOT / "scripts" / "generate_spleeter_safe_colab_notebook.py"
NOTEBOOK = ROOT / "notebooks" / "voice_separation" / "LA_STUDIO_SEPARATION_SPLEETER_2STEMS_GPU.ipynb"
PERSONAL_WORKER_REPOSITORY = "raw.githubusercontent.com/khoinguyen59/KOVA-DUB"


@dataclass(frozen=True)
class EmbeddedWorker:
    destination: str
    relative_path: str
    expected_sha256: str


@dataclass(frozen=True)
class BundleValidationResult:
    destination: str
    ok: bool
    message: str
    actual_sha256: str = ""


def normalize_text_bytes(payload: bytes) -> bytes:
    return payload.replace(b"\r\n", b"\n").replace(b"\r", b"\n")


def local_sha256(path: Path) -> str:
    """Hash a checked-in text worker independent of Git's Windows EOL mode."""
    payload = path.read_bytes()
    if path.suffix.lower() in {".py", ".txt", ".md"}:
        payload = normalize_text_bytes(payload)
    return hashlib.sha256(payload).hexdigest()


def load_generator(path: Path = GENERATOR) -> ModuleType:
    spec = importlib.util.spec_from_file_location("lastudio_spleeter_generator", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Cannot import worker generator: {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    if not hasattr(module, "WORKERS"):
        raise RuntimeError(f"Worker generator is missing WORKERS: {path}")
    return module


def load_workers(path: Path = GENERATOR) -> list[EmbeddedWorker]:
    generator = load_generator(path)
    project_root = path.resolve().parents[1]
    workers: list[EmbeddedWorker] = []
    for destination, relative_path in generator.WORKERS.items():
        normalized_path = str(relative_path).replace("\\", "/")
        source_path = project_root / Path(normalized_path)
        if not source_path.is_file():
            expected_sha256 = ""
        else:
            expected_sha256 = local_sha256(source_path)
        workers.append(EmbeddedWorker(str(destination), normalized_path, expected_sha256))
    return workers


def _code_source(document: dict) -> str:
    return "\n".join(
        "".join(cell.get("source", []))
        for cell in document.get("cells", [])
        if cell.get("cell_type") == "code"
    )


def embedded_sources_from_notebook(path: Path) -> dict[str, tuple[str, str]]:
    """Extract the statically assigned EMBEDDED_WORKERS mapping from a notebook."""
    document = json.loads(path.read_text(encoding="utf-8"))
    for cell in document.get("cells", []):
        if cell.get("cell_type") != "code":
            continue
        source = "".join(cell.get("source", []))
        if "EMBEDDED_WORKERS" not in source:
            continue
        tree = ast.parse(source, filename=str(path))
        for node in ast.walk(tree):
            if not isinstance(node, ast.Assign):
                continue
            if not any(isinstance(target, ast.Name) and target.id == "EMBEDDED_WORKERS" for target in node.targets):
                continue
            value = ast.literal_eval(node.value)
            if not isinstance(value, dict):
                raise ValueError("EMBEDDED_WORKERS is not a dictionary")
            result: dict[str, tuple[str, str]] = {}
            for destination, payload in value.items():
                if not isinstance(destination, str) or not isinstance(payload, tuple) or len(payload) != 2:
                    raise ValueError("EMBEDDED_WORKERS has an invalid entry")
                checksum, worker_source = payload
                if not isinstance(checksum, str) or not isinstance(worker_source, str):
                    raise ValueError("EMBEDDED_WORKERS has an invalid checksum or source")
                result[destination] = (checksum, worker_source)
            return result
    raise ValueError("generated notebook has no static EMBEDDED_WORKERS mapping")


def notebook_matches_generator(
    path: Path,
    workers: list[EmbeddedWorker],
    project_root: Path = ROOT,
) -> list[str]:
    embedding_tree: ast.AST | None = None
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
        source = _code_source(document)
        embedded = embedded_sources_from_notebook(path)
        for cell in document.get("cells", []):
            cell_source = "".join(cell.get("source", []))
            if cell.get("cell_type") == "code" and "EMBEDDED_WORKERS" in cell_source:
                embedding_tree = ast.parse(cell_source, filename=str(path))
                break
    except (OSError, json.JSONDecodeError, SyntaxError, ValueError) as error:
        return [f"cannot read embedded worker bundle from {path}: {error}"]

    issues: list[str] = []
    if PERSONAL_WORKER_REPOSITORY in source:
        issues.append("generated notebook references the personal GitHub worker repository")
    if "WORKER_REPOSITORY" in source or "WORKER_COMMIT" in source:
        issues.append("generated notebook still contains the obsolete remote worker pin protocol")
    if embedding_tree is not None and any(
        (isinstance(node, ast.Name) and node.id == "urlopen")
        or (isinstance(node, ast.Attribute) and node.attr == "urlopen")
        for node in ast.walk(embedding_tree)
    ):
        issues.append("generated notebook still downloads worker source at runtime")
    for cell in document.get("cells", []):
        cell_source = "".join(cell.get("source", []))
        if cell.get("cell_type") == "code" and "EMBEDDED_WORKERS" not in cell_source:
            if "urlopen" in cell_source or "raw.githubusercontent.com" in cell_source:
                issues.append("generated notebook contains a runtime source download outside the embedded bundle")
                break
    metadata = document.get("metadata", {}).get("la_studio", {})
    if metadata.get("worker_source") != "embedded-local":
        issues.append("notebook metadata does not declare worker_source=embedded-local")
    metadata_hashes = metadata.get("embedded_worker_sha256", {})
    if not isinstance(metadata_hashes, dict):
        issues.append("notebook metadata has no embedded_worker_sha256 map")
    if "!python /content/la_studio_separation_launcher.py" not in source:
        issues.append("generated notebook does not launch the embedded launcher")

    expected_destinations = {worker.destination for worker in workers}
    if set(embedded) != expected_destinations:
        issues.append(
            "embedded worker destinations differ from generator: "
            f"expected {sorted(expected_destinations)}, got {sorted(embedded)}"
        )
    for worker in workers:
        payload = embedded.get(worker.destination)
        if payload is None:
            continue
        embedded_sha256, worker_source = payload
        actual_sha256 = hashlib.sha256(worker_source.encode("utf-8")).hexdigest()
        if not worker.expected_sha256:
            issues.append(f"local worker is missing: {worker.relative_path}")
            continue
        if actual_sha256 != worker.expected_sha256:
            issues.append(
                f"embedded SHA-256 mismatch for {worker.destination}: "
                f"expected {worker.expected_sha256}, got {actual_sha256}"
            )
        if embedded_sha256 != actual_sha256:
            issues.append(
                f"embedded SHA-256 metadata mismatch for {worker.destination}: "
                f"declared {embedded_sha256}, got {actual_sha256}"
            )
        if metadata_hashes.get(worker.destination) != worker.expected_sha256:
            issues.append(
                f"notebook metadata SHA-256 mismatch for {worker.destination}: "
                f"expected {worker.expected_sha256}, got {metadata_hashes.get(worker.destination, '')}"
            )
        local_source = normalize_text_bytes(
            (project_root / Path(worker.relative_path)).read_bytes()
        ).decode("utf-8")
        if worker_source != local_source:
            issues.append(f"embedded source differs from local template: {worker.relative_path}")
        if f"Path('/content', destination).write_text" not in source:
            issues.append("generated notebook does not write embedded sources into /content")
            break
    return issues


def validate_project(root: Path = ROOT) -> tuple[list[BundleValidationResult], list[str]]:
    generator = root / "scripts" / "generate_spleeter_safe_colab_notebook.py"
    notebook = root / "notebooks" / "voice_separation" / NOTEBOOK.name
    workers = load_workers(generator)
    issues = notebook_matches_generator(notebook, workers, root)
    results: list[BundleValidationResult] = []
    for worker in workers:
        local_path = root / Path(worker.relative_path)
        if not local_path.is_file():
            results.append(BundleValidationResult(worker.destination, False, "local worker is missing"))
            continue
        actual = local_sha256(local_path)
        ok = actual == worker.expected_sha256
        results.append(
            BundleValidationResult(
                worker.destination,
                ok,
                "local source hash matches generator" if ok else "local source hash mismatch",
                actual,
            )
        )
        if not ok:
            issues.append(
                f"local SHA-256 mismatch for {worker.relative_path}: "
                f"expected {worker.expected_sha256}, got {actual}"
            )
    artifact_source = notebook.read_text(encoding="utf-8")
    if "https://github.com/k2-fsa/sherpa-onnx/releases/download/" not in artifact_source:
        issues.append("notebook does not use the official k2-fsa model release")
    return results, issues


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--report", type=Path)
    # Kept as accepted no-op compatibility flags for existing local scripts.
    parser.add_argument("--timeout", type=float, default=60.0, help=argparse.SUPPRESS)
    parser.add_argument("--attempts", type=int, default=1, help=argparse.SUPPRESS)
    args = parser.parse_args(argv)

    root = args.root.resolve()
    results, issues = validate_project(root)
    report = {
        "schemaVersion": 2,
        "status": "PASS" if not issues else "FAIL",
        "repositoryRoot": str(root),
        "checks": [asdict(result) for result in results],
        "issues": issues,
    }
    report_path = args.report or root / "out" / "prebuild-gate" / "colab-worker-pins.json"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2), encoding="utf-8")
    if issues:
        print("Embedded Colab worker verification failed:", file=sys.stderr)
        for issue in issues:
            print(f"- {issue}", file=sys.stderr)
        print(f"Evidence: {report_path}", file=sys.stderr)
        return 1
    print(f"Embedded Colab workers verified: {len(results)}/{len(results)} local payloads match.")
    print(f"Evidence: {report_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
