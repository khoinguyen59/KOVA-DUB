# Pre-Build Release Gate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make every portable release build run the accumulated pre-delivery checks automatically and stop before compilation when a known regression, source error, QML issue, or test failure is detected.

**Architecture:** Add a repository-rooted PowerShell gate that runs deterministic source checks, `git diff --check`, QML lint, C++/CTest regression tests, and the existing Colab/notebook contract validators. Make `scripts/package.ps1` invoke that gate before configure/build. Persist a machine-readable result under `out\prebuild-gate\latest.json`, and document the release-only command and bypass boundary in the checklist.

**Tech Stack:** PowerShell 5+/7, Qt 6 QML lint, CMake/CTest, Python validators already present in `scripts/`, Markdown release checklist.

**Spec:** `C:\Users\Nguyen Trong Khoi\Downloads\TTS\PRE_DELIVERY_CHECKLIST.md`

## Task 1: Establish a red contract for the mandatory gate

- [x] Add a workspace contract test that requires `scripts/prebuild_gate.ps1` to exist, contain the required checks, and be invoked by `scripts/package.ps1`.
- [x] Run the focused contract test and confirm it fails before the gate is implemented.

## Task 2: Implement the pre-build gate

- [x] Add `scripts/prebuild_gate.ps1` with fail-fast named checks and stable exit codes.
- [x] Validate the checklist and required source/build scripts before expensive checks.
- [x] Run `git diff --check`, QML lint, full CTest, and the existing Colab/remote/notebook validators through the repository root.
- [x] Write `out\prebuild-gate\latest.json` on success or failure with check names, durations, exit status, and timestamps.

## Task 3: Enforce the gate in packaging

- [x] Invoke the gate from `scripts/package.ps1` before CMake configure/build.
- [x] Abort packaging with a non-zero exit code when any gate check fails.
- [x] Keep package staging/layout verification after build as a separate post-build responsibility.

## Task 4: Update release documentation

- [x] Add the exact manual gate command and release rule to `PRE_DELIVERY_CHECKLIST.md`.
- [x] Record that direct CMake invocation is not a release build and that the package script is the enforced entry point.
- [x] Add a dated audit entry describing the gate and its evidence file.

## Task 5: Verify the complete path

- [x] Run the focused contract test, full CTest, QML lint, and the gate itself.
- [x] Run the package script and verify that the gate executes before the package build and that the portable output remains valid.
- [x] Inspect the generated gate evidence and run `git diff --check` before reporting completion.
