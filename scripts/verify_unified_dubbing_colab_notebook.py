#!/usr/bin/env python3
"""Static contract checks for the optional Unified Dubbing Colab notebook."""

from __future__ import annotations

import importlib.util
import ast
import hashlib
import json
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GENERATOR = ROOT / "scripts" / "generate_unified_dubbing_colab_notebook.py"
NOTEBOOK = ROOT / "notebooks" / "pipelines" / "LA_STUDIO_UNIFIED_DUBBING_GPU.ipynb"
COORDINATOR = ROOT / "notebooks" / "workers" / "LA_STUDIO_UNIFIED_DUBBING_COORDINATOR.py"


def load_generator():
    spec = importlib.util.spec_from_file_location("unified_generator", GENERATOR)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def load_coordinator():
    spec = importlib.util.spec_from_file_location("unified_coordinator", COORDINATOR)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    # dataclasses resolves postponed annotations through sys.modules while the
    # module body is being evaluated.
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def code_sources(notebook: dict) -> list[str]:
    return [
        "".join(cell.get("source", []))
        for cell in notebook.get("cells", [])
        if cell.get("cell_type") == "code"
    ]


def embedded_files_from_notebook(notebook: dict) -> dict[str, str]:
    """Extract the static bundle literal from the generated bundle cell."""
    for source in code_sources(notebook):
        if "EMBEDDED_UNIFIED_FILES" not in source:
            continue
        tree = ast.parse(source)
        for statement in ast.walk(tree):
            if not isinstance(statement, ast.Assign):
                continue
            if not any(isinstance(target, ast.Name) and target.id == "EMBEDDED_UNIFIED_FILES"
                       for target in statement.targets):
                continue
            value = ast.literal_eval(statement.value)
            assert isinstance(value, dict), "Unified embedded bundle must be a dict"
            assert all(isinstance(key, str) and isinstance(payload, str)
                       for key, payload in value.items()), \
                "Unified embedded bundle must map string paths to string payloads"
            return value
    raise AssertionError("Missing static EMBEDDED_UNIFIED_FILES bundle")


def main() -> None:
    module = load_generator()
    expected_document = module.make_notebook()
    actual = NOTEBOOK.read_text(encoding="utf-8")
    notebook = json.loads(actual)
    expected_cells = expected_document.get("cells", [])
    actual_cells = notebook.get("cells", [])
    expected_without_cells = {
        key: value for key, value in expected_document.items() if key != "cells"
    }
    actual_without_cells = {
        key: value for key, value in notebook.items() if key != "cells"
    }
    assert (
        expected_without_cells == actual_without_cells
        and actual_cells[:len(expected_cells)] == expected_cells
        and len(actual_cells) >= len(expected_cells)
    ), "Generated notebook core drift: run generate_unified_dubbing_colab_notebook.py"
    metadata = notebook["metadata"]["la_studio"]
    assert metadata["role"] == "unified-dubbing-coordinator"
    sources = "\n".join(code_sources(notebook))
    for required in (
        "UNIFIED_WORKERS", "LA_STUDIO_UNIFIED_DUBBING_URL=",
        "/v1/unified/{capability}/{model}/{route:path}", "httpx.AsyncClient",
        "wait_for_exact_health", "Cloudflare tunnel", "LA_STUDIO_UNIFIED_DUBBING_TOKEN",
        "EMBEDDED_UNIFIED_FILES", "SOURCE_ROOT = Path('/content/la-studio-unified-source')",
        "target = SOURCE_ROOT / relative_path",
    ):
        assert required in sources, f"Missing unified contract: {required}"
    runtime_sources = "\n".join(
        source for source in code_sources(notebook)
        if "EMBEDDED_UNIFIED_FILES" not in source
    )
    for forbidden in ("SOURCE_REPOSITORY", "SOURCE_COMMIT", "git clone",
                      "https://github.com/khoinguyen59/KOVA-DUB.git"):
        assert forbidden not in runtime_sources, \
            f"Unified notebook still has runtime repository dependency: {forbidden}"

    embedded_files = embedded_files_from_notebook(notebook)
    expected_files = module.embedded_unified_files()
    assert set(embedded_files) == set(expected_files), \
        "Embedded Unified bundle file set differs from local exact notebook sources"
    assert metadata["worker_source"] == "embedded-local"
    assert metadata["embedded_files"] == sorted(expected_files)
    expected_hashes = {
        relative_path: hashlib.sha256(payload.encode("utf-8")).hexdigest()
        for relative_path, payload in expected_files.items()
    }
    assert metadata["embedded_file_sha256"] == expected_hashes
    assert embedded_files == expected_files, \
        "Embedded Unified bundle payload differs from local exact notebook sources"

    coordinator = load_coordinator()
    config_source = next(source for source in code_sources(notebook) if "UNIFIED_WORKERS" in source)
    defaults = json.loads(config_source.split("UNIFIED_WORKERS = ", 1)[1].split(
        "\n                CONFIG_PATH", 1
    )[0])
    with tempfile.TemporaryDirectory(prefix="la-studio-unified-bundle-") as directory:
        bundle_root = Path(directory)
        for relative_path, payload in embedded_files.items():
            target = bundle_root / relative_path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(payload, encoding="utf-8")
        for selection in defaults:
            capability = selection["capability"]
            model = selection["model"]
            exact_notebook = coordinator.discover_exact_notebook(bundle_root, capability, model)
            worker_source = coordinator.worker_source_for(
                coordinator.WorkerSpec(capability, model, exact_notebook), bundle_root
            )
            compile(worker_source, str(exact_notebook), "exec")
    print("Unified Dubbing notebook contract: PASS")


if __name__ == "__main__":
    main()
