#include "savemanager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDebug>
#include <QtConcurrent>
#include <archive.h>
#include <archive_entry.h>

SaveManager::SaveManager(QObject *parent)
    : QObject(parent)
{
    QString defaultBackupDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                               + "/game-rewind";
    setBackupDirectory(defaultBackupDir);

    connect(&m_backupWatcher, &QFutureWatcher<AsyncResult>::finished,
            this, &SaveManager::onAsyncBackupFinished);
    connect(&m_restoreWatcher, &QFutureWatcher<AsyncResult>::finished,
            this, &SaveManager::onAsyncRestoreFinished);
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

void SaveManager::setCompressionLevel(int level)
{
    if (level >= 1 && level <= 9) {
        m_compressionLevel = level;
    }
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
        compressed = compressDirectory(game.detectedSavePath, backup.archivePath, m_compressionLevel);
    } else {
        compressed = compressFiles(game.detectedSavePath, profile.files, backup.archivePath, m_compressionLevel);
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

bool SaveManager::updateBackupMetadata(const BackupInfo &backup)
{
    if (backup.archivePath.isEmpty() || !QFile::exists(backup.archivePath)) {
        emit error("Backup archive not found");
        return false;
    }

    if (!saveBackupMetadata(backup)) {
        emit error("Failed to update backup metadata");
        return false;
    }

    emit backupUpdated(backup.gameId, backup.id);
    return true;
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

bool SaveManager::verifyBackup(const BackupInfo &backup)
{
    if (!QFile::exists(backup.archivePath)) {
        emit backupVerified(backup.gameId, backup.id, false);
        return false;
    }

    struct archive *a = archive_read_new();
    archive_read_support_filter_gzip(a);
    archive_read_support_format_tar(a);

    bool valid = false;
    if (archive_read_open_filename(a, backup.archivePath.toLocal8Bit().constData(), 10240) == ARCHIVE_OK) {
        struct archive_entry *entry;
        valid = true;
        while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
            archive_read_data_skip(a);
        }
        if (archive_errno(a) != 0) {
            valid = false;
        }
    }

    archive_read_free(a);
    emit backupVerified(backup.gameId, backup.id, valid);
    return valid;
}

bool SaveManager::isBusy() const
{
    return m_busy;
}

void SaveManager::createBackupAsync(const GameInfo &game, const QString &backupName,
                                     const QString &notes, const SaveProfile &profile)
{
    if (isBusy()) {
        emit error("Another operation is in progress");
        return;
    }

    if (!game.isDetected || game.detectedSavePath.isEmpty()) {
        emit error("Game save path not detected");
        return;
    }
    if (!QFile::exists(game.detectedSavePath)) {
        emit error("Save path does not exist: " + game.detectedSavePath);
        return;
    }

    QString gameBackupDir = getGameBackupDir(game.id);
    QDir().mkpath(gameBackupDir);

    m_pendingBackup = BackupInfo();
    m_pendingBackup.id = generateBackupId();
    m_pendingBackup.gameId = game.id;
    m_pendingBackup.gameName = game.name;
    m_pendingBackup.notes = notes;
    m_pendingBackup.timestamp = QDateTime::currentDateTime();
    m_pendingBackup.profileId = profile.id;
    m_pendingBackup.profileName = profile.name;
    m_pendingBackup.displayName = backupName.isEmpty()
        ? m_pendingBackup.timestamp.toString("yyyy-MM-dd HH:mm:ss")
        : backupName;

    QString archiveName = m_pendingBackup.id + ".tar.gz";
    m_pendingBackup.archivePath = gameBackupDir + "/" + archiveName;

    m_busy = true;
    m_cancelRequested = false;
    emit operationStarted("Creating backup...");

    QString savePath = game.detectedSavePath;
    QString archivePath = m_pendingBackup.archivePath;
    int compressionLevel = m_compressionLevel;
    QStringList profileFiles = profile.files;
    int profileId = profile.id;

    m_backupWatcher.setFuture(QtConcurrent::run([savePath, archivePath, compressionLevel,
                                                  profileFiles, profileId]() -> AsyncResult {
        AsyncResult result;
        if (profileId == -1) {
            result.success = compressDirectory(savePath, archivePath, compressionLevel);
        } else {
            result.success = compressFiles(savePath, profileFiles, archivePath, compressionLevel);
        }
        if (!result.success) {
            result.errorMessage = "Failed to create backup archive";
        }
        return result;
    }));
}

void SaveManager::onAsyncBackupFinished()
{
    AsyncResult result = m_backupWatcher.result();
    m_busy = false;

    if (m_cancelRequested) {
        QFile::remove(m_pendingBackup.archivePath);
        emit operationCancelled();
        return;
    }

    if (!result.success) {
        QFile::remove(m_pendingBackup.archivePath);
        emit error(result.errorMessage);
    } else {
        m_pendingBackup.size = QFileInfo(m_pendingBackup.archivePath).size();
        if (saveBackupMetadata(m_pendingBackup)) {
            emit backupCreated(m_pendingBackup.gameId, m_pendingBackup.id);
        } else {
            QFile::remove(m_pendingBackup.archivePath);
            emit error("Failed to save backup metadata");
        }
    }
    emit operationFinished();
}

void SaveManager::restoreBackupAsync(const BackupInfo &backup, const QString &targetPath)
{
    if (isBusy()) {
        emit error("Another operation is in progress");
        return;
    }

    if (!QFile::exists(backup.archivePath)) {
        emit error("Backup archive not found: " + backup.archivePath);
        return;
    }

    m_pendingBackup = backup;
    m_pendingRestoreTarget = targetPath;
    m_pendingIsProfile = (backup.profileId != -1);

    m_busy = true;
    m_cancelRequested = false;
    emit operationStarted("Restoring backup...");

    if (m_pendingIsProfile) {
        QDir().mkpath(targetPath);
        QString archivePath = backup.archivePath;

        m_restoreWatcher.setFuture(QtConcurrent::run([archivePath, targetPath]() -> AsyncResult {
            AsyncResult result;
            result.success = extractArchive(archivePath, targetPath);
            if (!result.success) {
                result.errorMessage = "Failed to extract backup archive";
            }
            return result;
        }));
    } else {
        m_pendingTempDir = m_backupDir + "/temp_restore_" + QString::number(QDateTime::currentMSecsSinceEpoch());
        QDir().mkpath(m_pendingTempDir);
        QString archivePath = backup.archivePath;
        QString tempDir = m_pendingTempDir;

        m_restoreWatcher.setFuture(QtConcurrent::run([archivePath, tempDir]() -> AsyncResult {
            AsyncResult result;
            result.success = extractArchive(archivePath, tempDir);
            if (!result.success) {
                result.errorMessage = "Failed to extract backup archive";
            }
            return result;
        }));
    }
}

void SaveManager::onAsyncRestoreFinished()
{
    AsyncResult result = m_restoreWatcher.result();
    m_busy = false;

    if (m_cancelRequested) {
        if (!m_pendingTempDir.isEmpty()) {
            removeDirectory(m_pendingTempDir);
            m_pendingTempDir.clear();
        }
        emit operationCancelled();
        return;
    }

    if (!result.success) {
        if (!m_pendingTempDir.isEmpty()) {
            removeDirectory(m_pendingTempDir);
            m_pendingTempDir.clear();
        }
        emit error(result.errorMessage);
    } else if (m_pendingIsProfile) {
        emit backupRestored(m_pendingBackup.gameId, m_pendingBackup.id);
    } else {
        // Full restore: move from temp dir to target
        QString targetPath = m_pendingRestoreTarget;
        if (QFile::exists(targetPath)) {
            removeDirectory(targetPath);
        }

        QDir tempDirObj(m_pendingTempDir);
        QStringList entries = tempDirObj.entryList(QDir::NoDotAndDotDot | QDir::AllEntries);
        QString sourceDir = m_pendingTempDir;
        if (entries.size() == 1) {
            QFileInfo entryInfo(m_pendingTempDir + "/" + entries.first());
            if (entryInfo.isDir()) {
                sourceDir = entryInfo.absoluteFilePath();
            }
        }

        if (copyDirectory(sourceDir, targetPath)) {
            emit backupRestored(m_pendingBackup.gameId, m_pendingBackup.id);
        } else {
            emit error("Failed to restore backup to target location");
        }
        removeDirectory(m_pendingTempDir);
        m_pendingTempDir.clear();
    }
    emit operationFinished();
}

void SaveManager::cancelOperation()
{
    m_cancelRequested = true;
    // The running QtConcurrent task will complete, but we'll discard the result
}

QStringList SaveManager::getAllGameIdsWithBackups() const
{
    QStringList gameIds;
    QDir gamesDir(m_backupDir + "/games");
    if (!gamesDir.exists()) return gameIds;

    QStringList subdirs = gamesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &subdir : subdirs) {
        QDir gameDir(gamesDir.absoluteFilePath(subdir));
        QStringList archives = gameDir.entryList(QStringList() << "*.tar.gz", QDir::Files);
        if (!archives.isEmpty()) {
            gameIds.append(subdir);
        }
    }
    return gameIds;
}

QString SaveManager::getGameNameFromBackups(const QString &gameId) const
{
    QList<BackupInfo> backups = getBackupsForGame(gameId);
    if (!backups.isEmpty()) {
        return backups.first().gameName;
    }
    return gameId;
}

QString SaveManager::getGameBackupDir(const QString &gameId) const
{
    return m_backupDir + "/games/" + gameId;
}

QString SaveManager::generateBackupId() const
{
    return QString::number(QDateTime::currentMSecsSinceEpoch());
}

// --- libarchive-based compression/extraction ---

void SaveManager::addDirectoryToArchive(struct archive *a, const QString &baseDir,
                                         const QString &relativePath)
{
    QDir dir(baseDir + "/" + relativePath);
    QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden,
                                              QDir::DirsFirst);

    for (const QFileInfo &fi : entries) {
        QString entryRelPath = relativePath.isEmpty()
            ? fi.fileName()
            : relativePath + "/" + fi.fileName();

        if (fi.isDir()) {
            // Write directory entry
            struct archive_entry *entry = archive_entry_new();
            archive_entry_set_pathname(entry, entryRelPath.toUtf8().constData());
            archive_entry_set_filetype(entry, AE_IFDIR);
            archive_entry_set_perm(entry, 0755);
            archive_entry_set_mtime(entry, fi.lastModified().toSecsSinceEpoch(), 0);
            archive_write_header(a, entry);
            archive_entry_free(entry);

            addDirectoryToArchive(a, baseDir, entryRelPath);
        } else if (fi.isFile()) {
            struct archive_entry *entry = archive_entry_new();
            archive_entry_set_pathname(entry, entryRelPath.toUtf8().constData());
            archive_entry_set_filetype(entry, AE_IFREG);
            archive_entry_set_perm(entry, fi.isExecutable() ? 0755 : 0644);
            archive_entry_set_size(entry, fi.size());
            archive_entry_set_mtime(entry, fi.lastModified().toSecsSinceEpoch(), 0);
            archive_write_header(a, entry);

            QFile file(fi.absoluteFilePath());
            if (file.open(QIODevice::ReadOnly)) {
                char buf[8192];
                qint64 bytesRead;
                while ((bytesRead = file.read(buf, sizeof(buf))) > 0) {
                    archive_write_data(a, buf, static_cast<size_t>(bytesRead));
                }
            }

            archive_entry_free(entry);
        } else if (fi.isSymLink()) {
            struct archive_entry *entry = archive_entry_new();
            archive_entry_set_pathname(entry, entryRelPath.toUtf8().constData());
            archive_entry_set_filetype(entry, AE_IFLNK);
            archive_entry_set_symlink(entry, fi.symLinkTarget().toUtf8().constData());
            archive_entry_set_perm(entry, 0777);
            archive_write_header(a, entry);
            archive_entry_free(entry);
        }
    }
}

