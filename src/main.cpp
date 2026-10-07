// PFV - Peel File Versions. The GUI is C++; the PFV library runs in the
// embedded Python (pfv-public/, called through pfv_app.py).
//
//   PFV [storage_path] [work_tree]   open work_tree (and that storage) at startup
//   PFV --python-check               start Python, print what's loaded, exit
//   PFV --license-check              verify the stored license, exit
//   PFV --sentry-check | --crash     test Sentry

#include "mainwindow.h"
#include "appPython.h"
#include "peel_app_info.h"

#include <QApplication>
#include <QDebug>
#include <QMenu>

#include <cstdio>
#include <memory>

#if PEEL_HAS_LICENSE
#include "licenseConfig.h"
#include "license_manager.h"

static std::unique_ptr<LicenseManager> makeLicenseManager()
{
    return std::make_unique<LicenseManager>(
        QStringLiteral(PEEL_APP_NAME),
        QString::fromUtf8(LicenseConfig::kServerUrl),
        QString::fromUtf8(LicenseConfig::kAppSecret),
        QString::fromUtf8(LicenseConfig::kProductSlug),
        QString::fromUtf8(LicenseConfig::kPublicKeyPem));
}
#endif

#if PEEL_HAS_SENTRY
#include "sentryConsent.h"
#include "startSentry.h"
#include <sentry.h>

// --sentry-check: send one message event, flush, print its id.
static int sentryCheck()
{
    if (!PEEL_SENTRY_DSN[0]) {
        std::fputs("Sentry: no DSN in this build (app.json sentry.dsn)\n", stdout);
        return 1;
    }
    sentry_value_t event = sentry_value_new_message_event(
        SENTRY_LEVEL_INFO, "pfv", "PFV --sentry-check");
    const sentry_uuid_t id = sentry_capture_event(event);
    char text[37];
    sentry_uuid_as_string(&id, text);
    const bool sent = !sentry_uuid_is_nil(&id) && sentry_flush(10000) == 0;
    std::printf("Sentry %s: event %s, release %s, environment %s\n", sent ? "OK" : "FAILED", text,
                PEEL_SENTRY_RELEASE, PEEL_SENTRY_ENVIRONMENT);
    return sent ? 0 : 1;
}
#endif

