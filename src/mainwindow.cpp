#include "mainwindow.h"
#include "dialogs.h"
#include "fileTreeModel.h"
#include "peel_app_info.h"
#include "pfvBackend.h"
#include "portable.h"

#include "scriptwidget.h"

#include <QApplication>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QTableWidget>
#include <QTreeView>
#include <QVBoxLayout>

#include <memory>

namespace {

// Window layout and the script editor's session: %APPDATA%/<app>/<app>.ini,
// or portable.txt next to the exe when that file is there and writable.
class AppSettings : public QSettings
{
public:
    AppSettings()
        : QSettings(peel::settingsPath(QStringLiteral(PEEL_APP_NAME), QStringLiteral(PEEL_APP_NAME)),
                    QSettings::IniFormat)
    {}
};

// Shows a wait cursor while in scope (for synchronous backend calls).
struct BusyCursor
{
    BusyCursor() { QApplication::setOverrideCursor(Qt::WaitCursor); }
    ~BusyCursor() { QApplication::restoreOverrideCursor(); }
};

QWidget *stepPanel(const QString &header, QVBoxLayout **body, QWidget *parent)
{
    auto *panel = new QWidget(parent);
    panel->setObjectName(QStringLiteral("stepPanel"));
    panel->setStyleSheet(QStringLiteral(
        "QWidget#stepPanel { background: #2a2a2a; border: 1px solid #3a3a3a; border-radius: 4px; }"));
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(4);
    auto *label = new QLabel(header, panel);
    label->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 11px; color: #9cdcfe;"));
    layout->addWidget(label);
    *body = layout;
    return panel;
}

QLineEdit *pathDisplay(QWidget *parent)
{
    auto *edit = new QLineEdit(parent);
    edit->setReadOnly(true);
    edit->setStyleSheet(QStringLiteral("font-size: 11px; padding: 2px 4px; color: #aaa; background: #1a1a1a;"
                                       " border: 1px solid #3a3a3a; border-radius: 3px;"));
    return edit;
}

QJsonObject args(std::initializer_list<std::pair<const char *, QJsonValue>> list)
{
    QJsonObject o;
    for (const auto &[key, value] : list)
        o.insert(QLatin1String(key), value);
    return o;
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral(PEEL_APP_NAME " " PEEL_APP_VERSION " - Peel File Versions"));
    resize(1400, 900);
    setupUi();

