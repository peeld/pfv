#include "dialogs.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>

namespace {

// OK/Cancel with a custom OK label (and optional style), as a form row.
QDialogButtonBox *addButtons(QDialog *dialog, QFormLayout *layout, const QString &okText,
                             const QString &okStyle = {})
{
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog);
    QPushButton *ok = buttons->button(QDialogButtonBox::Ok);
    if (!okText.isEmpty())
        ok->setText(okText);
    if (!okStyle.isEmpty())
        ok->setStyleSheet(okStyle);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    layout->addRow(buttons);
    return buttons;
}

QLineEdit *lineEdit(const QString &placeholder, QWidget *parent)
{
    auto *edit = new QLineEdit(parent);
    edit->setPlaceholderText(placeholder);
    return edit;
}

} // namespace

// ---------------------------------------------------------------------------

CheckoutDialog::CheckoutDialog(const QString &vdir, const QStringList &versions, const QString &defaultDest,
                               QWidget *parent)
    : QDialog(parent)
    , m_vdir(vdir)
    , m_defaultDest(defaultDest)
{
    setWindowTitle(tr("Checkout %1").arg(vdir));
    setMinimumWidth(500);
    auto *layout = new QFormLayout(this);
    layout->addRow(new QLabel(tr("File: %1").arg(vdir), this));

    m_version = new QComboBox(this);
    m_version->addItems(QStringList{QStringLiteral("latest"), QStringLiteral("HEAD")} + versions);
    layout->addRow(tr("Version:"), m_version);

    auto *destRow = new QHBoxLayout;
    m_dest = lineEdit(defaultDest, this);
    destRow->addWidget(m_dest, 1);
    auto *browse = new QPushButton(tr("Browse..."), this);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString start = m_dest->text().isEmpty() ? m_defaultDest : m_dest->text();
        const QString dest = QFileDialog::getSaveFileName(this, tr("Save %1 as...").arg(m_vdir), start);
        if (!dest.isEmpty())
            m_dest->setText(QDir::toNativeSeparators(dest));
    });
    destRow->addWidget(browse);
    layout->addRow(tr("Destination:"), destRow);

    auto *buttons = addButtons(this, layout, tr("Checkout"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
}

QString CheckoutDialog::version() const
{
    return m_version->currentText();
}

QString CheckoutDialog::dest() const
{
    return m_dest->text().isEmpty() ? m_defaultDest : m_dest->text();
}

// ---------------------------------------------------------------------------

DeleteDialog::DeleteDialog(const QString &vdir, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Delete %1").arg(vdir));
    setMinimumWidth(500);
    auto *layout = new QFormLayout(this);
    layout->addRow(new QLabel(tr("File: %1").arg(vdir), this));
    layout->addRow(new QLabel(tr("Marks the file deleted - history is preserved."), this));
    m_message = lineEdit(tr("e.g., File no longer needed"), this);
    layout->addRow(tr("Message:"), m_message);
    m_reason = lineEdit(tr("e.g., Project archived"), this);
    layout->addRow(tr("Reason:"), m_reason);
    auto *buttons = addButtons(this, layout, tr("Delete"), QStringLiteral("background-color: #F44336; color: white;"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
}

QString DeleteDialog::message() const
{
    return m_message->text().isEmpty() ? QStringLiteral("(deleted)") : m_message->text();
}

QString DeleteDialog::reason() const
{
    return m_reason->text();
}

// ---------------------------------------------------------------------------

RenameDialog::RenameDialog(const QString &vdir, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Rename %1").arg(vdir));
    setMinimumWidth(500);
    auto *layout = new QFormLayout(this);
    layout->addRow(new QLabel(tr("Current name: %1").arg(vdir), this));
    m_newName = lineEdit(tr("e.g., new_project.mp4"), this);
    m_newName->setText(vdir);
    layout->addRow(tr("New name:"), m_newName);
    m_message = lineEdit(tr("e.g., Conform to naming standard"), this);
    layout->addRow(tr("Message:"), m_message);
    auto *buttons = addButtons(this, layout, tr("Rename"), QStringLiteral("background-color: #2196F3; color: white;"));
    connect(buttons, &QDialogButtonBox::accepted, this, [this, vdir] {
        const QString name = newName();
        if (name.isEmpty() || name == vdir) {
            QMessageBox::warning(this, tr("Invalid Name"), tr("New name cannot be the same or empty"));
            return;
        }
        accept();
    });
}

QString RenameDialog::newName() const
{
    return m_newName->text().trimmed();
}

QString RenameDialog::message() const
{
    return m_message->text().isEmpty() ? tr("Renamed to %1").arg(newName()) : m_message->text();
}

// ---------------------------------------------------------------------------

UndeleteDialog::UndeleteDialog(const QString &vdir, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Restore %1").arg(vdir));
    setMinimumWidth(500);
    auto *layout = new QFormLayout(this);
    layout->addRow(new QLabel(tr("File: %1").arg(vdir), this));
    layout->addRow(new QLabel(tr("Restores the file from deletion."), this));
    m_version = lineEdit(tr("Leave empty for automatic (last good version)"), this);
    layout->addRow(tr("Restore from version:"), m_version);
    m_message = lineEdit(tr("e.g., Restoring for reprocessing"), this);
    layout->addRow(tr("Message:"), m_message);
    auto *buttons = addButtons(this, layout, tr("Restore"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
}

QString UndeleteDialog::message() const
{
    return m_message->text().isEmpty() ? QStringLiteral("(restored)") : m_message->text();
}

int UndeleteDialog::restoreVersion() const
{
    bool ok = false;
    const int v = m_version->text().trimmed().toInt(&ok);
    return ok && v > 0 ? v : 0;
}

// ---------------------------------------------------------------------------

NewFolderDialog::NewFolderDialog(const QString &title, const QString &nameLabel, const QString &placeholder,
                                 const QString &parentDir, bool canBrowse, QWidget *parent)
    : QDialog(parent)
    , m_parentDir(parentDir)
{
    setWindowTitle(title);
    setMinimumWidth(400);
    auto *layout = new QFormLayout(this);

    auto *parentRow = new QHBoxLayout;
    m_parentLabel = new QLabel(QDir::toNativeSeparators(parentDir), this);
    m_parentLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 10px;"));
    parentRow->addWidget(m_parentLabel, 1);
    if (canBrowse) {
        auto *browse = new QPushButton(tr("Browse..."), this);
        connect(browse, &QPushButton::clicked, this, [this] {
            const QString chosen = QFileDialog::getExistingDirectory(this, tr("Select Parent Directory"), m_parentDir);
            if (!chosen.isEmpty()) {
                m_parentDir = chosen;
                m_parentLabel->setText(QDir::toNativeSeparators(chosen));
            }
        });
        parentRow->addWidget(browse);
    }
    layout->addRow(tr("Create inside:"), parentRow);

    m_name = lineEdit(placeholder, this);
    layout->addRow(nameLabel, m_name);

    auto *buttons = addButtons(this, layout, {});
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (m_name->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("Name required"), tr("Please enter a name."));
            return;
        }
        accept();
    });
}

QString NewFolderDialog::path() const
{
    return QDir(m_parentDir).filePath(m_name->text().trimmed());
}

// ---------------------------------------------------------------------------

S3RepoDialog::S3RepoDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Remote S3 Repo"));
    setMinimumWidth(420);
    auto *layout = new QFormLayout(this);

    m_bucket = lineEdit(QStringLiteral("my-bucket"), this);
    m_prefix = lineEdit(tr("pfv/repos  (optional)"), this);
    m_region = lineEdit(tr("us-east-1  (optional)"), this);
    m_endpoint = lineEdit(tr("https://...  (optional, S3-compatible)"), this);
    m_keyId = lineEdit(tr("leave blank for env / AWS profile"), this);
    m_secret = lineEdit(tr("leave blank for env / AWS profile"), this);
    m_secret->setEchoMode(QLineEdit::Password);
    m_profile = lineEdit(tr("AWS CLI profile  (optional)"), this);

    layout->addRow(tr("Bucket:"), m_bucket);
    layout->addRow(tr("Prefix:"), m_prefix);
    layout->addRow(tr("Region:"), m_region);
    layout->addRow(tr("Endpoint URL:"), m_endpoint);
    layout->addRow(new QLabel(tr("- Credentials (leave blank for environment/profile; keys are not saved) -"), this));
    layout->addRow(tr("Access key ID:"), m_keyId);
    layout->addRow(tr("Secret key:"), m_secret);
    layout->addRow(tr("AWS profile:"), m_profile);

    auto *buttons = addButtons(this, layout, {});
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (m_bucket->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("Bucket required"), tr("Please enter an S3 bucket name."));
            return;
        }
        accept();
    });
}

QString S3RepoDialog::location() const
{
    const QString bucket = m_bucket->text().trimmed();
    QString prefix = m_prefix->text().trimmed();
    while (prefix.startsWith(QLatin1Char('/')))
        prefix.remove(0, 1);
    while (prefix.endsWith(QLatin1Char('/')))
        prefix.chop(1);
    return prefix.isEmpty() ? QStringLiteral("s3://%1").arg(bucket) : QStringLiteral("s3://%1/%2").arg(bucket, prefix);
}

QJsonObject S3RepoDialog::kwargs() const
{
    QJsonObject args;
    const auto put = [&args](const char *key, const QLineEdit *edit) {
        const QString text = edit->text().trimmed();
        if (!text.isEmpty())
            args.insert(QLatin1String(key), text);
    };
    put("region", m_region);
    put("endpoint_url", m_endpoint);
    put("aws_access_key_id", m_keyId);
    put("aws_secret_access_key", m_secret);
    put("profile", m_profile);
    return args;
}
