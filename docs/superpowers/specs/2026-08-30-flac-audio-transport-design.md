# FLAC Audio Transport and WAV Compatibility Design

**Status:** Approved for implementation on 2026-08-30

## Goal

Reduce disk usage and Colab transfer size without sending the normalized `master.wav` as the default remote input, while preserving compatibility with existing projects, Colab notebooks, local models, Qt playback, and final export.

## Decisions

1. FLAC is the default lossless interchange format for normalized audio and source-separation artifacts.
2. WAV remains an explicit compatibility option and a temporary adapter format only when an external model requires a WAV container.
3. MP3, M4A, OGG, OPUS, WMA, AIFF, AAC, and existing WAV/FLAC inputs remain user-selectable where the task accepts audio. They are decoded through `AudioFileDecoder` before DSP or model inference.
4. MP3 is never used as the canonical normalized or separated artifact. It is accepted as an input format, but no additional lossy generation is introduced.
5. Existing `.ladub.json` projects with `master.wav`, `analysis.wav`, or WAV stems continue to load. New ingest output uses `master.flac` and `analysis.flac`; path values in the project manifest remain authoritative.
6. Colab separation defaults to `output_format=flac`; its temporary internal `source-44100-stereo.wav` is an implementation detail and is deleted with the job directory.
7. TTS/voice-clone endpoints that currently return WAV are not falsely relabeled as FLAC. The desktop decodes the returned PCM, stages the canonical local artifact using the existing model contract, and uses FLAC for transport only when the worker advertises it.

## Data flow

```text
User input (MP3/WAV/FLAC/...)
        |
        v
FFmpeg ingest + EBU R128 -> master.flac (48 kHz, stereo)
                                  |
                                  +-> analysis.flac (16 kHz, mono)
                                  +-> local Separate via AudioFileDecoder
                                  +-> remote Colab upload as FLAC by default
                                                          |
                                                          v
                                      Colab temporary WAV for inference only
                                                          |
                                  vocals.flac + background.flac download
                                                          |
                          decoder-backed STT / alignment / mixer / export
```

## Compatibility and failure rules

- FLAC support is validated by content and decoder result, not only by suffix.
- A missing or invalid decoder fails the current task with a user-facing actionable error and keeps the full diagnostic in the log.
- A legacy WAV path is accepted if it exists; a project is not silently rewritten until a successful new ingest or explicit save.
- All remote audio upload operations use the actual filename and MIME type. The client must not label a FLAC or MP3 payload as `audio.wav`.
- FLAC is preferred for lossless transfer. WAV can be selected when a worker/model rejects FLAC.
- No UI operation performs a synchronous whole-file decode on the main thread.

## Verification boundary

The implementation must pass focused audio-format tests, full CTest, QML lint, notebook/worker contract checks, the repository prebuild gate, and the production packaged QML smoke before packaging. Live Colab inference remains an external environment check and must be reported separately from offline PASS results.
