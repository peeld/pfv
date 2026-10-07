#pragma once

// The C++ side of python/pfv_app.py: every PFV operation the GUI does
// goes through pfv_app.dispatch(name, args_json) in the embedded Python, with
// JSON in and out, so C++ never holds a Python object.
//
// call() works from any thread (it takes the GIL), so slow operations run on
// a QThreadPool thread with callAsync(). Without embedded Python
// (PEEL_HAS_PYTHON 0, e.g. official Qt) every call fails with a message.

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPointer>
#include <QString>

#include <functional>

struct PfvResult
{
    bool ok = false;
    QJsonValue value;  // the command's result when ok
    QString error;     // one-line message when not ok
    QString traceback; // Python traceback, when there is one

    QJsonObject object() const { return value.toObject(); }
    QJsonArray array() const { return value.toArray(); }
};

namespace PfvBackend {

// pfv_app.<name>(**args), on the calling thread.
PfvResult call(const QString &name, const QJsonObject &args = {});

// call() on a thread-pool thread; done(result) then runs on context's thread,
// unless context has been deleted by then.
void callAsync(const QString &name, const QJsonObject &args, QObject *context,
               std::function<void(const PfvResult &)> done);

// Runs work() on a thread-pool thread (for a sequence of call()s);
// done() then runs on context's thread, unless context is gone.
void runAsync(QObject *context, std::function<void()> work, std::function<void()> done);

// Wait for every callAsync()/runAsync() to finish: before Python shuts down.
void waitForAll();

} // namespace PfvBackend
