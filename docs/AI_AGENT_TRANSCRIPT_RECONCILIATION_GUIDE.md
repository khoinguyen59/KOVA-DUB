# AI Agent Guide: Merge STT and OCR Scripts into One Canonical Transcript

This document is an IDE-agnostic contract for an AI coding agent that must
reconcile two independently produced timed scripts:

- `STT`: speech-to-text observations from audio;
- `OCR`: subtitle/text observations from video frames.

The agent may be run from any IDE, terminal, notebook, or code-review tool.
It must not assume a particular UI, framework, model vendor, or file layout.

## Non-negotiable behavior

1. Never invent a cue, speaker, timestamp, or dialogue text.
2. Never modify `startMs` or `endMs` merely to make two sources line up.
3. Keep the original STT and OCR files unchanged. Write a separate canonical
   output and an audit report.
4. If both sources contain a cue but cannot be matched with sufficient
   evidence, use the STT cue as the canonical cue by default when STT exists.
   Do not append unmatched OCR-only cues to the default canonical script.
5. Preserve unmatched OCR as evidence in the audit section, so a human can
   review it later. `prefer-ocr` or explicit human review are opt-in policies.
6. If STT is empty, a valid OCR script may become canonical. If both are empty,
   fail with a clear error; never produce an empty “successful” script.
7. A canonical cue must have a positive duration and non-empty source text.

## Accepted input normalization

Accept JSON, SRT, VTT, or ASS/SSA. Convert each source to this internal form
before matching:

```json
{
  "id": "stable-or-generated-id",
  "startMs": 1200,
  "endMs": 3400,
  "sourceText": "original dialogue",
  "speakerId": "speaker-1",
  "confidence": 0.86,
  "source": "stt"
}
```

Rules:

- Parse timestamps into integer milliseconds.
- Reject cues with `startMs < 0`, `endMs <= startMs`, or empty text.
- Trim whitespace, preserve the original text in evidence, and use normalized
  text only for comparison.
- OCR cues must retain their frame/subtitle timing. Do not convert OCR into
  audio timing by shifting it.
- Keep cue order stable by `(startMs, endMs, originalIndex)`.
- Keep confidence in `[0, 1]`; use a documented fallback only when absent.

## Deterministic matching algorithm

For every STT cue, find at most one unused OCR cue. A candidate is eligible
when either:

- interval overlap ratio is at least `0.30`; or
- cue centers are at most `650 ms` apart and text similarity is at least
  `0.40`.

Score eligible candidates as:

```text
score = 0.70 * intervalOverlap + 0.30 * tokenJaccardSimilarity
```

Choose the highest score. Break ties deterministically by:

1. higher score;
2. higher interval overlap;
3. smaller center-time distance;
4. smaller original OCR index.

For a matched pair, classify a text conflict when normalized token similarity
is below `0.55`. The canonical decision is:

| Policy | Matched conflict | Unmatched OCR | Canonical text |
|---|---|---|---|
| `prefer-stt` (default) | STT | keep only in audit | STT |
| `prefer-ocr` | OCR | OCR may be canonical | OCR |
| `ask` | pending human review | visible for review | STT only after approval |
| `ai-suggest` | suggestion only; never auto-commit | audit | STT until approval |

When the texts match sufficiently, keep STT timing and STT text by default.
The OCR text and confidence still remain in provenance. Do not silently use a
higher OCR confidence to replace STT under `prefer-stt`.

## Canonical output contract

Write one output file containing:

```json
{
  "schemaVersion": 1,
  "policy": "prefer-stt",
  "canonicalSource": "stt",
  "segments": [
    {
      "id": "stt-001",
      "startMs": 1200,
      "endMs": 3400,
      "sourceText": "canonical dialogue",
      "targetText": "",
      "speakerId": "speaker-1",
      "timingSource": "stt",
      "fusionStatus": "matched|stt-only|resolved",
      "fusionChoice": "stt|ocr|pending",
      "fusionNeedsReview": false,
      "transcriptProvenance": [
        {"source": "stt", "text": "...", "confidence": 0.86},
        {"source": "ocr", "text": "...", "confidence": 0.91}
      ]
    }
  ],
  "audit": {
    "sttCount": 0,
    "ocrCount": 0,
    "matchedCount": 0,
    "conflictCount": 0,
    "unmatchedOcrCount": 0,
    "unmatchedOcr": [],
    "warnings": []
  }
}
```

The actual project may use a QVariant/JSON equivalent, but the semantic
fields must remain available. The `segments` array is the only array passed
to translation and TTS by default. The OCR array remains durable evidence.

## Reference pseudocode

```text
stt = normalize(sttInput, source="stt")
ocr = normalize(ocrInput, source="ocr")
if stt is empty and ocr is empty: fail("no usable transcript")
if stt is empty: return canonicalizeOcr(ocr)

usedOcr = set()
canonical = []
for s in stt in stable order:
    candidates = eligibleUnusedOcr(s, ocr, usedOcr)
    match = deterministicBest(candidates)
    if no match:
        canonical.append(markSttOnly(s))
        continue
    usedOcr.add(match.index)
    result = attachEvidence(s, match)
    if isConflict(s, match):
        if policy == "prefer-ocr": chooseOcr(result)
        elif policy == "ask" or policy == "ai-suggest": markPending(result)
        else: chooseStt(result)
    else:
        if policy == "prefer-ocr": chooseOcr(result)
        else: chooseStt(result)
    canonical.append(result)

if policy != "prefer-stt" or stt is empty:
    appendEligibleOcrOnlyCuesForExplicitReviewOrPromotion()
return canonical
```

## Acceptance checks for the agent

The implementation is not complete until all checks pass:

- identical STT/OCR cues produce one canonical cue, never duplicates;
- shifted but overlapping cues still match without changing either source
  timestamp;
- mismatched text defaults to STT and records OCR evidence;
- OCR-only cues are not appended under the default `prefer-stt` policy;
- explicit `ask` exposes conflicts and does not claim a silent resolution;
- STT-empty/OCR-valid promotes OCR with its original timeline;
- empty/invalid inputs fail clearly;
- output ordering is stable across repeated runs;
- canonical cues have non-empty text and `endMs > startMs`;
- the original STT/OCR files remain byte-for-byte unchanged.

## Agent instruction

Before editing code, locate the project's actual transcript schema and service
entry point. Reuse existing parsing, persistence, logging, and test helpers.
Do not create a second competing fusion implementation. Add regression tests
for every policy and preserve source provenance for debugging.
