#!/usr/bin/env python3
"""Unit tests for the immutable Colab worker pin validator."""

from __future__ import annotations

import hashlib
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from urllib.error import HTTPError

from verify_colab_worker_pins import (
    GENERATOR,
    NOTEBOOK,
    WorkerPin,
    load_pins,
    notebook_matches_generator,
    validate_pin,
    local_sha256,
)


class ColabWorkerPinTests(unittest.TestCase):
    def test_project_generator_declares_an_immutable_repository_and_workers(self) -> None:
        pins = load_pins(GENERATOR)

        self.assertEqual("khoinguyen59/KOVA-DUB", pins[0].repository)
        self.assertEqual(2, len(pins))
        self.assertTrue(all(len(pin.commit) == 40 for pin in pins))

    def test_checked_in_spleeter_notebook_matches_its_generator_pin(self) -> None:
        pins = load_pins(GENERATOR)

        self.assertEqual([], notebook_matches_generator(NOTEBOOK, pins))

    def test_rejects_a_missing_remote_commit(self) -> None:
        pin = WorkerPin(
            destination="worker.py",
            repository="owner/repo",
            commit="a" * 40,
            relative_path="workers/worker.py",
            expected_sha256="0" * 64,
        )

        def fetcher(url: str, timeout: float) -> bytes:
            raise HTTPError(url, 404, "Not Found", {}, None)

        result = validate_pin(pin, fetcher=fetcher, sleep=lambda _: None)

        self.assertFalse(result.ok)
        self.assertIn("HTTP 404", result.message)

    def test_rejects_a_remote_sha256_mismatch(self) -> None:
        payload = b"worker payload"
        pin = WorkerPin(
            destination="worker.py",
            repository="owner/repo",
            commit="b" * 40,
            relative_path="workers/worker.py",
            expected_sha256=hashlib.sha256(b"different payload").hexdigest(),
        )

        result = validate_pin(
            pin,
            fetcher=lambda url, timeout: payload,
            sleep=lambda _: None,
        )

        self.assertFalse(result.ok)
        self.assertIn("SHA-256 mismatch", result.message)

    def test_accepts_an_exact_remote_payload(self) -> None:
        payload = b"worker payload"
        pin = WorkerPin(
            destination="worker.py",
            repository="owner/repo",
            commit="c" * 40,
            relative_path="workers/worker.py",
            expected_sha256=hashlib.sha256(payload).hexdigest(),
        )

        result = validate_pin(
            pin,
            fetcher=lambda url, timeout: payload,
            sleep=lambda _: None,
        )

        self.assertTrue(result.ok)
        self.assertEqual(
            result.url,
            "https://raw.githubusercontent.com/owner/repo/"
            "cccccccccccccccccccccccccccccccccccccccc/workers/worker.py",
        )

    def test_local_text_worker_hash_is_stable_across_windows_line_endings(self) -> None:
        payload = b"first line\nsecond line\n"
        with TemporaryDirectory() as directory:
            path = Path(directory) / "worker.py"
            path.write_bytes(payload.replace(b"\n", b"\r\n"))

            self.assertEqual(hashlib.sha256(payload).hexdigest(), local_sha256(path))


if __name__ == "__main__":
    unittest.main(verbosity=2)