    AppSettings settings;
    QMenu *file = menuBar()->addMenu(tr("&File"));
    file->addAction(tr("&Open Workspace..."), this, &MainWindow::openExistingWorkspace);
    file->addAction(tr("&Refresh"), QKeySequence::Refresh, this, &MainWindow::refresh);
    file->addSeparator();
    file->addAction(tr("E&xit"), QKeySequence::Quit, this, &QWidget::close);
    addScriptEditor(&settings);
    restoreGeometry(settings.value(QStringLiteral("geometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("windowState")).toByteArray());

    // main.cpp adds to it too (License..., Send crash reports), finding it by name.
    QMenu *help = menuBar()->addMenu(tr("&Help"));
    help->setObjectName(QStringLiteral("helpMenu"));
    help->addAction(tr("About %1").arg(QStringLiteral(PEEL_APP_NAME)), this, [this] {
        QMessageBox::about(this, QStringLiteral(PEEL_APP_NAME),
                           QStringLiteral("%1 %2\nQt %3 (%4)").arg(PEEL_APP_NAME, PEEL_APP_VERSION, qVersion(), PEEL_QT_KIND));
    });
#if PEEL_HAS_SENTRY
    help->addAction(tr("Test crash (Sentry)"), this, &MainWindow::testCrash);
#endif
}

MainWindow::~MainWindow()
{
    // Background calls post back to this window: let them finish first.
    PfvBackend::waitForAll();
}

void MainWindow::setupUi()
{
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *main = new QVBoxLayout(central);
    main->setContentsMargins(10, 10, 10, 10);
    main->setSpacing(6);

    // ── Step 1: workspace ────────────────────────────────────────────
    QVBoxLayout *wsBody = nullptr;
    main->addWidget(stepPanel(tr("1  Workspace  - select an existing workspace, convert a directory, or create a new one"),
                              &wsBody, central));
    auto *wsRow = new QHBoxLayout;
    m_workspaceCombo = new QComboBox(central);
    m_workspaceCombo->setMinimumWidth(200);
    m_workspaceCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_workspaceCombo->setToolTip(tr("Recently opened workspaces"));
    connect(m_workspaceCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        const QString path = m_workspaceCombo->itemData(index).toString();
        if (!path.isEmpty() && QDir(path) != QDir(m_workTree))
            openWorkspace(path);
    });
    wsRow->addWidget(m_workspaceCombo);
    const auto button = [central](const QString &text, const QString &tip, auto receiver, auto slot) {
        auto *b = new QPushButton(text, central);
        b->setToolTip(tip);
        QObject::connect(b, &QPushButton::clicked, receiver, slot);
        return b;
    };
    wsRow->addWidget(button(tr("Open..."), tr("Open an existing PFV workspace directory"), this,
                            &MainWindow::openExistingWorkspace));
    wsRow->addWidget(button(tr("Convert..."), tr("Turn an existing directory into a PFV workspace"), this,
                            &MainWindow::convertDirectoryToWorkspace));
    wsRow->addWidget(button(tr("New..."), tr("Create a new blank workspace directory"), this,
                            &MainWindow::newBlankWorkspace));
    wsRow->addStretch();
    wsBody->addLayout(wsRow);
    m_workTreeEdit = pathDisplay(central);
    wsBody->addWidget(m_workTreeEdit);

    // ── Step 2: repo links ───────────────────────────────────────────
    QVBoxLayout *repoBody = nullptr;
    main->addWidget(stepPanel(tr("2  Repo Links  - pick a linked repo or add a new link for this workspace"),
                              &repoBody, central));
    auto *repoRow = new QHBoxLayout;
    m_repoCombo = new QComboBox(central);
    m_repoCombo->setMinimumWidth(160);
    m_repoCombo->setToolTip(tr("Repo links defined for this workspace"));
    connect(m_repoCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
        const QString name = m_repoCombo->itemData(index).toString();
        if (!name.isEmpty())
            connectRepo(name);
    });
    repoRow->addWidget(m_repoCombo);
    auto *addLink = new QPushButton(tr("+ Add Link ▾"), central);
    auto *addLinkMenu = new QMenu(addLink);
    addLinkMenu->addAction(tr("Link existing local repo..."), this, &MainWindow::linkExistingRepo);
    addLinkMenu->addAction(tr("Create new local repo..."), this, &MainWindow::newLocalRepo);
    addLinkMenu->addAction(tr("Link remote S3 repo..."), this, &MainWindow::linkS3Repo);
    addLink->setMenu(addLinkMenu);
    repoRow->addWidget(addLink);
    repoRow->addWidget(button(tr("Remove"), tr("Remove this repo link from the workspace (files not deleted)"), this,
                              &MainWindow::removeRepoLink));
    repoRow->addStretch();
    repoBody->addLayout(repoRow);
    m_storageEdit = pathDisplay(central);
    repoBody->addWidget(m_storageEdit);

    // ── Toolbar ──────────────────────────────────────────────────────
    auto *toolbar = new QHBoxLayout;
    toolbar->addStretch();
    toolbar->addWidget(button(tr("Refresh"), tr("Reload the file list (F5)"), this, &MainWindow::refresh));
    auto *sync = button(tr("🔄 Sync"), tr("Check in every modified tracked file"), this, &MainWindow::syncFiles);
    sync->setStyleSheet(QStringLiteral("background-color: #2e7d32; color: #e0e0e0; font-weight: bold;"));
    toolbar->addWidget(sync);
    main->addLayout(toolbar);

    // ── File tree | version history ──────────────────────────────────
    auto *left = new QWidget(central);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    auto *toggleRow = new QHBoxLayout;
    m_repoViewButton = new QPushButton(tr("Repo View"), left);
    m_workspaceViewButton = new QPushButton(tr("Workspace View"), left);
    for (QPushButton *b : {m_repoViewButton, m_workspaceViewButton}) {
        b->setCheckable(true);
        b->setFixedHeight(24);
        toggleRow->addWidget(b);
    }
    m_repoViewButton->setChecked(true);
    connect(m_repoViewButton, &QPushButton::clicked, this, [this] { setView(View::Repo); });
    connect(m_workspaceViewButton, &QPushButton::clicked, this, [this] { setView(View::Workspace); });
    toggleRow->addStretch();
    leftLayout->addLayout(toggleRow);
    m_tree = new QTreeView(left);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeView::customContextMenuRequested, this, &MainWindow::treeContextMenu);
    leftLayout->addWidget(m_tree);

    auto *right = new QWidget(central);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    auto *historyLabel = new QLabel(tr("Version History"), right);
    historyLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 12px; color: #d4d4d4;"));
    rightLayout->addWidget(historyLabel);
    m_history = new QTableWidget(0, 6, right);
    m_history->setHorizontalHeaderLabels({tr("Version"), tr("Type"), tr("Locked"), tr("Deleted"), tr("Tag"), tr("Meta")});
    m_history->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_history->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_history->verticalHeader()->hide();
    m_history->horizontalHeader()->setStretchLastSection(true);
    const int widths[] = {60, 60, 50, 50, 100};
    for (int col = 0; col < 5; ++col)
        m_history->setColumnWidth(col, widths[col]);
    rightLayout->addWidget(m_history);

    auto *actions = new QHBoxLayout;
    actions->addWidget(button(tr("Checkout"), tr("Check out a version of the selected file"), this,
                              &MainWindow::checkoutFile));
    actions->addWidget(button(tr("Abandon Checkout"), tr("Forget the selected file's checkouts without checking in"),
                              this, &MainWindow::abandonCheckout));
    actions->addSpacing(10);
    auto *del = button(tr("🗑️ Delete"), tr("Mark the selected file deleted (history is kept)"), this,
                       &MainWindow::deleteFile);
    del->setStyleSheet(QStringLiteral("background-color: #7f1f1f; color: #ffcdd2;"));
    actions->addWidget(del);
    auto *ren = button(tr("✏️ Rename"), tr("Mark the selected file renamed"), this, &MainWindow::renameFile);
    ren->setStyleSheet(QStringLiteral("background-color: #1a3a5c; color: #bbdefb;"));
    actions->addWidget(ren);
    actions->addWidget(button(tr("↶ Undelete"), tr("Restore a deleted file"), this, &MainWindow::undeleteFile));
    actions->addStretch();
    rightLayout->addLayout(actions);

