#!/usr/bin/env python3
"""Regression tests for the self-contained Unified Dubbing Colab notebook."""

from __future__ import annotations

import importlib.util
import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GENERATOR_PATH = ROOT / "scripts" / "generate_unified_dubbing_colab_notebook.py"
NOTEBOOK_PATH = ROOT / "notebooks" / "pipelines" / "LA_STUDIO_UNIFIED_DUBBING_GPU.ipynb"


def load_generator():
    spec = importlib.util.spec_from_file_location("unified_bundle_generator", GENERATOR_PATH)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Cannot load generator: {GENERATOR_PATH}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class UnifiedDubbingBundleTests(unittest.TestCase):
    def test_generator_does_not_clone_the_application_repository(self):
        source = GENERATOR_PATH.read_text(encoding="utf-8")
        self.assertNotIn("SOURCE_REPOSITORY", source)
        self.assertNotIn("SOURCE_COMMIT", source)
        self.assertNotIn("git clone", source)

    def test_generated_notebook_contains_the_coordinator_and_exact_notebook_bundle(self):
        document = load_generator().make_notebook()
        code_cells = [
            "".join(cell.get("source", []))
            for cell in document.get("cells", [])
            if cell.get("cell_type") == "code"
        ]
        source = "\n".join(code_cells)
        runtime_source = "\n".join(
            cell_source for cell_source in code_cells
            if "EMBEDDED_UNIFIED_FILES" not in cell_source
        )
        self.assertIn("EMBEDDED_UNIFIED_FILES", source)
        self.assertIn("LA_STUDIO_UNIFIED_DUBBING_COORDINATOR.py", source)
        self.assertIn("LA_STUDIO_SEPARATION_SPLEETER_2STEMS_GPU.ipynb", source)
        self.assertIn("SOURCE_ROOT = Path('/content/la-studio-unified-source')", source)
        self.assertIn("target = SOURCE_ROOT / relative_path", source)
        self.assertNotIn("git clone", runtime_source)

    def test_checked_in_notebook_has_no_runtime_repository_clone(self):
        document = json.loads(NOTEBOOK_PATH.read_text(encoding="utf-8"))
        code_cells = [
            "".join(cell.get("source", []))
            for cell in document.get("cells", [])
            if cell.get("cell_type") == "code"
        ]
        source = "\n".join(code_cells)
        runtime_source = "\n".join(
            cell_source for cell_source in code_cells
            if "EMBEDDED_UNIFIED_FILES" not in cell_source
        )
        self.assertIn("EMBEDDED_UNIFIED_FILES", source)
        self.assertIn("SOURCE_ROOT = Path('/content/la-studio-unified-source')", source)
        self.assertIn("target = SOURCE_ROOT / relative_path", source)
        self.assertNotIn("git clone", runtime_source)
        self.assertNotIn("https://github.com/khoinguyen59/KOVA-DUB.git", runtime_source)


if __name__ == "__main__":
    unittest.main(verbosity=2)
