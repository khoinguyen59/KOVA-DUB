#pragma once

#include <QVariantList>

namespace LAStudio {

// Deterministic STT/OCR reconciliation. It never calls a model. OCR is the
// canonical subtitle source by default; STT observations remain persisted as
// timing/speech evidence for audit and review.
class DubbingTranscriptFusionService final
{
public:
    // `prefer-ocr` is the default for subtitle-led dubbing projects.
    // `ask` remains available when a UI explicitly requests a review gate.
    static QString normalizePolicy(const QString &policy);
    static QVariantList normalizeOcrSegments(const QVariantList &ocrSegments);
    static QVariantList fuse(const QVariantList &sttSegments,
                             const QVariantList &ocrSegments,
                             const QString &policy = QStringLiteral("prefer-ocr"));
};

} // namespace LAStudio
