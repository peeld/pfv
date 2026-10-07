#pragma once

// The file tree on the left of the main window, in one of two modes:
//   Repo      - every vdir in the connected storage (pfv_app.repo_view())
//               File | Version | Size | Date | Status
//   Workspace - every file under the work tree (pfv_app.workspace_view())
//               Filename | Local size | Last modified | In repo?
// vdirs and relative paths with '/' are split into folder nodes.

#include <QAbstractItemModel>
#include <QJsonArray>

#include <memory>
#include <vector>

class FileTreeModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum class Mode { Repo, Workspace };

    struct Node
    {
        QString name;     // basename
        bool isDir = true;
        Node *parent = nullptr;
        std::vector<std::unique_ptr<Node>> children;

        QString vdir;     // storage name; empty for folders and untracked workspace files
        QString relPath;  // path relative to the work tree (workspace) or the vdir (repo)

        // Repo mode
        int version = 0;
        qint64 size = 0;
        QString date;     // already formatted
        QString status;
        bool locked = false;

        // Workspace mode
        bool inRepo = false;
        QString mtime;    // already formatted
    };

    explicit FileTreeModel(Mode mode, QObject *parent = nullptr);

    Mode mode() const { return m_mode; }

    // From pfv_app.repo_view()["files"] / workspace_view()["files"].
    static FileTreeModel *fromRepoView(const QJsonArray &files, QObject *parent);
    static FileTreeModel *fromWorkspaceView(const QJsonArray &files, QObject *parent);

    const Node *node(const QModelIndex &index) const;
    // Empty for folders and untracked workspace files.
    QString vdir(const QModelIndex &index) const;

    static QString formatSize(qint64 bytes);

    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
    Node *insert(const QString &path); // the leaf node for path, creating folders

    Mode m_mode;
    Node m_root;
};
