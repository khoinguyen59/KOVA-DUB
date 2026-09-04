#!/usr/bin/env python3
"""Regression scan for accidental LA Studio repository downloads in notebooks."""

from __future__ import annotations

import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
NOTEBOOK_ROOT = ROOT / "notebooks"
PERSONAL_REPOSITORY_MARKERS = (
    "github.com/khoinguyen59/kova-voice-studio",
    "github.com/khoinguyen59/KOVA-DUB",
    "raw.githubusercontent.com/khoinguyen59",
)


class NotebookRepositoryDependencyTests(unittest.TestCase):
    def test_no_runtime_notebook_downloads_the_application_repository(self):
        offenders: list[str] = []
        for path in sorted(NOTEBOOK_ROOT.rglob("*.ipynb")):
            document = json.loads(path.read_text(encoding="utf-8"))
            for cell in document.get("cells", []):
                if cell.get("cell_type") != "code":
                    continue
                source = cell.get("source", [])
                source = "".join(source) if isinstance(source, list) else str(source)
                # Unified Dubbing embeds exact notebook JSON as a data literal.
                # Those URLs are inert payloads and are validated by its own
                # materialization verifier; only executable cells are scanned.
                if path.name == "LA_STUDIO_UNIFIED_DUBBING_GPU.ipynb" and "EMBEDDED_UNIFIED_FILES" in source:
                    continue
                lowered = source.lower()
                if any(marker.lower() in lowered for marker in PERSONAL_REPOSITORY_MARKERS):
                    offenders.append(str(path.relative_to(ROOT)))
        self.assertEqual(offenders, [], "notebooks download an application-owned repository: " + ", ".join(offenders))


if __name__ == "__main__":
    unittest.main(verbosity=2)
