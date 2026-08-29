#!/usr/bin/env python3
"""Regression tests for the self-contained Spleeter Colab worker bundle."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from verify_colab_worker_pins import (
    GENERATOR,
    NOTEBOOK,
    embedded_sources_from_notebook,
    load_workers,
    local_sha256,
    notebook_matches_generator,
)


ROOT = GENERATOR.parents[1]


class ColabWorkerBundleTests(unittest.TestCase):
    def test_generator_declares_local_worker_sources_without_personal_repository(self) -> None:
        workers = load_workers(GENERATOR)
        generator_source = GENERATOR.read_text(encoding="utf-8")

        self.assertEqual(
            {
                "la_studio_separation_worker.py",
                "la_studio_separation_launcher.py",
            },
            {worker.destination for worker in workers},
        )
        self.assertTrue(all(worker.relative_path.startswith("notebooks/workers/") for worker in workers))
        self.assertNotIn("WORKER_REPOSITORY", generator_source)
        self.assertNotIn("WORKER_COMMIT", generator_source)

    def test_checked_in_notebook_contains_the_exact_local_worker_bundle(self) -> None:
        workers = load_workers(GENERATOR)

        self.assertEqual([], notebook_matches_generator(NOTEBOOK, workers))
        embedded = embedded_sources_from_notebook(NOTEBOOK)
        self.assertEqual({worker.destination for worker in workers}, set(embedded))
        for worker in workers:
            source = (ROOT / worker.relative_path).read_text(encoding="utf-8")
            self.assertEqual(source.replace("\r\n", "\n"), embedded[worker.destination][1])

    def test_rejects_a_notebook_that_fetches_worker_from_personal_github(self) -> None:
        document = json.loads(NOTEBOOK.read_text(encoding="utf-8"))
        document["cells"].append(
            {
                "cell_type": "code",
                "execution_count": None,
                "metadata": {},
                "outputs": [],
                "source": [
                    "from urllib.request import urlopen\n",
                    "urlopen('https://raw.githubusercontent.com/khoinguyen59/KOVA-DUB/main/worker.py')\n",
                ],
            }
        )
        with TemporaryDirectory() as directory:
            path = Path(directory) / NOTEBOOK.name
            path.write_text(json.dumps(document), encoding="utf-8")

            issues = notebook_matches_generator(path, load_workers(GENERATOR))

        self.assertTrue(any("personal GitHub" in issue for issue in issues))

    def test_rejects_an_embedded_worker_hash_mismatch(self) -> None:
        document = json.loads(NOTEBOOK.read_text(encoding="utf-8"))
        for cell in document["cells"]:
            source = "".join(cell.get("source", []))
            if "EMBEDDED_WORKERS" in source:
                cell["source"] = [source.replace("307861926e13ff9849b04594074b573b1da063b1791c56dc2f502ab64991c5af", "0" * 64)]
                break
        with TemporaryDirectory() as directory:
            path = Path(directory) / NOTEBOOK.name
            path.write_text(json.dumps(document), encoding="utf-8")

            issues = notebook_matches_generator(path, load_workers(GENERATOR))

        self.assertTrue(any("SHA-256" in issue for issue in issues))

    def test_local_text_worker_hash_is_stable_across_windows_line_endings(self) -> None:
        payload = b"first line\nsecond line\n"
        with TemporaryDirectory() as directory:
            path = Path(directory) / "worker.py"
            path.write_bytes(payload.replace(b"\n", b"\r\n"))

            self.assertEqual(hashlib.sha256(payload).hexdigest(), local_sha256(path))


if __name__ == "__main__":
    unittest.main(verbosity=2)
