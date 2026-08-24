# TTS and Voice Clone recheck — 2026-08-24

## Scope

This recheck covers the reusable cloned-voice boundary only:

1. Direct Colab Voice Cloning saves a reusable voice profile.
2. Standalone TTS can select that profile without silently switching to Local CPU, API Gateway, or another clone model.
3. Dubbing TTS retains the selected clone/profile across generated segments.
4. The Voice Cloning page lists and reuses profiles for the active exact Colab model.

No EXE was packaged in this recheck.

## Defect found and fixed

The Voice Cloning page previously used the model family currently selected in the gallery when saving and listing a reusable Direct Colab profile. The active worker can be a different exact model from the visible gallery selection. In that state, a successful clone could be saved under the wrong family and therefore not appear in TTS for the verified worker.

The page now derives the family from the active Direct Colab worker model whenever one is active. The same effective model is used for the saved-reference picker and model-specific transcript guidance. A request snapshots that model before it starts, so a later UI selection cannot file the finished result under a different family.

The Colab clone controller also retains the submitted reusable voice name for the completed history item. History now identifies both the Direct Colab model and the actual profile name rather than recording every result as a generic `Clone`.

## Behaviour after the change

- A Direct Colab cloned profile is saved and listed only under the exact verified worker model.
- TTS exposes reusable profiles only for the active Direct Colab voice-clone model and still requires user consent before clone synthesis.
- Changing the worker/model does not make a profile appear for a different model family.
- Existing per-model Colab, API Gateway, and local routes remain independent; no fallback route was added.
- Dubbing continues to use its selected saved clone profile for its voice-generation work rather than substituting a gallery default.

## Verification performed

All verification below ran against the current source after rebuilding the changed C++ and QML code in the MSVC environment.

| Check | Result |
| --- | --- |
| `LAStudio` compile target | Passed; includes `ColabVoiceCloneController.cpp` and `VoiceCloningStudioView.qml` |
| `LAStudioUnitTests` compile target | Passed; includes the changed clone controller and regression test |
| Focused regression group: Dubbing project, remote execution, Colab voice clone runner, Colab TTS runner | 4/4 passed |
| QML route smoke | passed |
| Full CTest suite | 39/39 passed |
| QML lint | passed; only pre-existing warnings in unrelated workflow dialogs |
| `git diff --check` | passed |

The first direct `cmake --build` invocation did not inherit the Visual Studio C++ include environment and failed on standard headers such as `type_traits`. Re-running through the repository's `vcvars64.bat` bootstrap compiled the application and tests successfully. This was a terminal environment issue, not an application source failure.

## Boundaries not claimed

This batch did not start a live Colab notebook or submit a real audio reference to an external worker, so it does not claim live-network acceptance. The exact UI-to-controller persistence boundary, source compile, QML smoke, and regression suite are covered locally. A future live acceptance run should use a verified exact-model Colab worker and confirm that a newly cloned named profile appears in both standalone TTS and the Dubbing TTS selector.
