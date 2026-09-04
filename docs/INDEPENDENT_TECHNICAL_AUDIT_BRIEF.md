# LA Studio — Independent Technical Audit Brief

## Project under review

Review the checkout at:

`C:\Users\Nguyen Trong Khoi\Downloads\TTS\LA-Studio`

LA Studio is a Windows desktop dubbing studio built with C++/Qt/QML and Python-based local, notebook, and remote AI engines. Its production workflow has eight tasks:

`1 Import` · `2 Normalize` · `3 Separate` · `4 Transcribe` · `5 Translate` · `6 Synthesize` · `7 Align` · `8 Mix & Export`

The review is independent. Existing reports, checklists, screenshots, and claims of completion are evidence to verify, not conclusions to accept.

## Audit scope

Assess the actual source, tests, notebooks, generated/runtime assets, and build/release path for:

- code and documentation accuracy: C++ symbols, Qt properties/signals/slots/invokables, QML types and bindings, Python/notebook interfaces, and external command contracts;
- workflow correctness: navigation, prerequisites, optional tasks, local upload/handoff, STT and OCR independence, reconciliation, translation, synthesis, alignment, mixing, export, and project reopen/persistence;
- state and concurrency: UI responsiveness, workers, subprocesses, cancellation, retry, resource lifetime, audio playback, race/deadlock risks, and Colab/local route transitions;
- UI/UX behavior: task ownership, controls, dialogs, thumbnail/video preview, timeline and seek behavior, scrolling, clipping, text collision, responsive sizing, and action feedback;
- build and delivery reliability: clean configure/build, test execution, QML validation, notebook/worker consistency, runtime packaging, startup, and release artifact layout.

Do not assume a feature is correct because a button exists. Trace the action to its implementation and, where feasible, execute the smallest reproducible path.

## Independence and evidence rules

1. Do not modify C++, QML, Python, notebooks, configuration, documentation, project data, or Git history.
2. Do not implement fixes, rewrite the product, or make design decisions on the project's behalf.
3. Use the repository as the primary source of truth. Use official external documentation only when an external API, CLI, model, or package contract must be verified.
4. For every finding, record the exact absolute or repository-relative file and line/symbol, the observed behavior, the expected behavior, and the evidence used.
5. Label each conclusion `Verified`, `Partially verified`, `Inferred`, or `Not verified`; distinguish a code defect from an unavailable environment, missing model, expired worker, or test limitation.
6. Do not treat a warning as a failure without showing its effect. Do not treat a passing unit test as proof of an end-to-end workflow.
7. Avoid speculative refactors and avoid inflating the result with cosmetic observations that do not affect correctness, usability, or delivery risk.

## Required review result

Return a concise but evidence-backed audit containing:

1. executive conclusion and release risk;
2. findings grouped by severity (`Blocker`, `High`, `Medium`, `Low`) with file/line evidence;
3. an eight-task matrix covering navigation, inputs/outputs, optionality, state, error handling, and handoff for every task;
4. separate UI/UX, threading/runtime, notebook/remote, persistence, and packaging observations;
5. tests and commands actually run, with pass/fail output summarized;
6. limitations and items that could not be verified;
7. recommendations only after the evidence, without silently applying changes.

A clean result means “no verified blocking defect in the tested scope”, not “the product is guaranteed defect-free”.
