#include "pfvBackend.h"
#include "peel_app_info.h"

#include <QJsonDocument>
#include <QMetaObject>
#include <QThreadPool>

#include <memory>

#if PEEL_HAS_PYTHON
#pragma push_macro("slots")
#undef slots
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#pragma pop_macro("slots")
#endif

namespace {

// Our own pool: waitForAll() mustn't wait for unrelated global-pool work.
QThreadPool *pool()
{
    static QThreadPool *p = [] {
        auto *tp = new QThreadPool;
        tp->setMaxThreadCount(4);
        return tp;
    }();
    return p;
}

PfvResult failure(const QString &error, const QString &traceback = {})
{
    PfvResult r;
    r.error = error;
    r.traceback = traceback;
    return r;
}

#if PEEL_HAS_PYTHON

// The pending Python exception as text (clears it). Caller holds the GIL.
QString takePythonError()
{
    PyObject *exc = PyErr_GetRaisedException();
    if (!exc)
        return QStringLiteral("unknown Python error");
    QString text;
    if (PyObject *s = PyObject_Str(exc)) {
        text = QString::fromUtf8(PyUnicode_AsUTF8(s));
        Py_DECREF(s);
    }
    if (text.isEmpty())
        text = QString::fromUtf8(Py_TYPE(exc)->tp_name);
    Py_DECREF(exc);
    PyErr_Clear();
    return text;
}

// pfv_app.dispatch(name, args) -> the JSON text it returns.
bool dispatch(const QByteArray &name, const QByteArray &args, QByteArray *reply, QString *error)
{
    if (!Py_IsInitialized()) {
        *error = QStringLiteral("Python isn't running");
        return false;
    }
    const PyGILState_STATE gil = PyGILState_Ensure();
    bool ok = false;
    PyObject *module = PyImport_ImportModule("pfv_app");
    if (!module) {
        *error = QStringLiteral("import pfv_app: ") + takePythonError();
    } else {
        PyObject *result = PyObject_CallMethod(module, "dispatch", "ss", name.constData(), args.constData());
        if (!result) {
            *error = takePythonError();
        } else {
            if (const char *text = PyUnicode_AsUTF8(result)) {
                *reply = QByteArray(text);
                ok = true;
            } else {
                *error = takePythonError();
            }
            Py_DECREF(result);
        }
        Py_DECREF(module);
    }
    PyGILState_Release(gil);
    return ok;
}

#endif

} // namespace

namespace PfvBackend {

PfvResult call(const QString &name, const QJsonObject &args)
{
#if PEEL_HAS_PYTHON
    QByteArray reply;
    QString error;
    if (!dispatch(name.toUtf8(), QJsonDocument(args).toJson(QJsonDocument::Compact), &reply, &error))
        return failure(QStringLiteral("%1: %2").arg(name, error));

    QJsonParseError parseError;
    const QJsonObject obj = QJsonDocument::fromJson(reply, &parseError).object();
    if (parseError.error != QJsonParseError::NoError)
        return failure(QStringLiteral("%1: bad reply from pfv_app (%2)").arg(name, parseError.errorString()));

    PfvResult r;
    r.ok = obj.value(QStringLiteral("ok")).toBool();
    r.value = obj.value(QStringLiteral("result"));
    r.error = obj.value(QStringLiteral("error")).toString();
    r.traceback = obj.value(QStringLiteral("traceback")).toString();
    return r;
#else
    Q_UNUSED(args);
    return failure(QStringLiteral("%1: this build has no embedded Python, so no PFV backend").arg(name));
#endif
}

void runAsync(QObject *context, std::function<void()> work, std::function<void()> done)
{
    QPointer<QObject> ctx(context);
    pool()->start([ctx, work = std::move(work), done = std::move(done)] {
        work();
        if (ctx)
            QMetaObject::invokeMethod(ctx.data(), done, Qt::QueuedConnection);
    });
}

void callAsync(const QString &name, const QJsonObject &args, QObject *context,
               std::function<void(const PfvResult &)> done)
{
    auto result = std::make_shared<PfvResult>();
    runAsync(context, [=] { *result = call(name, args); }, [=] { done(*result); });
}

void waitForAll()
{
    pool()->waitForDone();
}

} // namespace PfvBackend
