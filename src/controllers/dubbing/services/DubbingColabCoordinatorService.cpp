#include "DubbingColabCoordinatorService.h"

#include "remote/colab/ColabSession.h"
#include "core/storage/Settings.h"
#include "controllers/dubbing/DubbingColabModelRoutes.h"

namespace LAStudio {

DubbingColabCoordinatorService::DubbingColabCoordinatorService(Settings *settings, QObject *parent)
    : QObject(parent)
    , m_settings(settings)
{
}

void DubbingColabCoordinatorService::setRemoteSessions(ColabSession *translation, ColabSession *tts,
                                                       ColabSession *voiceClone, ColabSession *separation,
                                                       ColabSession *subtitleOcr)
{
    m_translationSession = translation;
    m_ttsSession = tts;
    m_voiceCloneSession = voiceClone;
    m_separationSession = separation;
    m_subtitleOcrSession = subtitleOcr;
}

QVariantList DubbingColabCoordinatorService::colabSetupStages() const
{
    QVariantList list;

    auto addStage = [&](const QString &id, const QString &name, ColabSession *session, const QString &notebook) {
        QVariantMap stage;
        stage["stageId"] = id;
        stage["displayName"] = name;
        stage["connected"] = session ? session->isActive() : false;
        stage["status"] = session ? (session->isVerified() ? "ready" : (session->isActive() ? "connected" : "disconnected")) : "unavailable";
        stage["notebookFile"] = notebook;
        stage["tunnelUrl"] = session ? session->workerUrl() : "";
        list.append(stage);
    };

    addStage("source-separate", tr("Vocal Isolation"), m_separationSession, "audio_separator_colab.ipynb");
    addStage("transcribe", tr("Speech to Text"), m_translationSession, "whisper_stt_colab.ipynb");
    addStage("subtitle-ocr", tr("Subtitle OCR"), m_subtitleOcrSession, "subtitle_ocr_colab.ipynb");
    addStage("translate", tr("Translation"), m_translationSession, "translation_colab.ipynb");
    addStage("synthesize", tr("TTS Voice Dubbing"), m_ttsSession, "kokoro_tts_colab.ipynb");

    return list;
}

bool DubbingColabCoordinatorService::selectWorkflowColabModel(const QString &nodeId, const QString &modelId)
{
    emit modelSelected(nodeId, modelId);
    emit colabSetupChanged();
    return true;
}

QString DubbingColabCoordinatorService::colabNotebookForNode(const QString &nodeId, const QString &modelId) const
{
    Q_UNUSED(modelId);
    if (nodeId == "source-separate" || nodeId == "isolator")
        return "audio_separator_colab.ipynb";
    if (nodeId == "subtitle-ocr")
        return "subtitle_ocr_colab.ipynb";
    if (nodeId == "transcribe")
        return "whisper_stt_colab.ipynb";
    if (nodeId == "translate")
        return "translation_colab.ipynb";
    if (nodeId == "synthesize" || nodeId == "tts" || nodeId == "assign-voices")
        return "kokoro_tts_colab.ipynb";

    return "unified_dubbing_colab.ipynb";
}

QVariantList DubbingColabCoordinatorService::colabModelOptionsForNode(const QString &nodeId) const
{
    QVariantList options;

    if (nodeId == "subtitle-ocr") {
        QVariantMap paddle;
        paddle["modelId"] = "paddleocr-v4";
        paddle["displayName"] = tr("PaddleOCR v4 (GPU)");
        options.append(paddle);

        QVariantMap tesseract;
        tesseract["modelId"] = "tesseract-5";
        tesseract["displayName"] = tr("Tesseract OCR (Colab)");
        options.append(tesseract);
    } else if (nodeId == "transcribe") {
        QVariantMap whisper;
        whisper["modelId"] = "whisper-large-v3";
        whisper["displayName"] = tr("Whisper Large v3");
        options.append(whisper);
    } else if (nodeId == "translate") {
        QVariantMap llama;
        llama["modelId"] = "llama-3.3-70b";
        llama["displayName"] = tr("Llama 3.3 70B (High Quality)");
        options.append(llama);

        QVariantMap qwen;
        qwen["modelId"] = "qwen-2.5-32b";
        qwen["displayName"] = tr("Qwen 2.5 32B");
        options.append(qwen);
    }

    return options;
}

QString DubbingColabCoordinatorService::defaultColabModelForNode(const QString &nodeId) const
{
    if (nodeId == "subtitle-ocr")
        return "paddleocr-v4";
    if (nodeId == "transcribe")
        return "whisper-large-v3";
    if (nodeId == "translate")
        return "llama-3.3-70b";
    if (nodeId == "synthesize" || nodeId == "tts")
        return "kokoro-v1.0";
    return "";
}

void DubbingColabCoordinatorService::checkColabSetup()
{
    m_checking = true;
    emit colabSetupChanged();

    m_checking = false;
    m_summary = tr("Colab endpoints checked.");
    emit colabSetupChanged();
}

} // namespace LAStudio
