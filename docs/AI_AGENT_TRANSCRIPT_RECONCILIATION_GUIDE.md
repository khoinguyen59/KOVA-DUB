# Transcript merge and translation

Use only the absolute paths supplied in the chat prompt:

- `STT input`: optional speech-recognition script.
- `OCR input`: optional subtitle/OCR script.
- `Canonical input`: the app's current script and fixed cue grid.
- `Merged output`: the exact script to write after reconciling STT/OCR.
- `Translation output`: the exact Vietnamese script to write.

If both STT and OCR inputs are supplied, write the merged script directly to
`Merged output` before translating it:

- Match cues by timestamp overlap and dialogue order.
- Use OCR as the cue/timestamp grid and prefer its text when the two sources conflict.
- Use STT as reference and to fill missing or unclear OCR text.
- If OCR is unavailable, use the canonical/STT cue grid. Do not invent dialogue.
- Keep the cue order, cue count, numbering, and timestamps unchanged.

Translate the merged script (or `Canonical input` when only one source is
supplied) from Chinese to Vietnamese and write it directly to `Translation output`:

- Preserve every cue, timestamp, meaning, context, emotion, and speaker role.
- Use natural Vietnamese pronouns and clear hierarchy between speakers.
- Convert unambiguous Chinese/Pinyin names to natural Vietnamese/Hán-Việt forms (`Wang` → `Vương`). Keep brands, URLs, and technical identifiers when they are proper names.
- Keep every cue non-empty, meaningful, and readable for its fixed duration. Cut sentences at natural boundaries without changing the timeline or cue count.
- For OCR-derived dialogue, preserve the OCR timeline and content priority; use STT only as reference or fallback.

Do not change timestamps, cue order, cue count, or file format. Before finishing, verify that both output files have identical timestamps in identical order and that every cue contains meaningful text.