    auto *split = new QSplitter(Qt::Horizontal, central);
    split->addWidget(left);
    split->addWidget(right);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 1);

    // ── Log panel ────────────────────────────────────────────────────
    auto *logWidget = new QWidget(central);
    auto *logLayout = new QVBoxLayout(logWidget);
    logLayout->setContentsMargins(0, 2, 0, 0);
    logLayout->setSpacing(2);
    auto *logHeader = new QHBoxLayout;
    auto *logLabel = new QLabel(tr("Log"), logWidget);
    logLabel->setStyleSheet(QStringLiteral("font-weight: bold; font-size: 11px; color: #9cdcfe;"));
    logHeader->addWidget(logLabel);
    logHeader->addStretch();
    m_log = new QPlainTextEdit(logWidget);
    auto *clear = button(tr("Clear"), tr("Clear the log"), m_log, &QPlainTextEdit::clear);
    clear->setStyleSheet(QStringLiteral("font-size: 10px; padding: 1px 4px;"));
    logHeader->addWidget(clear);
    logLayout->addLayout(logHeader);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(500);
    m_log->setStyleSheet(QStringLiteral("font-family: monospace; font-size: 10px; background: #141414;"
                                        " color: #b0b0b0; border: 1px solid #3a3a3a;"));
    logLayout->addWidget(m_log);

    auto *vsplit = new QSplitter(Qt::Vertical, central);
    vsplit->addWidget(split);
    vsplit->addWidget(logWidget);
    vsplit->setStretchFactor(0, 4);
    vsplit->setStretchFactor(1, 1);
    main->addWidget(vsplit, 1);

    m_status = new QLabel(tr("Ready"), central);
    m_status->setStyleSheet(QStringLiteral("color: #888; font-size: 10px;"));
    main->addWidget(m_status);

    setModel(new FileTreeModel(FileTreeModel::Mode::Repo, this));
}

void MainWindow::addScriptEditor(QSettings *settings)
{
    m_scriptWidget = new ScriptWidget(this);
    settings->beginGroup(QStringLiteral("scriptEditor"));
    m_scriptWidget->load(settings);
    settings->endGroup();

    // Floating at first; restoreState() puts it wherever the user left it.
    m_scriptDock = new QDockWidget(tr("Script Editor"), this);
    m_scriptDock->setObjectName(QStringLiteral("scriptEditorDock")); // for saveState()
    m_scriptDock->setWidget(m_scriptWidget);
    addDockWidget(Qt::BottomDockWidgetArea, m_scriptDock);
    m_scriptDock->setFloating(true);
    m_scriptDock->resize(900, 550);
    m_scriptDock->hide();

    QMenu *view = menuBar()->addMenu(tr("&View"));
    view->setObjectName(QStringLiteral("viewMenu"));
    QAction *toggle = m_scriptDock->toggleViewAction();
    toggle->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    view->addAction(toggle);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_scriptWidget->maybeSave()) {
        event->ignore();
        return;
    }
    AppSettings settings;
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    settings.setValue(QStringLiteral("windowState"), saveState());
    settings.setValue(QStringLiteral("workspace"), m_workTree);
    settings.beginGroup(QStringLiteral("scriptEditor"));
    m_scriptWidget->save(&settings);
    settings.endGroup();
    QMainWindow::closeEvent(event);
}

void MainWindow::testCrash()
{
    // volatile, so the compiler can't drop the write.
    volatile int *p = nullptr;
    *p = 42;
}

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------

void MainWindow::log(const QString &message, const QString &level)
{
    const QString time = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
    m_log->appendPlainText(QStringLiteral("[%1] %2 %3").arg(time, level.leftJustified(5), message));
    m_status->setText(QStringLiteral("%1 | %2").arg(time, message.section(QLatin1Char('\n'), 0, 0)));
}

void MainWindow::appendLog(const QString &message)
{
    log(message);
}

QString MainWindow::logText() const
{
    return m_log->toPlainText();
}

void MainWindow::reportError(const QString &title, const PfvResult &result)
{
    log(QStringLiteral("%1: %2").arg(title, result.error), QStringLiteral("ERROR"));
    if (!result.traceback.isEmpty())
        m_log->appendPlainText(result.traceback.trimmed());
    QMessageBox::critical(this, title, result.error);
}

