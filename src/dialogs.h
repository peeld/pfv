#pragma once

// Modal dialogs for the main window. Construct, exec(), then read the
// accessors.

#include <QDialog>
#include <QJsonObject>
#include <QStringList>

class QComboBox;
class QLabel;
class QLineEdit;

// Pick a version and destination for a checkout.
class CheckoutDialog : public QDialog
{
    Q_OBJECT
public:
    // versions: newest first; "latest" and "HEAD" are offered before them.
    CheckoutDialog(const QString &vdir, const QStringList &versions, const QString &defaultDest,
                   QWidget *parent = nullptr);
    QString version() const;
    QString dest() const;

private:
    QString m_vdir;
    QString m_defaultDest;
    QComboBox *m_version;
    QLineEdit *m_dest;
};

// Message and reason before marking a file deleted.
class DeleteDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DeleteDialog(const QString &vdir, QWidget *parent = nullptr);
    QString message() const;
    QString reason() const;

private:
    QLineEdit *m_message;
    QLineEdit *m_reason;
};

// New name and message for a rename.
class RenameDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RenameDialog(const QString &vdir, QWidget *parent = nullptr);
    QString newName() const;
    QString message() const;

private:
    QLineEdit *m_newName;
    QLineEdit *m_message;
};

// Optional version to restore from, and a message.
class UndeleteDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UndeleteDialog(const QString &vdir, QWidget *parent = nullptr);
    QString message() const;
    int restoreVersion() const; // 0 = automatic (the version before the deletion)

private:
    QLineEdit *m_version;
    QLineEdit *m_message;
};

// A name inside a parent folder: a new blank workspace or a new local repo.
class NewFolderDialog : public QDialog
{
    Q_OBJECT
public:
    NewFolderDialog(const QString &title, const QString &nameLabel, const QString &placeholder,
                    const QString &parentDir, bool canBrowse, QWidget *parent = nullptr);
    QString path() const;

private:
    QString m_parentDir;
    QLabel *m_parentLabel;
    QLineEdit *m_name;
};

// S3 bucket, prefix and credentials.
class S3RepoDialog : public QDialog
{
    Q_OBJECT
public:
    explicit S3RepoDialog(QWidget *parent = nullptr);
    QString location() const;   // s3://bucket[/prefix]
    QJsonObject kwargs() const; // for pfv_storage.open_storage()

private:
    QLineEdit *m_bucket, *m_prefix, *m_region, *m_endpoint, *m_keyId, *m_secret, *m_profile;
};
