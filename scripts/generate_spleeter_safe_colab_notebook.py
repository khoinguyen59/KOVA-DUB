#!/usr/bin/env python3
"""Generate the self-contained Spleeter Direct-Colab notebook.

The official Spleeter model remains downloaded from the upstream k2-fsa
release.  LA Studio's worker and launcher are application code, so they are
embedded from the checked-in local templates instead of being fetched from a
personal GitHub repository at notebook runtime.
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
from textwrap import dedent


ROOT = Path(__file__).resolve().parents[1]
NOTEBOOKS = ROOT / "notebooks" / "voice_separation"
NOTEBOOK = "LA_STUDIO_SEPARATION_SPLEETER_2STEMS_GPU.ipynb"
MODEL_ID = "sherpa-onnx-spleeter-2stems-fp16"
UPSTREAM_MODEL = "k2-fsa/sherpa-onnx-spleeter-2stems-fp16"
ARTIFACT_URL = (
    "https://github.com/k2-fsa/sherpa-onnx/releases/download/"
    "source-separation-models/sherpa-onnx-spleeter-2stems-fp16.tar.bz2"
)

# These are application-owned sources.  The generated notebook carries their
# exact normalized text, so a Colab runtime has no dependency on this project's
# GitHub repository or on a moving branch/commit.
WORKERS = {
    "la_studio_separation_worker.py": Path(
        "notebooks/workers/LA_STUDIO_SEPARATION_SPLEETER_2STEMS_WORKER.py"
    ),
    "la_studio_separation_launcher.py": Path(
        "notebooks/workers/LA_STUDIO_SEPARATION_SPLEETER_2STEMS_LAUNCHER.py"
    ),
}


def normalize_text_bytes(payload: bytes) -> bytes:
    return payload.replace(b"\r\n", b"\n").replace(b"\r", b"\n")


def worker_payloads() -> dict[str, tuple[str, str]]:
    payloads: dict[str, tuple[str, str]] = {}
    for destination, relative_path in WORKERS.items():
        path = ROOT / relative_path
        source = normalize_text_bytes(path.read_bytes()).decode("utf-8")
        digest = hashlib.sha256(source.encode("utf-8")).hexdigest()
        payloads[destination] = (digest, source)
    return payloads


def source_lines(source: str) -> list[str]:
    return (dedent(source).strip() + "\n").splitlines(keepends=True)


def make_notebook() -> dict:
    embedded = worker_payloads()
    worker_rows = ",\n".join(
        f"    {destination!r}: ({checksum!r}, {source!r})"
        for destination, (checksum, source) in embedded.items()
    )
    worker_embedding = "\n".join((
        "from hashlib import sha256",
        "from pathlib import Path",
        "",
        "EMBEDDED_WORKERS = {",
        worker_rows,
        "}",
        "for destination, (expected_sha256, source) in EMBEDDED_WORKERS.items():",
        "    actual_sha256 = sha256(source.encode('utf-8')).hexdigest()",
        "    if actual_sha256 != expected_sha256:",
        "        raise RuntimeError(f'Embedded worker integrity check failed for {destination}: {actual_sha256}')",
        "    Path('/content', destination).write_text(source, encoding='utf-8')",
        "print('Embedded verified exact-model CUDA worker and launcher templates.')",
    ))
    return {
        "cells": [
            {
                "cell_type": "markdown",
                "metadata": {},
                "source": source_lines(
                    f'''
                    # LA Studio voice-isolation - Spleeter 2-stem FP16

                    This notebook runs exactly `{MODEL_ID}` from the declared k2-fsa artifact on the temporary **Colab GPU worker**. The LA Studio worker and launcher are embedded in this notebook; no LA Studio GitHub repository or repository token is required at runtime.

                    The worker performs a CUDA startup probe before it prints a URL. It also sends long audio as bounded, overlapping segments, so the Spleeter FP16 CUDA convolution plan remains within the verified shape.

                    1. Choose **Runtime -> Change runtime type -> GPU**.
                    2. Run all cells. The final cell must print `startup probe: passed`.
                    3. Copy the printed URL and token to Dubbing -> Colab setup, then press **Check Colab**.
                    '''
                ),
            },
            {
                "cell_type": "code",
                "execution_count": None,
                "metadata": {},
                "outputs": [],
                "source": source_lines(
                    f'''
                    !nvidia-smi
                    %pip install -q --upgrade --no-cache-dir "onnxruntime-gpu==1.21.0" "kaldi-native-fbank" "soundfile==0.13.1" "fastapi==0.115.12" "uvicorn==0.34.3" "python-multipart==0.0.20"

                    import torch
                    if not torch.cuda.is_available():
                        raise RuntimeError('No Colab CUDA GPU is available. Select Runtime > Change runtime type > GPU, then restart and Run all.')
                    print('Colab CUDA:', torch.cuda.get_device_name(0))

                    !wget -q --show-progress -O /content/spleeter.tar.bz2 {ARTIFACT_URL}
                    !tar -xjf /content/spleeter.tar.bz2 -C /content
                    '''
                ),
            },
            {
                "cell_type": "code",
                "execution_count": None,
                "metadata": {},
                "outputs": [],
                "source": source_lines(worker_embedding),
            },
            {
                "cell_type": "code",
                "execution_count": None,
                "metadata": {},
                "outputs": [],
                "source": source_lines("!python /content/la_studio_separation_launcher.py"),
            },
        ],
        "metadata": {
            "accelerator": "GPU",
            "colab": {"gpuType": "T4", "provenance": []},
            "kernelspec": {"display_name": "Python 3", "name": "python3"},
            "language_info": {"name": "python"},
            "la_studio": {
                "capability": "voice-isolation",
                "family_id": MODEL_ID,
                "upstream_model": UPSTREAM_MODEL,
                "contract_version": 1,
                "device": "cuda",
                "cpu_fallback": False,
                "worker_contract": "spleeter-cuda-safe-20260816.1",
                "worker_source": "embedded-local",
                # Keep project-relative provenance paths explicit; runtime
                # still uses only the embedded payload.
                "worker_templates": [
                    "notebooks/workers/LA_STUDIO_SEPARATION_SPLEETER_2STEMS_WORKER.py",
                    "notebooks/workers/LA_STUDIO_SEPARATION_SPLEETER_2STEMS_LAUNCHER.py",
                ],
                "embedded_worker_sha256": {
                    destination: checksum for destination, (checksum, _) in embedded.items()
                },
                "artifact_url": ARTIFACT_URL,
            },
        },
        "nbformat": 4,
        "nbformat_minor": 5,
    }


def main() -> None:
    NOTEBOOKS.mkdir(parents=True, exist_ok=True)
    target = NOTEBOOKS / NOTEBOOK
    target.write_text(
        json.dumps(make_notebook(), indent=1, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    print(target.relative_to(ROOT))


if __name__ == "__main__":
    main()
