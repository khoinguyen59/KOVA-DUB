# Source Separation Architecture

This document describes the design, threading model, and implementation details of the Source Separation capability in LA Studio.

## 1. Component Diagram

```text
VoiceIsolatorController / DubbingJobRunner
                     │
         [SeparationRequest struct]
                     ▼
          SourceSeparationService
                     │
          [owns & runs via QThread]
                     ▼
             SeparationWorker
                     │
                     ├─► SeparationCache (lookup / save manifest)
                     │
                     ├─► SeparationAudioIO (decode via QAudioDecoder/FFmpeg)
                     │
                     └─► SeparationBackendFactory
                               │
                       [create backend]
                               ▼
                   SeparationBackend (Interface)
                               ▲
                               │
                  SherpaOnnxSeparationBackend
                               │
                       [owns instance]
                               ▼
                      SherpaOnnxRuntime (RAII DLL wrapper)
```

---

## 2. Ownership & Threading Model

- **`SourceSeparationService`**: Lives on the main/UI thread. It owns a dedicated `QThread` and an instance of `SeparationWorker`.
- **`SeparationWorker`**: Moved to the dedicated worker thread. All heavy computations—hashing, media decoding (via QAudioDecoder or FFmpeg), inference, cache read/write, and WAV encoding—are performed sequentially on this worker thread.
- **Queued Connections**: Thread communication uses Qt's queued signal/slot mechanism. Audio buffers are **never** passed back via signals to the UI thread; instead, filepath references to output stems are returned in the final `SeparationResult`.
- **Destruction Safety**: When the service is destroyed, it sets a shared-owned atomic cancel flag, requests the thread to quit, and waits only for a bounded graceful window. If a native backend is still inside its uninterruptible C API call, the worker and thread are detached and self-clean after the call returns. The shared flag outlives the UI service, so the detached worker cannot dereference destroyed service memory. The UI is never blocked indefinitely during window/application shutdown, and the native DLL is not force-terminated mid-call.

---

## 3. Typed Configuration Contract

Clients interact with the service using typed requests defined in `SeparationTypes.h`:

- **`SeparationConfiguration`**: Contains runtime paths, target backend ID, pipeline profile (e.g. `uvr-2stems`, `spleeter-2stems`), and mapping of role names (e.g., `model`, `vocals-model`) to resolved model paths.
- **`SeparationRequest`**: Combines the input media path, output root directory, configuration, and execution parameters (such as `numThreads`).
- **`SeparationResult`**: Reports execution status (`success`), cache status (`cacheHit`), target output stems (path, sample rate, channels), and structured error codes.

---

## 4. Backend Registration & Extension

The system defines the `SeparationBackend` interface. Concrete backends register their creator functions with the `SeparationBackendFactory`:

```cpp
class SeparationBackend {
public:
    virtual ~SeparationBackend() = default;
    virtual QString id() const = 0;
    virtual BackendResult separate(...) = 0;
};
```

### Adding a New Model Profile (UVR/Spleeter on sherpa-onnx)
- Add catalog entry under `catalog-src/`. Specify the correct `backend` and `pipelineProfile`.
- No code changes are required in `SourceSeparationService` or `SeparationWorker`.

### Adding a New Runtime/Inference Engine (e.g., Demucs, ONNX Runtime)
- Implement `SeparationBackend` for the new engine.
- Register the creator function in `SeparationBackendFactory::registerBuiltins()`.
- Add the corresponding runtime ID and model capabilities in the catalog.

---

## 5. Cancellation Mechanism & Limitations

- **Cancellation Token**: Passed down from the service to the worker and backends.
- **FFmpeg/Decoding Checkpoints**: The decoding stage checks the token periodically. If cancelled, FFmpeg is terminated immediately and partial staging files are removed.
- **Sherpa-Onnx Inference Block**: Native `sherpa-onnx` inference is executed as a single C API function call (`SherpaOnnxOfflineSourceSeparationProcess`), which cannot be interrupted midway. Therefore, cancellation requested during inference waits for the native call to return, then immediately discards the output instead of writing it to cache or notifying the user of success. The UI displays "cancellation requested" during this waiting period. A new Separate request is rejected while that native call is draining; it cannot overlap two native separation engines.

### UI responsiveness budget

- The desktop runner uses `InferenceBackendProfile::SourceSeparationCpu`, which reserves CPU capacity for the GUI and caps the native engine at three threads. It no longer passes a hard-coded four-thread value that can oversubscribe small machines.
- The standalone Voice Isolator uses the same policy, so a stale or oversized saved thread count cannot bypass the cap.
- The dedicated `QThread` starts at `QThread::LowPriority`. This does not make inference cancellable, but it prevents a CPU-heavy native call from starving QML input, paint and window-close events.
- FFmpeg decoder timeout recovery has a bounded post-`kill()` wait; a failed child cannot create a second unbounded shutdown wait.
- `SourceSeparationService::isolate()` only starts the worker thread and queues the request; it never invokes the native backend on the caller/UI thread. If the thread cannot start or the queued invocation cannot be scheduled, it returns a terminal error immediately instead of leaving the UI in a permanent processing state.

---

## 6. Cache Invalidation Contract

Separation results are cached under:
`<outputRoot>/<sourceHash>/<configurationHash>/`

### `configurationHash` components:
- Cache schema version (`v2`).
- `backendId` and `pipelineProfile`.
- Runtime ID, version, and runtime file fingerprint (name, size, modification time).
- Specific configuration signature and sorted list of model roles.
- Model file fingerprints (name, size, modification time).
- Expected list of output stem IDs.

### Invalidation criteria:
- If a model file is modified, its fingerprint changes, resulting in a cache miss.
- If a different profile is selected (e.g., UVR vs. Spleeter), the pipeline profile string changes, leading to a different configuration hash.
- Manifest validation: Cache hit is only accepted if `manifest.json` exists, matches the expected schema/key, and all declared stem files exist on disk with non-zero sizes.
