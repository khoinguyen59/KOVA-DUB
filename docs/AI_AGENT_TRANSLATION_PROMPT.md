# AI Agent Translation Contract: Timed Dubbing Script

Use this as a copy-paste prompt for an AI agent in any IDE or automation
environment. It is intentionally strict: the translated script is an input to
voice synthesis, so structural correctness is more important than stylistic
freedom.

## Copy-paste prompt

```text
You are translating a timed dubbing transcript.

Source language: Chinese (zh) by default.
Target language: Vietnamese (vi) by default.
The caller may explicitly override these languages, but never infer a new
language from the text.

Return JSON only. Do not return Markdown, commentary, explanations, or code
fences.

Input is an ordered array of timed cues. For every input cue, return exactly
one output cue with the same id, startMs, endMs, duration, order, speakerId,
role, and semantic context. Do not add cues, delete cues, split cues, merge
cues, or move cues.

Hard constraints:
1. Preserve startMs and endMs byte-for-byte when represented as integers.
2. Preserve cue count and cue order.
3. targetText must be non-empty for every cue that has non-empty sourceText.
4. Preserve meaning, context, speaker identity, social rank, gender only when
   it is actually present, and who is speaking to whom.
5. Preserve names, numbers, entities, negation, tense, intent, and scene
   context. Do not hallucinate facts or dialogue.
6. Write natural Vietnamese appropriate to the speaker and scene, but do not
   shorten away necessary meaning.
7. Keep the amount of Vietnamese text reasonable for the cue duration. Avoid
   both one-word under-translation and an overlong sentence that cannot be
   spoken in the preserved interval. If necessary, rewrite compactly without
   changing meaning; never change the timeline to fit text.
8. Never output empty filler, placeholder text, “TODO”, or an explanation in a
   dialogue field.
9. If the input is an OCR-derived cue, preserve its original timeline and
   preserve its source character count in `sourceCharCount`. Do not rewrite,
   re-segment, or retime an OCR cue. `targetText` may have a different
   language character count, but the source OCR text and its count must remain
   untouched.
10. If a cue is ambiguous, preserve the safest literal meaning and put the
    ambiguity in `notes`; do not invent a speaker or event.

Before returning, validate every cue against the schema and all constraints.
If the input is invalid, return a JSON object with `error` and `details`
instead of fabricating a translation.
```

## Required JSON shape

```json
{
  "schemaVersion": 1,
  "sourceLanguage": "zh",
  "targetLanguage": "vi",
  "segments": [
    {
      "id": "cue-001",
      "startMs": 1200,
      "endMs": 3400,
      "durationMs": 2200,
      "speakerId": "speaker-1",
      "role": "manager",
      "context": "manager speaks to an employee during a briefing",
      "sourceText": "原文",
      "targetText": "Bản dịch tiếng Việt có nội dung",
      "sourceKind": "stt|ocr|reconciled",
      "sourceCharCount": 2,
      "notes": ""
    }
  ]
}
```

## Validation rules

For each input/output pair, an agent must assert:

```text
output.count == input.count
output[i].id == input[i].id
output[i].startMs == input[i].startMs
output[i].endMs == input[i].endMs
output[i].durationMs == input[i].endMs - input[i].startMs
output[i].sourceText == input[i].sourceText
output[i].sourceCharCount == UnicodeCodePointCount(input[i].sourceText)
output[i].targetText.trim() != "" whenever sourceText.trim() != ""
```

Additional quality gates:

- reject negative timestamps and non-positive durations;
- reject duplicate IDs or changed order;
- reject target text that is only punctuation or a placeholder;
- reject output that drops names, numbers, negation, or explicit speaker
  markers without a documented linguistic reason;
- flag a cue for human review if the estimated spoken density is implausible,
  but never alter the preserved timeline;
- make the threshold deterministic and record it in the audit report.

## Timeline and text-density guidance

The duration is fixed. A practical agent may flag unusually dense Vietnamese
for review using a configurable heuristic such as target characters per second,
but this is a warning, not permission to change timestamps. The agent should
prefer concise, complete Vietnamese over literal word-for-word output when the
cue is short. It must not collapse a meaningful cue into a vague summary.

For OCR cues, the original OCR cue is authoritative evidence for its original
time interval and segmentation. Preserve it exactly in the canonical data and
translate only its text field into the target-language field.

## Failure behavior

On malformed JSON, missing IDs, missing timestamps, empty required dialogue,
or a mismatch between input and output count, fail closed with structured
diagnostics. Never return a partially valid script claiming success.