bool SaveManager::compressDirectory(const QString &sourceDir, const QString &archivePath,
                                     int compressionLevel)
{
    QFileInfo sourceInfo(sourceDir);
    if (!sourceInfo.exists() || !sourceInfo.isDir()) {
        qWarning() << "Source directory does not exist:" << sourceDir;
        return false;
    }

    struct archive *a = archive_write_new();
    archive_write_add_filter_gzip(a);
    archive_write_set_format_pax_restricted(a);

    // Set compression level
    QString filterOpts = QString("gzip:compression-level=%1").arg(compressionLevel);
    archive_write_set_options(a, filterOpts.toUtf8().constData());

    if (archive_write_open_filename(a, archivePath.toLocal8Bit().constData()) != ARCHIVE_OK) {
        qWarning() << "Failed to open archive for writing:" << archive_error_string(a);
        archive_write_free(a);
        return false;
    }

    // Use the directory name as the top-level entry (like tar does)
    QString dirName = sourceInfo.fileName();
    QString parentDir = sourceInfo.absolutePath();

    // Write the top-level directory entry
    struct archive_entry *dirEntry = archive_entry_new();
    archive_entry_set_pathname(dirEntry, dirName.toUtf8().constData());
    archive_entry_set_filetype(dirEntry, AE_IFDIR);
    archive_entry_set_perm(dirEntry, 0755);
    archive_entry_set_mtime(dirEntry, sourceInfo.lastModified().toSecsSinceEpoch(), 0);
    archive_write_header(a, dirEntry);
    archive_entry_free(dirEntry);

    // Add all contents under the directory name prefix.
    // We use parentDir as baseDir and dirName as the relative prefix so that
    // addDirectoryToArchive produces paths like "dirName/subdir/file.txt".
    addDirectoryToArchive(a, parentDir, dirName);

    archive_write_close(a);
    archive_write_free(a);
    return true;
}

