#!/usr/bin/env python3
"""Generate the one-tunnel optional Unified Dubbing Colab coordinator notebook."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
from textwrap import dedent


ROOT = Path(__file__).resolve().parents[1]
NOTEBOOK = ROOT / "notebooks" / "pipelines" / "LA_STUDIO_UNIFIED_DUBBING_GPU.ipynb"
COORDINATOR = ROOT / "notebooks" / "workers" / "LA_STUDIO_UNIFIED_DUBBING_COORDINATOR.py"


def embedded_unified_files() -> dict[str, str]:
    """Return the exact local inputs required by the unified coordinator.

    The coordinator starts workers by reading their generated notebooks. Keep
    those notebooks in the bundle so the Colab runtime does not need a clone
    of the desktop application's repository. Only notebooks with an exact
    capability/model identity are included; generic pipeline notebooks are not
    worker inputs.
    """
    files: dict[str, str] = {}
    for path in sorted((ROOT / "notebooks").rglob("*.ipynb")):
        if path == NOTEBOOK:
            continue
        document = json.loads(path.read_text(encoding="utf-8"))
        metadata = document.get("metadata", {}).get("la_studio", {})
        if metadata.get("compatibility_alias") is True:
            continue
        capability = metadata.get("capability")
        model = metadata.get("family_id") or metadata.get("model_id")
        if isinstance(capability, str) and capability.strip() and isinstance(model, str) and model.strip():
            relative_path = path.relative_to(ROOT).as_posix()
            files[relative_path] = path.read_text(encoding="utf-8")
    files[COORDINATOR.relative_to(ROOT).as_posix()] = COORDINATOR.read_text(encoding="utf-8")
    spleeter_worker = ROOT / "notebooks" / "workers" / "LA_STUDIO_SEPARATION_SPLEETER_2STEMS_WORKER.py"
    files[spleeter_worker.relative_to(ROOT).as_posix()] = spleeter_worker.read_text(encoding="utf-8")
    return files


def source_lines(source: str) -> list[str]:
    return (dedent(source).strip() + "\n").splitlines(keepends=True)


def make_notebook() -> dict:
    embedded_files = embedded_unified_files()
    embedded_rows = "\n".join(
        f"    {relative_path!r}: {source!r},"
        for relative_path, source in embedded_files.items()
    )
    bundle_source = "\n".join([
        "import shutil",
        "from pathlib import Path",
        "",
        "EMBEDDED_UNIFIED_FILES = {",
        embedded_rows,
        "}",
        "SOURCE_ROOT = Path('/content/la-studio-unified-source')",
        "shutil.rmtree(SOURCE_ROOT, ignore_errors=True)",
        "for relative_path, payload in EMBEDDED_UNIFIED_FILES.items():",
        "    target = SOURCE_ROOT / relative_path",
        "    target.parent.mkdir(parents=True, exist_ok=True)",
        "    target.write_text(payload, encoding='utf-8')",
        "COORDINATOR_PATH = Path('/content/la_studio_unified_dubbing_coordinator.py')",
        "COORDINATOR_PATH.write_text(",
        "    EMBEDDED_UNIFIED_FILES['notebooks/workers/LA_STUDIO_UNIFIED_DUBBING_COORDINATOR.py'],",
        "    encoding='utf-8',",
        ")",
        "print(f'Wrote {len(EMBEDDED_UNIFIED_FILES)} embedded coordinator inputs.')",
    ])
    default_workers = [
        {"capability": "voice-isolation", "model": "sherpa-onnx-spleeter-2stems-fp16"},
        {"capability": "stt", "model": "whisper.cpp"},
        {"capability": "subtitle-ocr", "model": "pp-ocrv5-multilingual-3.1"},
        {"capability": "translation", "model": "m2m100-418m"},
        {"capability": "tts", "model": "kokoro"},
        {"capability": "forced-alignment", "model": "mms-forced-aligner-onnx"},
    ]
    return {
        "cells": [
            {"cell_type": "markdown", "metadata": {}, "source": source_lines("""
                # LA Studio Unified Dubbing Coordinator (optional)

                This is a real one-URL, one-token coordinator for the selected Dubbing models. It starts each selected **exact CUDA worker** privately, verifies its ordinary `/health`, then exposes it through one Cloudflare tunnel. It never substitutes local CPU or API Gateway routes. The coordinator and exact worker notebooks are embedded in this notebook; no LA Studio repository clone or repository token is required at runtime.

                Keep the normal per-model notebooks if you prefer them. This notebook is optional and does not replace those routes.
            """)},
            {"cell_type": "code", "execution_count": None, "metadata": {}, "outputs": [], "source": source_lines(f"""
                import subprocess
                import sys
                from pathlib import Path

                subprocess.run([sys.executable, '-m', 'pip', 'install', '--quiet', '--upgrade', 'fastapi==0.115.12', 'uvicorn==0.34.3', 'httpx==0.28.1'], check=True)
                print('Installed the unified coordinator runtime; application sources are embedded below.')
            """)},
            {"cell_type": "code", "execution_count": None, "metadata": {}, "outputs": [], "source": source_lines(f"""
                import json
                from pathlib import Path

                # Keep only the exact models you intend to select in LA Studio.
                # Every generated exact notebook is embedded below, so this list
                # can be edited without cloning the desktop repository.
                UNIFIED_WORKERS = {json.dumps(default_workers, indent=4)}
                CONFIG_PATH = Path('/content/la_studio_unified_workers.json')
                CONFIG_PATH.write_text(json.dumps(UNIFIED_WORKERS, indent=2), encoding='utf-8')
                print('Will prewarm:', ', '.join(f"{{row['capability']}}/{{row['model']}}" for row in UNIFIED_WORKERS))
            """)},
            {"cell_type": "code", "execution_count": None, "metadata": {}, "outputs": [], "source": source_lines(bundle_source)},
            {"cell_type": "code", "execution_count": None, "metadata": {}, "outputs": [], "source": source_lines("""
                # This cell remains running while LA Studio uses the unified worker.
                # It prints one URL and one token only after every selected exact CUDA worker is healthy.
                !python3 /content/la_studio_unified_dubbing_coordinator.py \\
                    --source-root /content/la-studio-unified-source \\
                    --config /content/la_studio_unified_workers.json
            """)},
        ],
        "metadata": {
            "kernelspec": {"display_name": "Python 3", "language": "python", "name": "python3"},
            "language_info": {"name": "python", "version": "3.x"},
            "la_studio": {
                "role": "unified-dubbing-coordinator",
                "contract_version": 1,
                "route_template": "/v1/unified/<capability>/<model>/<worker-route>",
                "worker_source": "embedded-local",
                "embedded_files": sorted(embedded_files),
                "embedded_file_sha256": {
                    relative_path: hashlib.sha256(source.encode("utf-8")).hexdigest()
                    for relative_path, source in embedded_files.items()
                },
            },
        },
        "nbformat": 4,
        "nbformat_minor": 5,
    }


def main() -> None:
    NOTEBOOK.parent.mkdir(parents=True, exist_ok=True)
    NOTEBOOK.write_text(json.dumps(make_notebook(), ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(NOTEBOOK)


if __name__ == "__main__":
    main()
