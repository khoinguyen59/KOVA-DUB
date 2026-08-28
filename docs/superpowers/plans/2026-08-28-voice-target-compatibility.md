# Voice target compatibility across Clone, TTS, and Dubbing

## Goal

Make every valid voice preset and every user-created clone usable through both
VieNeu and OmniVoice in the Voice Cloning, TTS, and Dubbing tabs. A voice's
original model family remains provenance only; it must not decide which target
runtime is allowed to execute the clone.

## Current verified gaps

- `VoiceClonePresetService` only adds an OmniVoice compatibility entry for
  VieNeu presets and `presetsForFamily()` still filters primarily by source
  family.
- Dubbing derives `voiceCloneModelId` from the source family, so CapCut/F5 and
  other presets can be rejected even when their managed reference audio is
  valid.
- Dubbing local execution rejects saved clone voices unless the loaded backend
  is Qwen3-TTS, then calls ordinary `synthesize()` with private profile keys.
  VieNeu and OmniVoice already expose real `cloneVoice()` backends but are not
  dispatched through that path by Dubbing.
- The QML TTS and Dubbing selectors use the selected preset's source family as
  the worker model, which prevents a single voice from being selected for both
  target models.

## Design

### Shared voice record

Preserve `sourceModelFamily`/`familyId` for provenance. Normalize every record,
including legacy user records, with:

- `voiceModelTargets: ["vieneu", "omnivoice"]`;
- `targetBindings.vieneu` and `targetBindings.omnivoice`, each carrying the
  managed reference audio, reference transcript, target model family, and a
  status derived from reference validation;
- the existing `compatibleModelFamilies` field retained for backward
  compatibility, with canonical target aliases added rather than replacing
  source metadata.

The service will migrate records in memory and persist the new fields whenever
a user preset is saved. A target is `reference-ready` only when the managed
audio passes the existing file/checksum validation. Runtime/worker readiness is
separate and is never faked by a metadata badge.

### Target resolution

Use two UI targets:

- `vieneu`, resolved to the selected exact VieNeu runtime (v3 Turbo by
  default, while an explicitly selected v2 runtime remains valid);
- `omnivoice`, resolved to the exact OmniVoice runtime.

The target is stored as `voiceCloneModelId` for workflow execution. The source
family is never used as the target route. Any valid reference-backed voice is
cloned by passing its managed reference audio and optional transcript to the
selected backend.

### Execution

- Local Dubbing calls `TtsEngine::cloneVoice()` for each generated chunk when a
  saved voice is selected. This makes the VieNeu and OmniVoice backend clone
  implementations reachable and removes the Qwen-only saved-profile gate.
- Existing Colab profile reuse remains enabled, but the profile cache key is
  scoped by preset id, target model, reference signature, language, and worker
  session. Direct Colab validates the exact target notebook before dispatch.
- API Gateway remains a normal TTS route; if it cannot consume a reference
  clone, the UI reports the route limitation and offers Local/Direct Colab
  without silently substituting another voice.
- Reference transcript is optional where the selected runtime supports audio
  only. Missing transcript may lower quality, but it must not make a valid
  private clone disappear from either target list.

### UI

- Voice Gallery cards in Clone, TTS, and Dubbing show both `VieNeu` and
  `OmniVoice` target badges for every valid reference-backed voice.
- TTS and Dubbing expose target selection independently from source metadata.
  Selecting any voice keeps the selected target and opens the matching model/
  Colab setup when it is not ready.
- Clone saves the source reference once; the same preset becomes available to
  both target bindings without duplicating the user's audio file.
- `Ready` is shown only for a validated reference plus a loaded local runtime
  or verified remote worker. Otherwise the action is `Prepare`, `Connect`, or
  an actionable error with the existing technical log preserved.

## Test-first acceptance criteria

1. Presets from `f5-tts`, `vieneu-tts`, `omnivoice`, arbitrary legacy family,
   and user-created custom records expose both canonical targets.
2. Old user preset JSON loads without data loss and gains target bindings.
3. `presetsForFamily("vieneu")`, an exact VieNeu model id, and `omnivoice`
   return every valid reference-backed preset, regardless of source family.
4. Local VieNeu and OmniVoice clone requests receive the selected reference
   path and transcript; Dubbing does not reject them as non-Qwen profiles.
5. Direct Colab clone requests use the selected target model and do not derive
   the route from the source voice family.
6. Clone, TTS, and Dubbing QML source contracts expose both target labels and
   do not filter a valid custom clone by its source family.
7. Existing tests remain green; QML lint and production preview smoke pass.

## Verification

- Run the focused unit tests red before production changes.
- Build the configured test target and run the complete CTest suite.
- Run QML lint and the production Dubbing/voice preview smoke checks.
- Update the relevant audit/report Markdown with changed symbols and evidence.

## Implementation status

- [x] Normalize every preset to the VieNeu + OmniVoice target contract.
- [x] Resolve bundled and user reference audio through managed storage rules.
- [x] Route Clone, TTS, and Dubbing selection through the shared target-aware library.
- [x] Dispatch local Dubbing saved voices through the backend `cloneVoice()` path.
- [x] Enforce exact target/runtime matching and keep remote route failures explicit.
- [x] Add target-aware gallery badges/filtering, including user-created voices.
- [x] Build the Qt/MSVC test target and run `TestDubbingProject` successfully.
- [x] Run full CTest, QML lint, and production preview smoke successfully.
- [x] Update the audit report with implementation and verification evidence.

The local verification boundary is green. Live VieNeu/OmniVoice inference and
Direct Colab GPU verification remain deployment tests because this environment
does not provide the required model assets, GPU, or Colab credentials.
