#!/usr/bin/env python3
"""Regenerate the legacy voice-clone notebook as a self-contained OmniVoice alias."""

from __future__ import annotations

import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CANONICAL = ROOT / "notebooks" / "voice_cloning" / "LA_STUDIO_VOICE_CLONE_OMNIVOICE_GPU.ipynb"
LEGACY = ROOT / "notebooks" / "voice_cloning" / "LA_STUDIO_VOICE_CLONE_GPU.ipynb"


def make_notebook() -> dict:
    document = json.loads(CANONICAL.read_text(encoding="utf-8"))
    document["metadata"].setdefault("la_studio", {})["legacy_alias_for"] = CANONICAL.name
    document["metadata"]["la_studio"]["compatibility_alias"] = True
    document["cells"][0] = {
        "cell_type": "markdown",
        "metadata": {},
        "source": [
            "# LA Studio Direct Colab Voice Clone GPU (compatibility alias)\n",
            "\n",
            "This legacy entry now runs the self-contained exact OmniVoice notebook contract. "
            "It does not clone or download LA Studio application code.\n",
            "\n",
            "Only clone a voice when you have the speaker's explicit permission and provide the "
            "exact reference transcript.\n",
            "\n",
            f"Canonical implementation: `{CANONICAL.name}`.\n",
        ],
    }
    return document


def main() -> None:
    LEGACY.write_text(json.dumps(make_notebook(), ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    print(LEGACY)


if __name__ == "__main__":
    main()
