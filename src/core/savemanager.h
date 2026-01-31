#ifndef SAVEMANAGER_H
#define SAVEMANAGER_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <QList>
#include "gameinfo.h"

class SaveManager : public QObject {
    Q_OBJECT

public:
    explicit SaveManager(QObject *parent = nullptr);

    void setBackupDirectory(const QString &dir);
    QString getBackupDirectory() const;
    void setCompressionLevel(int level);

    // Synchronous methods (kept for internal use)
    bool createBackup(const GameInfo &game, const QString &backupName = QString(),
                      const QString &notes = QString(), const SaveProfile &profile = SaveProfile());
    bool restoreBackup(const BackupInfo &backup, const QString &targetPath);
    bool deleteBackup(const BackupInfo &backup);
    bool updateBackupMetadata(const BackupInfo &backup);

    // Async methods
    void createBackupAsync(const GameInfo &game, const QString &backupName = QString(),
                           const QString &notes = QString(), const SaveProfile &profile = SaveProfile());
    void restoreBackupAsync(const BackupInfo &backup, const QString &targetPath);
    void cancelOperation();
    bool isBusy() const;

    QList<BackupInfo> getBackupsForGame(const QString &gameId) const;
    BackupInfo getBackupById(const QString &gameId, const QString &backupId) const;
    QStringList getAllGameIdsWithBackups() const;
    QString getGameNameFromBackups(const QString &gameId) const;
    bool verifyBackup(const BackupInfo &backup);

signals:
    void backupCreated(const QString &gameId, const QString &backupId);
    void backupRestored(const QString &gameId, const QString &backupId);
    void backupDeleted(const QString &gameId, const QString &backupId);
    void backupUpdated(const QString &gameId, const QString &backupId);
    void backupVerified(const QString &gameId, const QString &backupId, bool valid);
    void operationStarted(const QString &description);
    void operationFinished();
    void operationCancelled();
    void error(const QString &message);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    enum class PendingOp { None, Backup, Restore };

    QString getGameBackupDir(const QString &gameId) const;
    QString generateBackupId() const;
    bool compressDirectory(const QString &sourceDir, const QString &archivePath);
    bool compressFiles(const QString &baseDir, const QStringList &relativePaths, const QString &archivePath);
    bool extractArchive(const QString &archivePath, const QString &targetDir);
    bool restoreProfileBackup(const BackupInfo &backup, const QString &targetPath);
    bool copyDirectory(const QString &source, const QString &destination);
    bool removeDirectory(const QString &path);
    qint64 getDirectorySize(const QString &path) const;
    bool saveBackupMetadata(const BackupInfo &backup);
    BackupInfo loadBackupMetadata(const QString &metadataPath) const;

    QString m_backupDir;
    int m_compressionLevel = 6;

    // Async state
    QProcess *m_process = nullptr;
    PendingOp m_pendingOp = PendingOp::None;
    BackupInfo m_pendingBackup;
    GameInfo m_pendingGame;
    QString m_pendingRestoreTarget;
    QString m_pendingTempDir;
    bool m_pendingIsProfile = false;
};

#endif // SAVEMANAGER_H
