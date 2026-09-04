# Transcript merge and translation

Use only the absolute paths supplied in the chat prompt:

- `STT input`: optional speech-recognition script.
- `OCR input`: optional subtitle/OCR script.
- `Translation input`: the canonical script to translate, supplied by the app.
- `Translation output`: the exact file to write, supplied by the app.

If both STT and OCR inputs are supplied, combine them into the canonical script before translating:

- Match cues by timestamp overlap and dialogue order.
- Prefer OCR text and OCR timing when the two sources conflict.
- Use STT as reference and to fill missing or unclear OCR text.
- If a conflict cannot be resolved, keep the STT cue as the fallback; do not invent dialogue.
- Keep the canonical cue order, cue count, numbering, and timestamps unchanged.

Translate the canonical script from Chinese to Vietnamese:

- Preserve every cue, timestamp, meaning, context, emotion, and speaker role.
- Use natural Vietnamese pronouns and clear hierarchy between speakers.
- Convert unambiguous Chinese/Pinyin names to natural Vietnamese/Hán-Việt forms (`Wang` → `Vương`). Keep brands, URLs, and technical identifiers when they are proper names.
- Keep every cue non-empty, meaningful, and readable for its fixed duration. Cut sentences at natural boundaries without changing the timeline or cue count.
- For OCR-derived dialogue, preserve the OCR timeline and content priority; use STT only as reference or fallback.

Write only the translated script to the exact `Translation output` path. Do not change timestamps, cue order, cue count, or file format. Before finishing, verify that the output has identical timestamps in identical order and that every cue contains meaningful text.
