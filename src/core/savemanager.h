#ifndef SAVEMANAGER_H
#define SAVEMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include "gameinfo.h"

class SaveManager : public QObject {
    Q_OBJECT

public:
    explicit SaveManager(QObject *parent = nullptr);

    void setBackupDirectory(const QString &dir);
    QString getBackupDirectory() const;

    bool createBackup(const GameInfo &game, const QString &backupName = QString(), const QString &notes = QString());
    bool restoreBackup(const BackupInfo &backup, const QString &targetPath);
    bool deleteBackup(const BackupInfo &backup);

    QList<BackupInfo> getBackupsForGame(const QString &gameId) const;
    BackupInfo getBackupById(const QString &gameId, const QString &backupId) const;

signals:
    void backupCreated(const QString &gameId, const QString &backupId);
    void backupRestored(const QString &gameId, const QString &backupId);
    void backupDeleted(const QString &gameId, const QString &backupId);
    void error(const QString &message);

private:
    QString getGameBackupDir(const QString &gameId) const;
    QString generateBackupId() const;
    bool compressDirectory(const QString &sourceDir, const QString &archivePath);
    bool extractArchive(const QString &archivePath, const QString &targetDir);
    bool copyDirectory(const QString &source, const QString &destination);
    bool removeDirectory(const QString &path);
    qint64 getDirectorySize(const QString &path) const;
    bool saveBackupMetadata(const BackupInfo &backup);
    BackupInfo loadBackupMetadata(const QString &metadataPath) const;

    QString m_backupDir;
};

#endif // SAVEMANAGER_H
