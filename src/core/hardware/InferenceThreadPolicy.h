#pragma once

#include <QString>

namespace LAStudio {

enum class InferenceBackendProfile {
    WhisperCpu,
    WhisperGpu,
    CrispAsrCpu,
    CrispAsrGpu,
    VieNeuNative,
    Kokoro,
    RealtimeTts,
    // Source separation invokes a native CPU inference call. Keep a small
    // bounded budget so that decoding, QML rendering and input handling retain
    // CPU capacity while the model is running.
    SourceSeparationCpu
};

struct InferenceThreadRequest {
    InferenceBackendProfile profile = InferenceBackendProfile::CrispAsrCpu;
    int requestedThreads = 0;
    int activeInferenceJobs = 1;
};

class InferenceThreadPolicy {
public:
    static int recommendedThreadCount(const InferenceThreadRequest &request);
    static int recommendedThreadCount(InferenceBackendProfile profile, int requestedThreads = 0);
    static int effectiveCoreCount();
    static int logicalThreadCount();
    static QString describeProfile(InferenceBackendProfile profile);
};

} // namespace LAStudio
