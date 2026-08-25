#include "DubbingTranscriptService.h"

namespace LAStudio {

DubbingTranscriptService::DubbingTranscriptService(DubbingProject *project, QObject *parent)
    : QObject(parent)
    , m_project(project)
{
    refreshConflictCount();
}

void DubbingTranscriptService::setProject(DubbingProject *project)
{
    m_project = project;
    refreshConflictCount();
    emit segmentsChanged();
    emit transcriptConfigurationChanged();
}

QVariantList DubbingTranscriptService::segments() const
{
    return m_project ? m_project->segments : QVariantList();
}

void DubbingTranscriptService::setSegments(const QVariantList &segs)
{
    if (m_project) {
        m_project->segments = segs;
        refreshConflictCount();
        emit segmentsChanged();
    }
}

bool DubbingTranscriptService::updateSegment(int index, const QVariantMap &data)
{
    if (!m_project || index < 0 || index >= m_project->segments.size())
        return false;

    QVariantMap seg = m_project->segments[index].toMap();
    for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
        seg[it.key()] = it.value();
    }

    m_project->segments[index] = seg;
    refreshConflictCount();
    emit segmentsChanged();
    return true;
}

bool DubbingTranscriptService::removeSegment(int index)
{
    if (!m_project || index < 0 || index >= m_project->segments.size())
        return false;

    m_project->segments.removeAt(index);
    refreshConflictCount();
    emit segmentsChanged();
    return true;
}

bool DubbingTranscriptService::reorderSegments(int fromIndex, int toIndex)
{
    if (!m_project || fromIndex < 0 || fromIndex >= m_project->segments.size() ||
        toIndex < 0 || toIndex >= m_project->segments.size() || fromIndex == toIndex)
        return false;

    m_project->segments.move(fromIndex, toIndex);
    emit segmentsChanged();
    return true;
}

QVariantMap DubbingTranscriptService::transcriptConfiguration() const
{
    return m_project ? m_project->transcriptConfiguration : QVariantMap();
}

void DubbingTranscriptService::setTranscriptConfiguration(const QVariantMap &config)
{
    if (m_project) {
        m_project->transcriptConfiguration = config;
        emit transcriptConfigurationChanged();
    }
}

void DubbingTranscriptService::setTranscriptFusionPolicy(const QString &policy)
{
    if (!m_project) return;
    m_project->transcriptConfiguration["fusionPolicy"] = policy;
    emit transcriptConfigurationChanged();
}

int DubbingTranscriptService::unresolvedTranscriptConflictCount() const
{
    return m_unresolvedConflictCount;
}

void DubbingTranscriptService::refreshConflictCount()
{
    if (!m_project) {
        m_unresolvedConflictCount = 0;
        return;
    }

    int count = 0;
    for (const QVariant &v : m_project->segments) {
        const QVariantMap seg = v.toMap();
        if (seg.value("fusionStatus").toString() == "conflict") {
            count++;
        }
    }
    m_unresolvedConflictCount = count;
}

bool DubbingTranscriptService::resolveTranscriptConflict(int index, const QString &source)
{
    if (!m_project || index < 0 || index >= m_project->segments.size())
        return false;

    QVariantMap seg = m_project->segments[index].toMap();
    if (seg.value("fusionStatus").toString() != "conflict")
        return false;

    if (source == "stt") {
        seg["sourceText"] = seg.value("fusionSttText").toString();
        seg["fusionStatus"] = "resolved-stt";
    } else if (source == "ocr") {
        seg["sourceText"] = seg.value("fusionOcrText").toString();
        seg["fusionStatus"] = "resolved-ocr";
    } else {
        return false;
    }

    m_project->segments[index] = seg;
    refreshConflictCount();
    emit segmentsChanged();
    return true;
}

bool DubbingTranscriptService::resolveAllTranscriptConflicts(const QString &source)
{
    if (!m_project) return false;

    bool changed = false;
    for (int i = 0; i < m_project->segments.size(); ++i) {
        QVariantMap seg = m_project->segments[i].toMap();
        if (seg.value("fusionStatus").toString() == "conflict") {
            if (source == "stt") {
                seg["sourceText"] = seg.value("fusionSttText").toString();
                seg["fusionStatus"] = "resolved-stt";
                m_project->segments[i] = seg;
                changed = true;
            } else if (source == "ocr") {
                seg["sourceText"] = seg.value("fusionOcrText").toString();
                seg["fusionStatus"] = "resolved-ocr";
                m_project->segments[i] = seg;
                changed = true;
            }
        }
    }

    if (changed) {
        refreshConflictCount();
        emit segmentsChanged();
    }
    return changed;
}

QVariantMap DubbingTranscriptService::transcriptConflictAiAvailability() const
{
    QVariantMap result;
    result["available"] = true;
    result["provider"] = "local";
    return result;
}

bool DubbingTranscriptService::requestTranscriptConflictAiSuggestion(int segmentIndex)
{
    emit aiSuggestionRequested(segmentIndex);
    return true;
}

bool DubbingTranscriptService::acceptTranscriptConflictAiSuggestion(int segmentIndex)
{
    if (!m_project || segmentIndex < 0 || segmentIndex >= m_project->segments.size())
        return false;

    QVariantMap seg = m_project->segments[segmentIndex].toMap();
    const QString suggestion = seg.value("fusionAiSuggestion").toString();

    if (!suggestion.isEmpty()) {
        seg["sourceText"] = suggestion;
        seg["fusionStatus"] = "resolved-ai";
        seg["fusionAiSuggestionStatus"] = "accepted";
        m_project->segments[segmentIndex] = seg;
        refreshConflictCount();
        emit segmentsChanged();
        return true;
    }
    return false;
}

bool DubbingTranscriptService::rejectTranscriptConflictAiSuggestion(int segmentIndex)
{
    if (!m_project || segmentIndex < 0 || segmentIndex >= m_project->segments.size())
        return false;

    QVariantMap seg = m_project->segments[segmentIndex].toMap();
    seg["fusionAiSuggestionStatus"] = "rejected";
    m_project->segments[segmentIndex] = seg;
    emit segmentsChanged();
    return true;
}

} // namespace LAStudio
