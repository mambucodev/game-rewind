#include "savemanager.h"
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QDebug>

SaveManager::SaveManager(QObject *parent)
    : QObject(parent)
{
    QString defaultBackupDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                               + "/game-rewind";
    setBackupDirectory(defaultBackupDir);
}

void SaveManager::setBackupDirectory(const QString &dir)
{
    m_backupDir = dir;
    QDir().mkpath(m_backupDir);
}

QString SaveManager::getBackupDirectory() const
{
    return m_backupDir;
}

bool SaveManager::createBackup(const GameInfo &game, const QString &backupName,
                               const QString &notes, const SaveProfile &profile)
{
    if (!game.isDetected || game.detectedSavePath.isEmpty()) {
        emit error("Game save path not detected");
        return false;
    }

    if (!QFile::exists(game.detectedSavePath)) {
        emit error("Save path does not exist: " + game.detectedSavePath);
        return false;
    }

    QString gameBackupDir = getGameBackupDir(game.id);
    QDir().mkpath(gameBackupDir);

    BackupInfo backup;
    backup.id = generateBackupId();
    backup.gameId = game.id;
    backup.gameName = game.name;
    backup.notes = notes;
    backup.timestamp = QDateTime::currentDateTime();
    backup.profileId = profile.id;
    backup.profileName = profile.name;

    if (backupName.isEmpty()) {
        backup.displayName = backup.timestamp.toString("yyyy-MM-dd HH:mm:ss");
    } else {
        backup.displayName = backupName;
    }

    QString archiveName = backup.id + ".tar.gz";
    backup.archivePath = gameBackupDir + "/" + archiveName;

    bool compressed;
    if (profile.id == -1) {
        compressed = compressDirectory(game.detectedSavePath, backup.archivePath);
    } else {
        compressed = compressFiles(game.detectedSavePath, profile.files, backup.archivePath);
    }

    if (!compressed) {
        emit error("Failed to create backup archive");
        return false;
    }

    backup.size = QFile(backup.archivePath).size();

    if (!saveBackupMetadata(backup)) {
        emit error("Failed to save backup metadata");
        QFile::remove(backup.archivePath);
        return false;
    }

    emit backupCreated(game.id, backup.id);
    return true;
}

bool SaveManager::restoreBackup(const BackupInfo &backup, const QString &targetPath)
{
    if (!QFile::exists(backup.archivePath)) {
        emit error("Backup archive not found: " + backup.archivePath);
        return false;
    }

    // Profile backups: extract directly, overwriting only specific files
    if (backup.profileId != -1) {
        return restoreProfileBackup(backup, targetPath);
    }

    // Full directory backup: extract to temp, replace entire directory
    QString tempDir = m_backupDir + "/temp_restore_" + QString::number(QDateTime::currentMSecsSinceEpoch());

    if (!extractArchive(backup.archivePath, tempDir)) {
        emit error("Failed to extract backup archive");
        removeDirectory(tempDir);
        return false;
    }

    if (QFile::exists(targetPath)) {
        if (!removeDirectory(targetPath)) {
            emit error("Failed to remove existing save directory");
            removeDirectory(tempDir);
            return false;
        }
    }

    QDir tempDirObj(tempDir);
    QStringList entries = tempDirObj.entryList(QDir::NoDotAndDotDot | QDir::AllEntries);

    QString sourceDir = tempDir;
    if (entries.size() == 1) {
        QFileInfo entryInfo(tempDir + "/" + entries.first());
        if (entryInfo.isDir()) {
            sourceDir = entryInfo.absoluteFilePath();
        }
    }

    if (!copyDirectory(sourceDir, targetPath)) {
        emit error("Failed to restore backup to target location");
        removeDirectory(tempDir);
        return false;
    }

    removeDirectory(tempDir);

    emit backupRestored(backup.gameId, backup.id);
    return true;
}

bool SaveManager::deleteBackup(const BackupInfo &backup)
{
    bool success = true;

    if (QFile::exists(backup.archivePath)) {
        success = QFile::remove(backup.archivePath);
    }

    QString metadataPath = backup.archivePath + ".json";
    if (QFile::exists(metadataPath)) {
        success = success && QFile::remove(metadataPath);
    }

    if (success) {
        emit backupDeleted(backup.gameId, backup.id);
    } else {
        emit error("Failed to delete backup");
    }

    return success;
}

QList<BackupInfo> SaveManager::getBackupsForGame(const QString &gameId) const
{
    QList<BackupInfo> backups;
    QString gameBackupDir = getGameBackupDir(gameId);

    QDir dir(gameBackupDir);
    if (!dir.exists()) {
        return backups;
    }

    QStringList metadataFiles = dir.entryList(QStringList() << "*.json", QDir::Files);

    for (const QString &metadataFile : metadataFiles) {
        QString metadataPath = dir.absoluteFilePath(metadataFile);
        BackupInfo backup = loadBackupMetadata(metadataPath);

        if (!backup.id.isEmpty()) {
            backups.append(backup);
        }
    }

    std::sort(backups.begin(), backups.end(), [](const BackupInfo &a, const BackupInfo &b) {
        return a.timestamp > b.timestamp;
    });

    return backups;
}

BackupInfo SaveManager::getBackupById(const QString &gameId, const QString &backupId) const
{
    QList<BackupInfo> backups = getBackupsForGame(gameId);

    for (const BackupInfo &backup : backups) {
        if (backup.id == backupId) {
            return backup;
        }
    }

    return BackupInfo();
}