bool MainWindow::requireSelection()
{
    if (m_currentVdir.isEmpty() || m_storage.isEmpty()) {
        QMessageBox::warning(this, tr("Selection Error"), tr("Please select a file first"));
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Workspaces and storage
// ---------------------------------------------------------------------------

QString MainWindow::workTree() const
{
    return m_workTree;
}

QString MainWindow::storageLocation() const
{
    return m_storage;
}

QString MainWindow::currentVdir() const
{
    return m_currentVdir;
}

void MainWindow::startup(const QString &workTree, const QString &storage)
{
    log(QStringLiteral("%1 %2 started").arg(PEEL_APP_NAME, PEEL_APP_VERSION));
    QString path = workTree;
    if (path.isEmpty())
        path = AppSettings().value(QStringLiteral("workspace")).toString();
    if (!path.isEmpty() && QFileInfo(path).isDir())
        openWorkspace(path, storage);
    else
        refreshWorkspaceCombo();
}

void MainWindow::refreshWorkspaceCombo()
{
    const PfvResult r = PfvBackend::call(QStringLiteral("workspaces"));
    if (!r.ok)
        log(r.error, QStringLiteral("WARN"));
    QSignalBlocker block(m_workspaceCombo);
    m_workspaceCombo->clear();
    m_workspaceCombo->addItem(tr("- select workspace -"));
    for (const QJsonValue &v : r.array()) {
        const QJsonObject ws = v.toObject();
        const QString path = ws.value(QStringLiteral("path")).toString();
        m_workspaceCombo->addItem(ws.value(QStringLiteral("name")).toString(), path);
        if (!m_workTree.isEmpty() && QDir(path) == QDir(m_workTree))
            m_workspaceCombo->setCurrentIndex(m_workspaceCombo->count() - 1);
    }
}

void MainWindow::openWorkspace(const QString &path, const QString &storageOverride)
{
    log(tr("Opening workspace: %1").arg(path));
    BusyCursor busy;
    const PfvResult r = PfvBackend::call(QStringLiteral("open_workspace"), args({{"path", path}}));
    if (!r.ok) {
        reportError(tr("Workspace Error"), r);
        return;
    }
    m_workTree = r.object().value(QStringLiteral("path")).toString();
    m_workTreeEdit->setText(m_workTree);
    setStorage({}); // open_workspace disconnects
    refreshWorkspaceCombo();
    refreshRepoCombo();
    if (!storageOverride.isEmpty())
        connectStorage(storageOverride, {});
    else
        connectRepo({});
}

void MainWindow::refreshRepoCombo()
{
    const PfvResult r = PfvBackend::call(QStringLiteral("repos"));
    QSignalBlocker block(m_repoCombo);
    m_repoCombo->clear();
    const QJsonArray repos = r.array();
    for (const QJsonValue &v : repos) {
        const QJsonObject repo = v.toObject();
        const QString name = repo.value(QStringLiteral("name")).toString();
        m_repoCombo->addItem(name, name);
        m_repoCombo->setItemData(m_repoCombo->count() - 1, repo.value(QStringLiteral("storage")).toString(),
                                 Qt::ToolTipRole);
        if (repo.value(QStringLiteral("default")).toBool())
            m_repoCombo->setCurrentIndex(m_repoCombo->count() - 1);
    }
    if (repos.isEmpty())
        m_repoCombo->addItem(tr("(no repos in session)"));
}

void MainWindow::setStorage(const QString &location)
{
    m_storage = location;
    m_storageEdit->setText(location);
}

void MainWindow::connectStorage(const QString &location, const QJsonObject &kwargs)
{
    log(tr("Connecting to storage: %1").arg(location));
    PfvResult r;
    {
        BusyCursor busy;
        r = PfvBackend::call(QStringLiteral("connect"), args({{"location", location}, {"kwargs", kwargs}}));
    }
    if (r.ok) {
        setStorage(location);
        log(tr("Storage opened: %1").arg(location));
    } else {
        setStorage({});
        reportError(tr("Storage Error"), r);
    }
    refresh();
}

void MainWindow::connectRepo(const QString &name)
{
    PfvResult r;
    {
        BusyCursor busy;
        r = PfvBackend::call(QStringLiteral("connect_repo"),
                             name.isEmpty() ? QJsonObject() : args({{"name", name}}));
    }
    if (!r.ok) {
        setStorage({});
        reportError(tr("Repo Error"), r);
    } else {
        const QJsonValue location = r.object().value(QStringLiteral("location"));
        if (location.isNull()) {
            setStorage({});
            log(tr("No repo linked to this workspace yet: add one under step 2"), QStringLiteral("WARN"));
        } else {
            setStorage(location.toString());
            log(tr("Storage opened: %1").arg(m_storage));
        }
    }
    refresh();
}

// ---------------------------------------------------------------------------
// File list
// ---------------------------------------------------------------------------

void MainWindow::setView(View view)
{
    m_view = view;
    m_repoViewButton->setChecked(view == View::Repo);
    m_workspaceViewButton->setChecked(view == View::Workspace);
    refresh();
}

void MainWindow::setModel(FileTreeModel *model)
{
    QAbstractItemModel *old = m_tree->model();
    QItemSelectionModel *oldSelection = m_tree->selectionModel();
    m_tree->setModel(model);
    m_model = model;
    delete oldSelection; // setModel() doesn't
    delete old;
    connect(m_tree->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex &current) { onSelectionChanged(current); });
    m_tree->setColumnWidth(0, 300);
    m_tree->expandToDepth(0);
}

void MainWindow::refresh()
{
    clearHistory();
    m_currentVdir.clear();
    const int request = ++m_listRequest;

    if (m_view == View::Workspace) {
        if (m_workTree.isEmpty()) {
            log(tr("Workspace view: no workspace open"), QStringLiteral("WARN"));
            setModel(new FileTreeModel(FileTreeModel::Mode::Workspace, this));
            return;
        }
        log(tr("Loading workspace view: %1").arg(m_workTree));
        PfvBackend::callAsync(QStringLiteral("workspace_view"), {}, this, [this, request](const PfvResult &r) {
            if (request != m_listRequest)
                return;
            if (!r.ok) {
                reportError(tr("Workspace Error"), r);
                return;
            }
            const QJsonArray files = r.object().value(QStringLiteral("files")).toArray();
            int tracked = 0;
            for (const QJsonValue &f : files)
                tracked += f.toObject().value(QStringLiteral("in_repo")).toBool() ? 1 : 0;
            log(tr("Workspace: %1 file(s) - %2 tracked, %3 untracked")
                    .arg(files.size()).arg(tracked).arg(files.size() - tracked));
            setModel(FileTreeModel::fromWorkspaceView(files, this));
            m_tree->resizeColumnToContents(0);
        });
        return;
    }

    if (m_storage.isEmpty()) {
        log(tr("Repo view: no storage connected"), QStringLiteral("WARN"));
        setModel(new FileTreeModel(FileTreeModel::Mode::Repo, this));
        return;
    }
    log(tr("Loading repo view..."));
    PfvBackend::callAsync(QStringLiteral("repo_view"), {}, this, [this, request](const PfvResult &r) {
        if (request != m_listRequest)
            return;
        if (!r.ok) {
            reportError(tr("Load Error"), r);
            return;
        }
        const QJsonObject result = r.object();
        for (const QJsonValue &e : result.value(QStringLiteral("errors")).toArray()) {
            const QJsonObject err = e.toObject();
            log(tr("Error loading vdir '%1': %2")
                    .arg(err.value(QStringLiteral("vdir")).toString(), err.value(QStringLiteral("error")).toString()),
                QStringLiteral("ERROR"));
        }
        const QJsonArray files = result.value(QStringLiteral("files")).toArray();
        log(tr("Repo view ready (%1 versioned file(s))").arg(files.size()));
        setModel(FileTreeModel::fromRepoView(files, this));
        m_tree->resizeColumnToContents(0);
    });
}

// ---------------------------------------------------------------------------
// History
// ---------------------------------------------------------------------------

void MainWindow::onSelectionChanged(const QModelIndex &current)
{
    m_currentVdir = m_model ? m_model->vdir(current) : QString();
    if (m_currentVdir.isEmpty()) {
        clearHistory(); // folder or untracked workspace file
        return;
    }
    log(tr("Selected: %1").arg(m_currentVdir));
    loadHistory(m_currentVdir);
}

void MainWindow::clearHistory()
{
    ++m_historyRequest;
    m_history->setRowCount(0);
    m_historyVersions.clear();
}

void MainWindow::loadHistory(const QString &vdir)
{
    clearHistory();
    if (m_storage.isEmpty())
        return;
    const int request = m_historyRequest;
    PfvBackend::callAsync(QStringLiteral("history"), args({{"vdir", vdir}}), this, [this, request](const PfvResult &r) {
        if (request != m_historyRequest)
            return;
        if (!r.ok) {
            log(tr("Error loading history: %1").arg(r.error), QStringLiteral("ERROR"));
            return;
        }
        const QJsonObject info = r.object();
        const int latest = info.value(QStringLiteral("latest")).toInt();
        const QJsonArray slotList = info.value(QStringLiteral("slots")).toArray();
        log(tr("Version history: %1 slot(s), latest v%2").arg(slotList.size()).arg(latest));
        m_history->setRowCount(int(slotList.size()));
        for (int row = 0; row < slotList.size(); ++row) {
            const QJsonObject s = slotList[row].toObject();
            const int version = s.value(QStringLiteral("version")).toInt();
            const bool locked = s.value(QStringLiteral("is_locked")).toBool();
            const bool deleted = s.value(QStringLiteral("is_deleted")).toBool();
            const QString meta = s.value(QStringLiteral("has_meta")).toBool()
                ? QStringLiteral("%1: %2").arg(s.value(QStringLiteral("author")).toString(QStringLiteral("?")),
                                               s.value(QStringLiteral("message")).toString().left(50))
                : QString();
            const QStringList values = {QString::number(version), s.value(QStringLiteral("content_type")).toString(),
                                        locked ? tr("YES") : QString(), deleted ? tr("YES") : QString(),
                                        s.value(QStringLiteral("tag")).toString(), meta};
            for (int col = 0; col < values.size(); ++col) {
                auto *cell = new QTableWidgetItem(values[col]);
                if (version == latest)
                    cell->setBackground(QColor(0x1a, 0x33, 0x50)); // dark blue: latest
                if (col == 2 && locked)
                    cell->setBackground(QColor(0x5a, 0x4a, 0x00)); // dark amber: locked
                if (col == 3 && deleted)
                    cell->setBackground(QColor(0x5a, 0x10, 0x10)); // dark red: deleted
                m_history->setItem(row, col, cell);
            }
            m_historyVersions.prepend(QString::number(version));
        }
    });
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

void MainWindow::checkoutFile()
{
    if (!requireSelection())
        return;
    // Default destination: the file's place in the work tree.
    const QString defaultDest = m_workTree.isEmpty()
        ? QString() : QDir::toNativeSeparators(QDir(m_workTree).filePath(m_currentVdir));
    CheckoutDialog dialog(m_currentVdir, m_historyVersions, defaultDest, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString dest = dialog.dest();
    if (dest.isEmpty()) {
        QMessageBox::warning(this, tr("Checkout"), tr("Choose a destination file"));
        return;
    }
    log(tr("Checkout: %1 v%2 -> %3").arg(m_currentVdir, dialog.version(), dest));
    PfvResult r;
    {
        BusyCursor busy;
        r = PfvBackend::call(QStringLiteral("checkout"),
                             args({{"vdir", m_currentVdir}, {"version", dialog.version()}, {"dest", dest}}));
    }
    if (!r.ok) {
        reportError(tr("Checkout Error"), r);
        return;
    }
    log(tr("Checkout complete: %1 (v%2)").arg(r.object().value(QStringLiteral("dest")).toString())
            .arg(r.object().value(QStringLiteral("version")).toInt()));
    QMessageBox::information(this, tr("Success"), tr("File checked out to:\n%1").arg(dest));
    refresh();
}

void MainWindow::abandonCheckout()
{
    if (!requireSelection())
        return;
    const PfvResult r = PfvBackend::call(QStringLiteral("abandon"), args({{"vdir", m_currentVdir}}));
    if (!r.ok) {
        reportError(tr("Error"), r);
        return;
    }
    const QJsonArray dests = r.object().value(QStringLiteral("dests")).toArray();
    if (dests.isEmpty()) {
        QMessageBox::information(this, tr("No Checkouts"), tr("No active checkouts found"));
        return;
    }
    for (const QJsonValue &d : dests)
        log(tr("Abandoned checkout: %1").arg(d.toString()));
    QMessageBox::information(this, tr("Success"), tr("Abandoned %1 checkout(s)").arg(dests.size()));
    refresh();
}

void MainWindow::deleteFile()
{
    if (!requireSelection())
        return;
    DeleteDialog dialog(m_currentVdir, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    log(tr("Marking deleted: %1").arg(m_currentVdir));
    const PfvResult r = PfvBackend::call(
        QStringLiteral("delete"),
        args({{"vdir", m_currentVdir}, {"message", dialog.message()}, {"reason", dialog.reason()}}));
    if (!r.ok) {
        reportError(tr("Delete Error"), r);
        return;
    }
    const int v = r.object().value(QStringLiteral("version")).toInt();
    log(tr("Marked deleted: %1 -> v%2").arg(m_currentVdir).arg(v));
    QMessageBox::information(this, tr("Success"), tr("Marked as deleted - version %1").arg(v));
    refresh();
}

void MainWindow::renameFile()
{
    if (!requireSelection())
        return;
    RenameDialog dialog(m_currentVdir, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    log(tr("Renaming: %1 -> %2").arg(m_currentVdir, dialog.newName()));
    const PfvResult r = PfvBackend::call(
        QStringLiteral("rename"),
        args({{"vdir", m_currentVdir}, {"new_name", dialog.newName()}, {"message", dialog.message()}}));
    if (!r.ok) {
        reportError(tr("Rename Error"), r);
        return;
    }
    const int v = r.object().value(QStringLiteral("version")).toInt();
    log(tr("Renamed: %1 -> %2 (v%3)").arg(m_currentVdir, dialog.newName()).arg(v));
    QMessageBox::information(this, tr("Success"), tr("Renamed to %1 - version %2").arg(dialog.newName()).arg(v));
    refresh();
}

void MainWindow::undeleteFile()
{
    if (!requireSelection())
        return;
    const PfvResult check = PfvBackend::call(QStringLiteral("is_deleted"), args({{"vdir", m_currentVdir}}));
    if (check.ok && !check.object().value(QStringLiteral("deleted")).toBool()) {
        QMessageBox::information(this, tr("Not Deleted"), tr("%1 is not deleted").arg(m_currentVdir));
        return;
    }
    UndeleteDialog dialog(m_currentVdir, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    QJsonObject a = args({{"vdir", m_currentVdir}, {"message", dialog.message()}});
    if (dialog.restoreVersion() > 0)
        a.insert(QStringLiteral("restore_version"), dialog.restoreVersion());
    log(tr("Restoring: %1").arg(m_currentVdir));
    const PfvResult r = PfvBackend::call(QStringLiteral("undelete"), a);
    if (!r.ok) {
        reportError(tr("Undelete Error"), r);
        return;
    }
    const int v = r.object().value(QStringLiteral("version")).toInt();
    log(tr("Restored: %1 -> v%2").arg(m_currentVdir).arg(v));
    QMessageBox::information(this, tr("Success"), tr("Restored - version %1").arg(v));
    refresh();
}

void MainWindow::treeContextMenu(const QPoint &pos)
{
    // Workspace view: untracked files can be committed as new vdirs.
    if (!m_model || m_model->mode() != FileTreeModel::Mode::Workspace)
        return;
    const FileTreeModel::Node *node = m_model->node(m_tree->indexAt(pos));
    if (!node || node->isDir || node->inRepo)
        return;
    QMenu menu(this);
    QAction *commit = menu.addAction(tr("Commit as new file..."));
    if (menu.exec(m_tree->viewport()->mapToGlobal(pos)) == commit)
        commitNewFile(node->relPath);
}

void MainWindow::commitNewFile(const QString &relPath)
{
    if (m_storage.isEmpty()) {
        QMessageBox::warning(this, tr("No Repo"), tr("Open a repo before committing files"));
        return;
    }
    if (QMessageBox::question(this, tr("Commit New File"),
                              tr("Commit '%1' as a new versioned file?\n\nVdir name: %1").arg(relPath),
                              QMessageBox::Ok | QMessageBox::Cancel) != QMessageBox::Ok)
        return;
    log(tr("Committing new file: %1").arg(relPath));
    PfvResult r;
    {
        BusyCursor busy;
        r = PfvBackend::call(QStringLiteral("commit_new"), args({{"rel_path", relPath}}));
    }
    if (!r.ok) {
        reportError(tr("Commit Error"), r);
        return;
    }
    log(tr("Committed: %1 (v%2)").arg(relPath).arg(r.object().value(QStringLiteral("version")).toInt()));
    refresh();
}

// ---------------------------------------------------------------------------
// Sync
// ---------------------------------------------------------------------------

void MainWindow::syncFiles()
{
    if (m_storage.isEmpty()) {
        QMessageBox::warning(this, tr("Storage Error"), tr("Storage not available"));
        return;
    }
    auto *progress = new QProgressDialog(tr("Syncing files..."), QString(), 0, 0, this);
    progress->setWindowModality(Qt::WindowModal);
    progress->setAttribute(Qt::WA_DeleteOnClose);
    progress->show();
    log(tr("Sync started: %1 <-> %2").arg(m_workTree, m_storage));

    // Runs on a pool thread; progress and the result come back queued.
    QPointer<MainWindow> self(this);
    QPointer<QProgressDialog> dialog(progress);
    const auto report = [self, dialog](const QString &message) {
        if (!self)
            return;
        QMetaObject::invokeMethod(self.data(), [self, dialog, message] {
            if (dialog)
                dialog->setLabelText(message);
            if (self)
                self->log(tr("Sync: %1").arg(message));
        }, Qt::QueuedConnection);
    };
    auto outcome = std::make_shared<std::pair<bool, QString>>(false, QString());

    PfvBackend::runAsync(this, [report, outcome] {
        report(tr("Loading status..."));
        const PfvResult plan = PfvBackend::call(QStringLiteral("sync_plan"));
        if (!plan.ok) {
            *outcome = {false, tr("Sync error: %1").arg(plan.error)};
            return;
        }
        const QJsonObject p = plan.object();
        const QJsonArray conflicts = p.value(QStringLiteral("conflicts")).toArray();
        const QJsonArray modified = p.value(QStringLiteral("modified")).toArray();
        const QJsonArray moved = p.value(QStringLiteral("repo_moved")).toArray();
        if (p.value(QStringLiteral("tracked")).toInt() == 0) {
            *outcome = {true, tr("No tracked files found")};
            return;
        }
        if (!conflicts.isEmpty()) {
            *outcome = {false, tr("Found %1 conflicted file(s)").arg(conflicts.size())};
            return;
        }
        if (!moved.isEmpty())
            report(tr("Skipping %1 file(s) with repo changes").arg(moved.size()));
        if (modified.isEmpty()) {
            *outcome = {true, tr("All files up to date")};
            return;
        }
        report(tr("Checking in %1 modified file(s)...").arg(modified.size()));
        int failed = 0;
        for (int i = 0; i < modified.size(); ++i) {
            const QString dest = modified[i].toString();
            report(QStringLiteral("  [%1/%2] %3").arg(i + 1).arg(modified.size()).arg(QFileInfo(dest).fileName()));
            const PfvResult r = PfvBackend::call(QStringLiteral("checkin"), args({{"dest", dest}}));
            if (!r.ok) {
                ++failed;
                report(tr("    Error: %1").arg(r.error));
            }
        }
        *outcome = {failed == 0, failed == 0 ? tr("Synced %1 file(s)").arg(modified.size())
                                             : tr("Synced %1 of %2 file(s), %3 failed")
                                                   .arg(modified.size() - failed).arg(modified.size()).arg(failed)};
    }, [this, dialog, outcome] {
        if (dialog)
            dialog->close();
        const auto &[ok, message] = *outcome;
        log(tr("Sync finished: %1").arg(message), ok ? QStringLiteral("INFO") : QStringLiteral("ERROR"));
        if (ok)
            QMessageBox::information(this, tr("Sync Complete"), message);
        else
            QMessageBox::warning(this, tr("Sync Error"), message);
        refresh();
    });
}

// ---------------------------------------------------------------------------
// Creating and choosing workspaces
// ---------------------------------------------------------------------------

void MainWindow::openExistingWorkspace()
{
    const QString chosen = QFileDialog::getExistingDirectory(this, tr("Open Workspace Directory"), m_workTree);
    if (!chosen.isEmpty())
        openWorkspace(chosen);
}

void MainWindow::convertDirectoryToWorkspace()
{
    const QString chosen =
        QFileDialog::getExistingDirectory(this, tr("Select Directory to Convert to Workspace"), m_workTree);
    if (chosen.isEmpty())
        return;
    const PfvResult r = PfvBackend::call(QStringLiteral("init_workspace"), args({{"path", chosen}}));
    if (!r.ok) {
        reportError(tr("Could not initialise workspace"), r);
        return;
    }
    if (r.object().value(QStringLiteral("created")).toBool())
        log(tr("Initialised workspace at %1").arg(chosen));
    else
        QMessageBox::information(this, tr("Already a Workspace"),
                                 tr("'%1' is already a PFV workspace.\nOpening it.").arg(chosen));
    openWorkspace(chosen);
}

void MainWindow::newBlankWorkspace()
{
    const QString parentDir = m_workTree.isEmpty() ? QDir::homePath() : QFileInfo(m_workTree).absolutePath();
    NewFolderDialog dialog(tr("New Blank Workspace"), tr("Workspace name:"), tr("e.g. my-project"), parentDir,
                           /*canBrowse=*/true, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString path = dialog.path();
    const PfvResult r = PfvBackend::call(QStringLiteral("init_workspace"), args({{"path", path}}));
    if (!r.ok) {
        reportError(tr("Could not initialise workspace"), r);
        return;
    }
    log(tr("Created workspace at %1").arg(path));
    openWorkspace(path);
}

// ---------------------------------------------------------------------------
// Repo links
// ---------------------------------------------------------------------------

void MainWindow::addRepoLink(const QString &location, const QJsonObject &kwargs, const QString &name)
{
    if (m_workTree.isEmpty()) {
        QMessageBox::warning(this, tr("No Workspace"), tr("Open or create a workspace first (step 1)"));
        return;
    }
    connectStorage(location, kwargs);
    const PfvResult r = PfvBackend::call(QStringLiteral("add_repo"),
                                         args({{"name", name}, {"location", location}, {"kwargs", kwargs}}));
    if (!r.ok)
        log(tr("Could not save repo link: %1").arg(r.error), QStringLiteral("WARN"));
    refreshRepoCombo();
    // Show the link just added (refreshRepoCombo selects the default).
    const int index = m_repoCombo->findData(r.object().value(QStringLiteral("name")).toString());
    if (index >= 0) {
        QSignalBlocker block(m_repoCombo);
        m_repoCombo->setCurrentIndex(index);
    }
}

void MainWindow::linkExistingRepo()
{
    const QString chosen = QFileDialog::getExistingDirectory(this, tr("Select Existing Repo Directory"), m_storage);
    if (chosen.isEmpty())
        return;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Name This Repo Link"),
                                               tr("Label for this repo link in the workspace:"), QLineEdit::Normal,
                                               QFileInfo(chosen).fileName(), &ok);
    if (ok)
        addRepoLink(QDir::toNativeSeparators(chosen), {}, name);
}

void MainWindow::newLocalRepo()
{
    const QString parentDir = QFileDialog::getExistingDirectory(this, tr("Select Parent Directory for New Repo"),
                                                                m_storage);
    if (parentDir.isEmpty())
        return;
    NewFolderDialog dialog(tr("New Local Repo"), tr("Repo folder name:"), tr("e.g. my-project-repo"), parentDir,
                           /*canBrowse=*/false, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString path = QDir::toNativeSeparators(dialog.path());
    if (!QDir().mkpath(path)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not create directory:\n%1").arg(path));
        return;
    }
    addRepoLink(path, {}, QFileInfo(path).fileName());
    log(tr("New repo created at %1").arg(path));
}

void MainWindow::linkS3Repo()
{
    S3RepoDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString location = dialog.location();
    QString defaultName = location.section(QLatin1Char('/'), -1);
    if (defaultName.isEmpty())
        defaultName = QStringLiteral("s3-repo");
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("Name This Repo Link"),
                                         tr("Label for this S3 repo link in the workspace:"), QLineEdit::Normal,
                                         defaultName, &ok);
    if (!ok || name.isEmpty())
        name = defaultName;
    addRepoLink(location, dialog.kwargs(), name);
}

void MainWindow::removeRepoLink()
{
    const QString name = m_repoCombo->currentData().toString();
    if (name.isEmpty()) {
        QMessageBox::information(this, tr("No Repo Selected"), tr("Select a repo link to remove."));
        return;
    }
    if (QMessageBox::question(this, tr("Remove Repo Link"),
                              tr("Remove the link '%1' from this workspace?\n\nThe repo files on disk/S3 are NOT deleted.")
                                  .arg(name)) != QMessageBox::Yes)
        return;
    const PfvResult r = PfvBackend::call(QStringLiteral("remove_repo"), args({{"name", name}}));
    if (!r.ok) {
        reportError(tr("Error"), r);
        return;
    }
    refreshRepoCombo();
    log(tr("Removed repo link '%1'").arg(name));
}
