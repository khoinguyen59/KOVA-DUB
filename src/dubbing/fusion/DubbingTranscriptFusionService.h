#pragma once

#include <QVariantList>

namespace LAStudio {

// Deterministic STT/OCR reconciliation. It never calls a model. STT is the
// safe canonical fallback when OCR cannot be matched; the original OCR
// observations remain persisted separately for audit/review.
class DubbingTranscriptFusionService final
{
public:
    // `prefer-stt` is the safe default for unmatched or conflicting sources.
    // `ask` remains available when a UI explicitly requests a review gate.
    static QString normalizePolicy(const QString &policy);
    static QVariantList normalizeOcrSegments(const QVariantList &ocrSegments);
    static QVariantList fuse(const QVariantList &sttSegments,
                             const QVariantList &ocrSegments,
                             const QString &policy = QStringLiteral("prefer-stt"));
};

} // namespace LAStudio