QString SaveManager::getGameBackupDir(const QString &gameId) const
{
    return m_backupDir + "/games/" + gameId;
}

QString SaveManager::generateBackupId() const
{
    return QString::number(QDateTime::currentMSecsSinceEpoch());
}

bool SaveManager::compressDirectory(const QString &sourceDir, const QString &archivePath)
{
    QFileInfo sourceInfo(sourceDir);
    QString parentDir = sourceInfo.absolutePath();
    QString dirName = sourceInfo.fileName();

    QProcess process;
    process.setWorkingDirectory(parentDir);

    QStringList args;
    args << "-czf" << archivePath << dirName;

    process.start("tar", args);
    process.waitForFinished(-1);

    if (process.exitCode() != 0) {
        qWarning() << "tar failed:" << process.readAllStandardError();
        return false;
    }

    return true;
}

bool SaveManager::compressFiles(const QString &baseDir, const QStringList &relativePaths,
                                const QString &archivePath)
{
    QProcess process;
    process.setWorkingDirectory(baseDir);

    QStringList args;
    args << "-czf" << archivePath;

    for (const QString &relPath : relativePaths) {
        QString fullPath = baseDir + "/" + relPath;
        if (QFileInfo::exists(fullPath)) {
            args << relPath;
        } else {
            qWarning() << "Profile file not found, skipping:" << fullPath;
        }
    }

    if (args.size() <= 2) {
        qWarning() << "No profile files found on disk";
        return false;
    }

    process.start("tar", args);
    process.waitForFinished(-1);

    if (process.exitCode() != 0) {
        qWarning() << "tar failed:" << process.readAllStandardError();
        return false;
    }

    return true;
}

bool SaveManager::restoreProfileBackup(const BackupInfo &backup, const QString &targetPath)
{
    QDir().mkpath(targetPath);

    QProcess process;
    process.setWorkingDirectory(targetPath);

    QStringList args;
    args << "-xzf" << backup.archivePath;

    process.start("tar", args);
    process.waitForFinished(-1);

    if (process.exitCode() != 0) {
        qWarning() << "tar extraction failed:" << process.readAllStandardError();
        emit error("Failed to restore profile backup");
        return false;
    }

    emit backupRestored(backup.gameId, backup.id);
    return true;
}

bool SaveManager::extractArchive(const QString &archivePath, const QString &targetDir)
{
    QDir().mkpath(targetDir);

    QProcess process;
    process.setWorkingDirectory(targetDir);

    QStringList args;
    args << "-xzf" << archivePath;

    process.start("tar", args);
    process.waitForFinished(-1);

    if (process.exitCode() != 0) {
        qWarning() << "tar extraction failed:" << process.readAllStandardError();
        return false;
    }

    return true;
}

bool SaveManager::copyDirectory(const QString &source, const QString &destination)
{
    QDir sourceDir(source);
    if (!sourceDir.exists()) {
        return false;
    }

    QDir destDir(destination);
    if (!destDir.exists()) {
        if (!QDir().mkpath(destination)) {
            return false;
        }
    }

    QStringList entries = sourceDir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries);

    for (const QString &entry : entries) {
        QString srcPath = source + "/" + entry;
        QString dstPath = destination + "/" + entry;

        QFileInfo info(srcPath);

        if (info.isDir()) {
            if (!copyDirectory(srcPath, dstPath)) {
                return false;
            }
        } else {
            if (!QFile::copy(srcPath, dstPath)) {
                return false;
            }
        }
    }

    return true;
}

bool SaveManager::removeDirectory(const QString &path)
{
    QDir dir(path);
    return dir.removeRecursively();
}

qint64 SaveManager::getDirectorySize(const QString &path) const
{
    qint64 size = 0;
    QDir dir(path);

    QStringList entries = dir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries);

    for (const QString &entry : entries) {
        QString entryPath = path + "/" + entry;
        QFileInfo info(entryPath);

        if (info.isDir()) {
            size += getDirectorySize(entryPath);
        } else {
            size += info.size();
        }
    }

    return size;
}

bool SaveManager::saveBackupMetadata(const BackupInfo &backup)
{
    QJsonObject obj;
    obj["id"] = backup.id;
    obj["gameId"] = backup.gameId;
    obj["gameName"] = backup.gameName;
    obj["displayName"] = backup.displayName;
    obj["notes"] = backup.notes;
    obj["timestamp"] = backup.timestamp.toString(Qt::ISODate);
    obj["archivePath"] = backup.archivePath;
    obj["size"] = backup.size;
    obj["profileName"] = backup.profileName;
    obj["profileId"] = backup.profileId;

    QJsonDocument doc(obj);

    QString metadataPath = backup.archivePath + ".json";
    QFile file(metadataPath);

    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    file.write(doc.toJson());
    file.close();

    return true;
}

BackupInfo SaveManager::loadBackupMetadata(const QString &metadataPath) const
{
    BackupInfo backup;

    QFile file(metadataPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return backup;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isObject()) {
        return backup;
    }

    QJsonObject obj = doc.object();
    backup.id = obj["id"].toString();
    backup.gameId = obj["gameId"].toString();
    backup.gameName = obj["gameName"].toString();
    backup.displayName = obj["displayName"].toString();
    backup.notes = obj["notes"].toString();
    backup.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
    backup.archivePath = obj["archivePath"].toString();
    backup.size = obj["size"].toInteger();
    backup.profileName = obj["profileName"].toString();
    backup.profileId = obj["profileId"].toInt(-1);

    return backup;
}
