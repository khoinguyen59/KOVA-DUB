#pragma once

#include <QObject>
#include <QThread>

namespace LAStudio {

// Worker objects in this application are deliberately unparented before
// moveToThread().  A native/network backend can still be unwinding when its
// owner is destroyed, so joining a QThread forever from the GUI thread turns
// a transient worker failure into an application hang.  Request a normal
// interruption, wait a finite grace period, then detach only the QThread
// object.  The queued work owns no controller state and will finish/delete
// itself; Windows reclaims anything still active when the process exits.
inline bool stopOrDetachWorkerThread(QThread *&thread, int gracefulWaitMs = 2000)
{
    if (!thread) return true;
    thread->requestInterruption();
    thread->quit();
    if (thread->wait(gracefulWaitMs)) {
        thread->setParent(nullptr);
        delete thread;
        thread = nullptr;
        return true;
    }

    thread->setParent(nullptr);
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater,
                     Qt::UniqueConnection);
    thread = nullptr;
    return false;
}

} // namespace LAStudio
