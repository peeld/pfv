#include "fileTreeModel.h"

#include <QColor>
#include <QDateTime>
#include <QJsonObject>

namespace {

QString formatIsoDate(const QString &iso)
{
    if (iso.isEmpty())
        return {};
    const QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    return dt.isValid() ? dt.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm")) : iso;
}

int rowOf(const FileTreeModel::Node *node)
{
    const auto &siblings = node->parent->children;
    for (size_t i = 0; i < siblings.size(); ++i)
        if (siblings[i].get() == node)
            return int(i);
    return 0;
}

} // namespace

FileTreeModel::FileTreeModel(Mode mode, QObject *parent)
    : QAbstractItemModel(parent)
    , m_mode(mode)
{}

QString FileTreeModel::formatSize(qint64 bytes)
{
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double size = double(bytes);
    for (const char *unit : units) {
        if (size < 1024)
            return QStringLiteral("%1 %2").arg(size, 0, 'f', 1).arg(QLatin1String(unit));
        size /= 1024;
    }
    return QStringLiteral("%1 PB").arg(size, 0, 'f', 1);
}

FileTreeModel::Node *FileTreeModel::insert(const QString &path)
{
    const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    Node *folder = &m_root;
    for (qsizetype i = 0; i + 1 < parts.size(); ++i) {
        Node *next = nullptr;
        for (const auto &child : folder->children)
            if (child->isDir && child->name == parts[i]) {
                next = child.get();
                break;
            }
        if (!next) {
            auto created = std::make_unique<Node>();
            created->name = parts[i];
            created->parent = folder;
            next = created.get();
            folder->children.push_back(std::move(created));
        }
        folder = next;
    }
    auto leaf = std::make_unique<Node>();
    leaf->name = parts.isEmpty() ? path : parts.last();
    leaf->isDir = false;
    leaf->parent = folder;
    Node *raw = leaf.get();
    folder->children.push_back(std::move(leaf));
    return raw;
}

FileTreeModel *FileTreeModel::fromRepoView(const QJsonArray &files, QObject *parent)
{
    auto *model = new FileTreeModel(Mode::Repo, parent);
    for (const QJsonValue &v : files) {
        const QJsonObject f = v.toObject();
        const QString vdir = f.value(QStringLiteral("vdir")).toString();
        Node *n = model->insert(vdir);
        n->vdir = vdir;
        n->relPath = vdir;
        n->version = f.value(QStringLiteral("version")).toInt();
        n->size = qint64(f.value(QStringLiteral("size")).toDouble());
        n->date = formatIsoDate(f.value(QStringLiteral("committed_at")).toString());
        n->locked = f.value(QStringLiteral("is_locked")).toBool();
        if (f.value(QStringLiteral("conflict")).toBool())
            n->status = QStringLiteral("⚠ CONFLICT");
        else if (f.value(QStringLiteral("file_modified")).toBool())
            n->status = QStringLiteral("✎ Modified");
        else if (f.value(QStringLiteral("repo_moved")).toBool())
            n->status = QStringLiteral("↻ Repo changed");
        else
            n->status = QStringLiteral("✓");
        if (n->locked)
            n->status += QStringLiteral(" 🔒");
    }
    return model;
}

FileTreeModel *FileTreeModel::fromWorkspaceView(const QJsonArray &files, QObject *parent)
{
    auto *model = new FileTreeModel(Mode::Workspace, parent);
    for (const QJsonValue &v : files) {
        const QJsonObject f = v.toObject();
        const QString rel = f.value(QStringLiteral("rel_path")).toString();
        Node *n = model->insert(rel);
        n->relPath = rel;
        n->inRepo = f.value(QStringLiteral("in_repo")).toBool();
        n->vdir = n->inRepo ? rel : QString();
        n->size = qint64(f.value(QStringLiteral("size")).toDouble());
        n->mtime = QDateTime::fromMSecsSinceEpoch(qint64(f.value(QStringLiteral("mtime")).toDouble() * 1000))
                       .toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    }
    return model;
}

const FileTreeModel::Node *FileTreeModel::node(const QModelIndex &index) const
{
    return index.isValid() ? static_cast<const Node *>(index.internalPointer()) : nullptr;
}

QString FileTreeModel::vdir(const QModelIndex &index) const
{
    const Node *n = node(index);
    return n ? n->vdir : QString();
}

QModelIndex FileTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return {};
    const Node *p = parent.isValid() ? node(parent) : &m_root;
    return createIndex(row, column, p->children[size_t(row)].get());
}

QModelIndex FileTreeModel::parent(const QModelIndex &index) const
{
    const Node *n = node(index);
    if (!n || !n->parent || n->parent == &m_root)
        return {};
    return createIndex(rowOf(n->parent), 0, n->parent);
}

int FileTreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;
    const Node *p = parent.isValid() ? node(parent) : &m_root;
    return int(p->children.size());
}

int FileTreeModel::columnCount(const QModelIndex &) const
{
    return m_mode == Mode::Repo ? 5 : 4;
}

QVariant FileTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    static const QStringList repo = {tr("File"), tr("Version"), tr("Size"), tr("Date"), tr("Status")};
    static const QStringList workspace = {tr("Filename"), tr("Local size"), tr("Last modified"), tr("In repo?")};
    const QStringList &headers = m_mode == Mode::Repo ? repo : workspace;
    return section < headers.size() ? headers[section] : QVariant();
}

QVariant FileTreeModel::data(const QModelIndex &index, int role) const
{
    const Node *n = node(index);
    if (!n)
        return {};
    const int col = index.column();

    if (role == Qt::DisplayRole) {
        if (col == 0)
            return n->name;
        if (n->isDir)
            return {}; // folder rows: name only
        if (m_mode == Mode::Repo) {
            switch (col) {
            case 1: return n->version;
            case 2: return formatSize(n->size);
            case 3: return n->date;
            case 4: return n->status;
            }
        } else {
            switch (col) {
            case 1: return formatSize(n->size);
            case 2: return n->mtime;
            case 3: return n->inRepo ? tr("Yes") : tr("No");
            }
        }
        return {};
    }

    if (role == Qt::ForegroundRole && !n->isDir) {
        if (m_mode == Mode::Repo && col == 0 && n->locked)
            return QColor(0xFF, 0x98, 0x00); // amber: locked
        if (m_mode == Mode::Workspace && col == 3 && !n->inRepo)
            return QColor(0x55, 0x55, 0x55); // dim: untracked
    }

    if (role == Qt::UserRole)
        return n->vdir;

    return {};
}
