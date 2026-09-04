#!/usr/bin/env python3
"""Verify the legacy voice-clone notebook has no application-repository dependency."""

from __future__ import annotations

import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CANONICAL = ROOT / "notebooks" / "voice_cloning" / "LA_STUDIO_VOICE_CLONE_OMNIVOICE_GPU.ipynb"
LEGACY = ROOT / "notebooks" / "voice_cloning" / "LA_STUDIO_VOICE_CLONE_GPU.ipynb"


def code_cells(document: dict) -> list[dict]:
    return [cell for cell in document.get("cells", []) if cell.get("cell_type") == "code"]


def source(cell: dict) -> str:
    value = cell.get("source", [])
    return "".join(value) if isinstance(value, list) else str(value)


def main() -> int:
    canonical = json.loads(CANONICAL.read_text(encoding="utf-8"))
    legacy = json.loads(LEGACY.read_text(encoding="utf-8"))
    canonical_code = [source(cell) for cell in code_cells(canonical)]
    legacy_code = [source(cell) for cell in code_cells(legacy)]
    if legacy_code != canonical_code:
        raise AssertionError(
            "Legacy voice-clone notebook drifted from the exact OmniVoice implementation; "
            "run scripts/generate_legacy_voice_clone_compat_notebook.py"
        )
    metadata = legacy.get("metadata", {}).get("la_studio", {})
    if metadata.get("legacy_alias_for") != CANONICAL.name or metadata.get("compatibility_alias") is not True:
        raise AssertionError("Legacy voice-clone notebook is missing its compatibility alias metadata")
    runtime_source = "\n".join(legacy_code)
    for forbidden in (
        "kova-voice-studio",
        "KOVA-DUB",
        "raw.githubusercontent.com/khoinguyen59",
        "REPO_URL",
        "REPO_REF",
    ):
        if forbidden in runtime_source:
            raise AssertionError(f"Legacy voice-clone notebook has an application repository dependency: {forbidden}")
    print("Legacy Voice Clone compatibility notebook: PASS")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, json.JSONDecodeError, AssertionError) as error:
        print(f"Legacy Voice Clone compatibility notebook: FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