bool SaveManager::compressFiles(const QString &baseDir, const QStringList &relativePaths,
                                const QString &archivePath, int compressionLevel)
{
    struct archive *a = archive_write_new();
    archive_write_add_filter_gzip(a);
    archive_write_set_format_pax_restricted(a);

    QString filterOpts = QString("gzip:compression-level=%1").arg(compressionLevel);
    archive_write_set_options(a, filterOpts.toUtf8().constData());

    if (archive_write_open_filename(a, archivePath.toLocal8Bit().constData()) != ARCHIVE_OK) {
        qWarning() << "Failed to open archive for writing:" << archive_error_string(a);
        archive_write_free(a);
        return false;
    }

    int filesAdded = 0;
    for (const QString &relPath : relativePaths) {
        QString fullPath = baseDir + "/" + relPath;
        QFileInfo fi(fullPath);
        if (!fi.exists()) {
            qWarning() << "Profile file not found, skipping:" << fullPath;
            continue;
        }

        if (fi.isFile()) {
            struct archive_entry *entry = archive_entry_new();
            archive_entry_set_pathname(entry, relPath.toUtf8().constData());
            archive_entry_set_filetype(entry, AE_IFREG);
            archive_entry_set_perm(entry, fi.isExecutable() ? 0755 : 0644);
            archive_entry_set_size(entry, fi.size());
            archive_entry_set_mtime(entry, fi.lastModified().toSecsSinceEpoch(), 0);
            archive_write_header(a, entry);

            QFile file(fullPath);
            if (file.open(QIODevice::ReadOnly)) {
                char buf[8192];
                qint64 bytesRead;
                while ((bytesRead = file.read(buf, sizeof(buf))) > 0) {
                    archive_write_data(a, buf, static_cast<size_t>(bytesRead));
                }
            }
            archive_entry_free(entry);
            filesAdded++;
        } else if (fi.isDir()) {
            struct archive_entry *entry = archive_entry_new();
            archive_entry_set_pathname(entry, relPath.toUtf8().constData());
            archive_entry_set_filetype(entry, AE_IFDIR);
            archive_entry_set_perm(entry, 0755);
            archive_entry_set_mtime(entry, fi.lastModified().toSecsSinceEpoch(), 0);
            archive_write_header(a, entry);
            archive_entry_free(entry);

            addDirectoryToArchive(a, baseDir, relPath);
            filesAdded++;
        }
    }

    archive_write_close(a);
    archive_write_free(a);

    if (filesAdded == 0) {
        qWarning() << "No profile files found on disk";
        QFile::remove(archivePath);
        return false;
    }

    return true;
}

