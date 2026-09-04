#include "SourceSeparationService.h"
#include "SeparationWorker.h"
#include "core/utils/Logger.h"
#include <QMetaObject>

namespace LAStudio {

namespace {

// sherpa-onnx performs inference inside one native C call. The call checks no
// Qt interruption flag and therefore cannot be forcefully joined safely from
// the UI thread. Give normal cancellation a short graceful window, then
// detach the still-running worker so application shutdown never waits forever.
constexpr int kGracefulShutdownWaitMs = 1000;

}

SourceSeparationService::SourceSeparationService(QObject *parent)
    : QObject(parent)
    , m_factory(std::make_shared<SeparationBackendFactory>())
{
    init();
}

SourceSeparationService::SourceSeparationService(std::shared_ptr<SeparationBackendFactory> factory, QObject *parent)
    : QObject(parent)
    , m_factory(factory)
{
    init();
}

SourceSeparationService::~SourceSeparationService()
{
    cancel();
    if (!m_thread) {
        return;
    }

    m_thread->requestInterruption();
    m_thread->quit();
    if (m_thread->wait(kGracefulShutdownWaitMs)) {
        // The worker is no longer executing, so deleting it here is safe. The
        // thread object is parented to this service and is also safe to delete
        // after wait() has returned.
        delete m_worker;
        m_worker = nullptr;
        delete m_thread;
        m_thread = nullptr;
        return;
    }

    // Never terminate a third-party native inference stack: doing so while a
    // DLL owns internal buffers can corrupt the process. Detach both objects;
    // the thread will finish after its native call returns, delete its worker,
    // and self-delete. The application can close immediately and the OS will
    // reclaim any remaining work when the process exits.
    Logger::warning(QStringLiteral("SourceSeparation"), QStringLiteral(
        "Source separation did not stop within %1 ms during shutdown; detaching the native worker to keep UI shutdown bounded.")
            .arg(kGracefulShutdownWaitMs));
    if (m_worker) {
        QObject::disconnect(m_worker, nullptr, this, nullptr);
        QObject::connect(m_thread, &QThread::finished,
                         m_worker, &QObject::deleteLater, Qt::UniqueConnection);
    }
    m_thread->setParent(nullptr);
    m_thread = nullptr;
    m_worker = nullptr;
}

void SourceSeparationService::init()
{
    static bool registered = false;
    if (!registered) {
        qRegisterMetaType<SeparationConfiguration>();
        qRegisterMetaType<SeparationRequest>();
        qRegisterMetaType<SeparationStem>();
        qRegisterMetaType<SeparationResult>();
        qRegisterMetaType<SeparationErrorCode>();
        registered = true;
    }

    m_thread = new QThread(this);
    m_worker = new SeparationWorker(m_factory, nullptr);
    m_worker->moveToThread(m_thread);

    connect(m_worker, &SeparationWorker::progress, this, &SourceSeparationService::progress);
    connect(m_worker, &SeparationWorker::finished, this, &SourceSeparationService::onWorkerFinished);

    // This handles the bounded-shutdown detach path. On the normal shutdown
    // path the service deletes the worker after wait() returns.
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);
}

bool SourceSeparationService::isolate(const SeparationRequest &request, QString *error)
{
    if (m_processing) {
        if (error) {
            *error = QStringLiteral("Busy");
        }
        return false;
    }

    m_processing = true;
    m_cancelRequested->storeRelease(0);

    if (!m_thread->isRunning()) {
        // Keep native inference from starving the GUI event loop on machines
        // where the runtime creates additional worker threads of its own.
        m_thread->start(QThread::LowPriority);
    }
    if (!m_thread->isRunning()) {
        m_processing = false;
        if (error) {
            *error = QStringLiteral("Could not start the source-separation worker thread.");
        }
        return false;
    }

    SeparationWorker *worker = m_worker;
    const std::shared_ptr<QAtomicInt> cancellationFlag = m_cancelRequested;
    const bool queued = QMetaObject::invokeMethod(worker,
                              [worker, request, cancellationFlag]() {
        worker->process(request, cancellationFlag);
    }, Qt::QueuedConnection);
    if (!queued) {
        m_processing = false;
        if (error) {
            *error = QStringLiteral("Could not schedule the source-separation worker.");
        }
        return false;
    }
    return true;
}

void SourceSeparationService::cancel()
{
    m_cancelRequested->storeRelease(1);
}

void SourceSeparationService::onWorkerFinished(const SeparationResult &result)
{
    m_processing = false;
    if (m_cancelRequested->loadAcquire()) {
        SeparationResult cancelledResult = result;
        cancelledResult.success = false;
        cancelledResult.errorCode = SeparationErrorCode::Cancelled;
        cancelledResult.error = QStringLiteral("Source separation cancelled.");
        emit finished(cancelledResult);
    } else {
        emit finished(result);
    }
}

} // namespace LAStudio
