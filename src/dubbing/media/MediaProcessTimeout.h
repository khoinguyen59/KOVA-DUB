#pragma once

#include <QtGlobal>

namespace LAStudio::MediaProcessTimeout {

// A test-only override keeps the timeout regression tests deterministic while
// preserving conservative production deadlines for real media files. The
// value is deliberately process-wide so all media boundaries fail closed in
// the same way when an operator needs to diagnose a problematic runtime.
inline int configured(int fallbackMilliseconds)
{
    bool ok = false;
    const int overrideMilliseconds = qEnvironmentVariableIntValue(
        "LASTUDIO_MEDIA_PROCESS_TIMEOUT_MS", &ok);
    return ok && overrideMilliseconds > 0 ? overrideMilliseconds : fallbackMilliseconds;
}

constexpr int kProbeTimeoutMs = 60 * 1000;
constexpr int kFfmpegTimeoutMs = 30 * 60 * 1000;
constexpr int kValidationTimeoutMs = 60 * 1000;

} // namespace LAStudio::MediaProcessTimeout