// The dark theme the PySide GUI used.
static const char *kStyleSheet = R"(
QWidget { background-color: #1e1e1e; color: #d4d4d4; font-size: 12px; }
QMainWindow, QDialog { background-color: #1e1e1e; }
QTreeView, QTableWidget, QHeaderView::section {
    background-color: #252526; color: #d4d4d4; border: 1px solid #3a3a3a; gridline-color: #3a3a3a; }
QTreeView::item:selected, QTableWidget::item:selected { background-color: #094771; color: #ffffff; }
QTreeView::item:hover, QTableWidget::item:hover { background-color: #2a2d2e; }
QHeaderView::section {
    background-color: #2d2d2d; border: none; border-right: 1px solid #3a3a3a;
    border-bottom: 1px solid #3a3a3a; padding: 3px 6px; font-weight: bold; color: #cccccc; }
QPushButton { background-color: #3a3a3a; color: #d4d4d4; border: 1px solid #555; border-radius: 3px; padding: 3px 10px; }
QPushButton:hover { background-color: #4a4a4a; }
QPushButton:pressed { background-color: #2a2a2a; }
QPushButton:checked { background-color: #094771; border-color: #0078d4; color: #fff; }
QComboBox { background-color: #3a3a3a; color: #d4d4d4; border: 1px solid #555; border-radius: 3px; padding: 2px 6px; }
QComboBox QAbstractItemView { background-color: #252526; color: #d4d4d4; selection-background-color: #094771; }
QLineEdit { background-color: #1a1a1a; color: #aaaaaa; border: 1px solid #3a3a3a; border-radius: 3px; padding: 2px 4px; }
QSplitter::handle { background-color: #3a3a3a; width: 1px; }
QScrollBar:vertical, QScrollBar:horizontal { background: #1e1e1e; border: none; width: 10px; height: 10px; }
QScrollBar::handle:vertical, QScrollBar::handle:horizontal { background: #4a4a4a; border-radius: 5px; min-height: 20px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QMenuBar { background-color: #2d2d2d; }
QMenu { background-color: #2d2d2d; color: #d4d4d4; border: 1px solid #3a3a3a; }
QMenu::item:selected { background-color: #094771; }
QProgressDialog, QMessageBox, QInputDialog { background-color: #2d2d2d; }
QLabel { background: transparent; }
)";

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(PEEL_APP_NAME);
    QApplication::setApplicationVersion(PEEL_APP_VERSION);

#if PEEL_HAS_SENTRY
    // Before anything else can crash. With no DSN Sentry stays off. Nothing is
    // sent until the user agrees (asked once, below), except for the explicit
    // test flags --sentry-check and --crash.
    const bool sentryOn = PEEL_SENTRY_DSN[0] != 0;
    const bool sentryTest = app.arguments().contains(QStringLiteral("--sentry-check"))
                         || app.arguments().contains(QStringLiteral("--crash"));
    if (sentryOn)
        startSentry(PEEL_APP_NAME, PEEL_SENTRY_DSN, PEEL_SENTRY_RELEASE, PEEL_SENTRY_ENVIRONMENT,
                    /*requireConsent=*/!sentryTest);
    if (app.arguments().contains(QStringLiteral("--crash")))
        MainWindow::testCrash();
    if (app.arguments().contains(QStringLiteral("--sentry-check"))) {
        const int ret = sentryCheck();
        if (sentryOn)
            stopSentry();
        return ret;
    }
#endif

    // --python-check: start Python, print pfv_startup.check()'s report to
    // stdout and exit (0 = all checks passed) without showing the window.
    const bool pythonCheck = app.arguments().contains(QStringLiteral("--python-check"));

    // Positional arguments, as the PySide GUI took them: [storage_path] [work_tree].
    QStringList positional;
    for (const QString &arg : app.arguments().mid(1))
        if (!arg.startsWith(QLatin1Char('-')))
            positional << arg;
    const QString storageArg = positional.value(0);
    const QString workTreeArg = positional.value(1);

#if PEEL_HAS_LICENSE
    // --license-check: verify the stored license (renewing it if due) without
    // the activation dialog, print the result and exit.
    const bool licenseCheck = app.arguments().contains(QStringLiteral("--license-check"));
    std::unique_ptr<LicenseManager> license;
    // --python-check is a build check and doesn't need a license.
    if (!pythonCheck) {
        license = makeLicenseManager();
        const bool licensed = licenseCheck ? license->ensureLicensedHeadless()
                                           : license->ensureLicensed(/*dialogParent=*/nullptr);
        if (licenseCheck || !licensed) {
            if (licenseCheck) {
                const LicenseIdentity id = license->identity();
                std::printf("License %s (%s)\n  license %s, machine %s, email %s\n",
                            licensed ? "OK" : "FAILED", license->dataDirPath().toUtf8().constData(),
                            qPrintable(id.licenseId), qPrintable(id.machineId),
                            id.email.isEmpty() ? "(not in the license)" : qPrintable(id.email));
            }
#if PEEL_HAS_SENTRY
            if (sentryOn)
                stopSentry();
#endif
            return licensed ? 0 : 1;
        }
#if PEEL_HAS_SENTRY
        // Crash reports say which license and machine they came from.
        if (sentryOn) {
            const LicenseIdentity id = license->identity();
            sentryUser(id.email.toUtf8().constData(), nullptr, id.machineId.toUtf8().constData());
            sentryTag("license", id.licenseId.toUtf8().constData());
        }
#endif
    }
#endif

#if PEEL_HAS_SENTRY
    // First GUI start: ask whether to send crash reports.
    if (sentryOn && !pythonCheck)
        askSentryConsentIfUnknown(nullptr, QStringLiteral(PEEL_APP_NAME));
#endif

    app.setStyleSheet(QString::fromLatin1(kStyleSheet));

    int ret = 0;
    {
        MainWindow window;
#if PEEL_HAS_LICENSE
        if (license) {
            if (QMenu *help = window.findChild<QMenu *>(QStringLiteral("helpMenu")))
                help->addAction(QObject::tr("License..."), &window,
                                [&] { license->showLicenseInfo(&window); });
        }
#endif
#if PEEL_HAS_SENTRY
        if (sentryOn) {
            if (QMenu *help = window.findChild<QMenu *>(QStringLiteral("helpMenu")))
                addSentryConsentAction(help);
        }
#endif
#if PEEL_HAS_PYTHON
        QString error;
        const bool pythonOk = appPythonStart(&window, &error);
        if (!pythonOk) {
            qWarning().noquote() << "Python:" << error;
            window.appendLog(QStringLiteral("Python failed to start - PFV can't work without it:\n") + error);
        }
        if (pythonCheck) {
            QString report = error;
            ret = (pythonOk && appPythonCheck(&report)) ? 0 : 1;
            std::fputs(report.toUtf8().constData(), stdout);
            std::fputs("\n", stdout);
        }
#else
        window.appendLog(QStringLiteral("This build has no embedded Python, so no PFV backend "
                                        "(build with a qtbld preset)"));
        if (pythonCheck) {
            std::fputs("This build has no embedded Python\n", stdout);
            ret = 1;
        }
#endif
        if (!pythonCheck) {
#if PEEL_HAS_PYTHON
            if (pythonOk)
                window.startup(workTreeArg, storageArg);
#endif
            window.show();
            ret = app.exec();
        }
        // ~MainWindow waits for background backend calls before Python stops.
    }
#if PEEL_HAS_PYTHON
    // After the window (and everything Python connected to) is gone: PySide
    // runs code when a QObject it wrapped or connected to is destroyed.
    appPythonStop();
#endif
#if PEEL_HAS_SENTRY
    if (sentryOn)
        stopSentry();
#endif
    return ret;
}
