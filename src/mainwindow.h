#pragma once

// The PFV main window: (1) pick a workspace, (2) pick a repo link, then the
// file tree (repo or workspace view) next to the selected file's version
// history, with a log panel underneath. All PFV work is done by the Python
// backend (pfvBackend.h); this class is only UI.
//
// shiboken6 parses this header for the built-in "pfvgui" module
// (src/bindings.xml): keep the public API to plain Qt types.

#include <QMainWindow>

class FileTreeModel;
class QComboBox;
class QDockWidget;
class QJsonObject;
class QLabel;
class QLineEdit;
class QModelIndex;
class QPlainTextEdit;
class QPoint;
class QPushButton;
class QSettings;
class QTableWidget;
class QTreeView;
class ScriptWidget;
struct PfvResult;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // Once Python is running: fill the workspace list, then open workTree
    // (connecting to storage if given) if it isn't empty.
    void startup(const QString &workTree, const QString &storage);

    // Open a workspace folder and connect to its default repo, or to
    // storageOverride if that isn't empty.
    void openWorkspace(const QString &path, const QString &storageOverride = QString());
    QString workTree() const;
    QString storageLocation() const;
    // The selected file's vdir, or empty.
    QString currentVdir() const;
    // Reload the file list.
    void refresh();
    void appendLog(const QString &message);
    // Everything in the log panel.
    QString logText() const;

    // Crashes the process (a null write), to test Sentry crash reports.
    static void testCrash();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    enum class View { Repo, Workspace };

    void setupUi();
    void addScriptEditor(QSettings *settings);
    void log(const QString &message, const QString &level = QStringLiteral("INFO"));
    // Logs a failed result (with its traceback) and shows a message box.
    void reportError(const QString &title, const PfvResult &result);
    bool requireSelection();

    void refreshWorkspaceCombo();
    void refreshRepoCombo();
    void connectStorage(const QString &location, const QJsonObject &kwargs);
    void connectRepo(const QString &name); // empty: the session's default
    void setStorage(const QString &location);

    void setView(View view);
    void setModel(FileTreeModel *model);
    void onSelectionChanged(const QModelIndex &current);
    void loadHistory(const QString &vdir);
    void clearHistory();

    void checkoutFile();
    void abandonCheckout();
    void deleteFile();
    void renameFile();
    void undeleteFile();
    void treeContextMenu(const QPoint &pos);
    void commitNewFile(const QString &relPath);
    void syncFiles();

    void openExistingWorkspace();
    void convertDirectoryToWorkspace();
    void newBlankWorkspace();
    void addRepoLink(const QString &location, const QJsonObject &kwargs, const QString &name);
    void linkExistingRepo();
    void newLocalRepo();
    void linkS3Repo();
    void removeRepoLink();

    QString m_workTree;
    QString m_storage;   // location of the connected storage, empty if none
    QString m_currentVdir;
    View m_view = View::Repo;
    int m_listRequest = 0;    // drops replies from superseded list loads
    int m_historyRequest = 0; // and history loads

    QComboBox *m_workspaceCombo = nullptr;
    QComboBox *m_repoCombo = nullptr;
    QLineEdit *m_workTreeEdit = nullptr;
    QLineEdit *m_storageEdit = nullptr;
    QPushButton *m_repoViewButton = nullptr;
    QPushButton *m_workspaceViewButton = nullptr;
    QTreeView *m_tree = nullptr;
    FileTreeModel *m_model = nullptr;
    QTableWidget *m_history = nullptr;
    QStringList m_historyVersions; // newest first, for the checkout dialog
    QPlainTextEdit *m_log = nullptr;
    QLabel *m_status = nullptr;
    QDockWidget *m_scriptDock = nullptr;
    ScriptWidget *m_scriptWidget = nullptr;
};