bool SaveManager::extractArchive(const QString &archivePath, const QString &targetDir)
{
    QDir().mkpath(targetDir);

    struct archive *a = archive_read_new();
    archive_read_support_filter_gzip(a);
    archive_read_support_format_tar(a);

    struct archive *ext = archive_write_disk_new();
    archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM);
    archive_write_disk_set_standard_lookup(ext);

    if (archive_read_open_filename(a, archivePath.toLocal8Bit().constData(), 10240) != ARCHIVE_OK) {
        qWarning() << "Failed to open archive:" << archive_error_string(a);
        archive_read_free(a);
        archive_write_free(ext);
        return false;
    }

    bool success = true;
    struct archive_entry *entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        // Prepend target directory to entry pathname
        QString entryPath = targetDir + "/" + QString::fromUtf8(archive_entry_pathname(entry));
        archive_entry_set_pathname(entry, entryPath.toLocal8Bit().constData());

        int r = archive_write_header(ext, entry);
        if (r != ARCHIVE_OK) {
            qWarning() << "Extract header error:" << archive_error_string(ext);
            success = false;
            break;
        }

        if (archive_entry_size(entry) > 0) {
            const void *buff;
            size_t size;
            la_int64_t offset;
            while (archive_read_data_block(a, &buff, &size, &offset) == ARCHIVE_OK) {
                if (archive_write_data_block(ext, buff, size, offset) != ARCHIVE_OK) {
                    qWarning() << "Extract write error:" << archive_error_string(ext);
                    success = false;
                    break;
                }
            }
        }
        archive_write_finish_entry(ext);

        if (!success) break;
    }

    if (archive_errno(a) != 0) {
        qWarning() << "Archive read error:" << archive_error_string(a);
        success = false;
    }

    archive_read_free(a);
    archive_write_free(ext);
    return success;
}

bool SaveManager::restoreProfileBackup(const BackupInfo &backup, const QString &targetPath)
{
    QDir().mkpath(targetPath);

    if (!extractArchive(backup.archivePath, targetPath)) {
        emit error("Failed to restore profile backup");
        return false;
    }

    emit backupRestored(backup.gameId, backup.id);
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
