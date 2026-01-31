#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include "core/savemanager.h"
#include "core/gameinfo.h"

static int s_testCounter = 0;

class TestSaveManager : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;
    SaveManager *m_mgr = nullptr;
    QString m_backupDir;
    QString m_saveDir;

    void createSaveFiles()
    {
        QDir().mkpath(m_saveDir);
        QFile f1(m_saveDir + "/save.dat");
        if (!f1.open(QIODevice::WriteOnly))
            qFatal("Failed to create save.dat");
        f1.write("save data content 12345");
        f1.close();

        QFile f2(m_saveDir + "/config.ini");
        if (!f2.open(QIODevice::WriteOnly))
            qFatal("Failed to create config.ini");
        f2.write("[settings]\nvolume=80\n");
        f2.close();

        QDir().mkpath(m_saveDir + "/subdir");
        QFile f3(m_saveDir + "/subdir/extra.bin");
        if (!f3.open(QIODevice::WriteOnly))
            qFatal("Failed to create extra.bin");
        f3.write("binary content");
        f3.close();
    }

    GameInfo makeGame(const QString &id, const QString &name)
    {
        GameInfo game;
        game.id = id;
        game.name = name;
        game.platform = "native";
        game.detectedSavePath = m_saveDir;
        game.isDetected = true;
        return game;
    }

private slots:
    void init()
    {
        ++s_testCounter;
        m_backupDir = m_tmpDir.path() + "/backups_" + QString::number(s_testCounter);
        m_saveDir = m_tmpDir.path() + "/saves_" + QString::number(s_testCounter);

        m_mgr = new SaveManager(this);
        m_mgr->setBackupDirectory(m_backupDir);
    }

    void cleanup()
    {
        delete m_mgr;
        m_mgr = nullptr;
    }

    // --- Basic operations ---

    void createBackup_success()
    {
        createSaveFiles();
        GameInfo game = makeGame("game1", "Test Game");

        QSignalSpy spy(m_mgr, &SaveManager::backupCreated);
        QVERIFY(m_mgr->createBackup(game, "My Backup", "Some notes"));
        QCOMPARE(spy.count(), 1);

        QList<BackupInfo> backups = m_mgr->getBackupsForGame("game1");
        QCOMPARE(backups.size(), 1);
        QCOMPARE(backups[0].displayName, QString("My Backup"));
        QCOMPARE(backups[0].notes, QString("Some notes"));
        QCOMPARE(backups[0].gameId, QString("game1"));
        QCOMPARE(backups[0].gameName, QString("Test Game"));
        QVERIFY(backups[0].size > 0);
        QVERIFY(QFile::exists(backups[0].archivePath));
    }

    void createBackup_defaultName()
    {
        createSaveFiles();
        GameInfo game = makeGame("game2", "Game Two");

        QVERIFY(m_mgr->createBackup(game));

        QList<BackupInfo> backups = m_mgr->getBackupsForGame("game2");
        QCOMPARE(backups.size(), 1);
        // Default name is timestamp format yyyy-MM-dd HH:mm:ss
        QVERIFY(backups[0].displayName.contains("-"));
        QVERIFY(backups[0].displayName.contains(":"));
    }

    void createBackup_noSavePath()
    {
        GameInfo game;
        game.id = "bad-game";
        game.name = "Bad";
        game.isDetected = false;

        QSignalSpy errorSpy(m_mgr, &SaveManager::error);
        QVERIFY(!m_mgr->createBackup(game));
        QCOMPARE(errorSpy.count(), 1);
    }

    void createBackup_nonexistentPath()
    {
        GameInfo game;
        game.id = "nopath";
        game.name = "No Path";
        game.isDetected = true;
        game.detectedSavePath = "/nonexistent/save/path";

        QSignalSpy errorSpy(m_mgr, &SaveManager::error);
        QVERIFY(!m_mgr->createBackup(game));
        QCOMPARE(errorSpy.count(), 1);
    }

    // --- Listing and retrieval ---

    void getBackupsForGame_sortedDescending()
    {
        createSaveFiles();
        GameInfo game = makeGame("sort-game", "Sort Game");

        m_mgr->createBackup(game, "First");
        QThread::sleep(1); // Ensure different second-precision timestamps (ISO date)
        m_mgr->createBackup(game, "Second");
        QThread::sleep(1);
        m_mgr->createBackup(game, "Third");

        QList<BackupInfo> backups = m_mgr->getBackupsForGame("sort-game");
        QCOMPARE(backups.size(), 3);
        // Sorted descending by timestamp
        QVERIFY(backups[0].timestamp >= backups[1].timestamp);
        QVERIFY(backups[1].timestamp >= backups[2].timestamp);
        // Most recent first
        QCOMPARE(backups[0].displayName, QString("Third"));
        QCOMPARE(backups[2].displayName, QString("First"));
    }

    void getBackupsForGame_empty()
    {
        QList<BackupInfo> backups = m_mgr->getBackupsForGame("nonexistent-game");
        QVERIFY(backups.isEmpty());
    }

    void getBackupById_found()
    {
        createSaveFiles();
        GameInfo game = makeGame("byid-game", "ById Game");
        m_mgr->createBackup(game, "Target");

        QList<BackupInfo> backups = m_mgr->getBackupsForGame("byid-game");
        QCOMPARE(backups.size(), 1);

        BackupInfo found = m_mgr->getBackupById("byid-game", backups[0].id);
        QCOMPARE(found.displayName, QString("Target"));
        QCOMPARE(found.id, backups[0].id);
    }

    void getBackupById_notFound()
    {
        BackupInfo result = m_mgr->getBackupById("game", "no-such-id");
        QVERIFY(result.id.isEmpty());
    }

    // --- Restore ---

    void restoreBackup_success()
    {
        createSaveFiles();
        GameInfo game = makeGame("restore-game", "Restore Game");
        m_mgr->createBackup(game, "Restorable");

        QList<BackupInfo> backups = m_mgr->getBackupsForGame("restore-game");
        QCOMPARE(backups.size(), 1);

        // Delete original saves
        QDir(m_saveDir).removeRecursively();
        QVERIFY(!QFile::exists(m_saveDir + "/save.dat"));

        // Restore
        QSignalSpy spy(m_mgr, &SaveManager::backupRestored);
        QVERIFY(m_mgr->restoreBackup(backups[0], m_saveDir));
        QCOMPARE(spy.count(), 1);

        // Verify files restored
        QVERIFY(QFile::exists(m_saveDir + "/save.dat"));
        QVERIFY(QFile::exists(m_saveDir + "/config.ini"));
        QVERIFY(QFile::exists(m_saveDir + "/subdir/extra.bin"));
    }

    void restoreBackup_missingArchive()
    {
        BackupInfo fake;
        fake.archivePath = "/nonexistent/archive.tar.gz";
        fake.gameId = "fake";

        QSignalSpy errorSpy(m_mgr, &SaveManager::error);
        QVERIFY(!m_mgr->restoreBackup(fake, m_saveDir));
        QCOMPARE(errorSpy.count(), 1);
    }

    // --- Delete ---

    void deleteBackup_success()
    {
        createSaveFiles();
        GameInfo game = makeGame("del-game", "Delete Game");
        m_mgr->createBackup(game, "To Delete");

        QList<BackupInfo> backups = m_mgr->getBackupsForGame("del-game");
        QCOMPARE(backups.size(), 1);

        QString archivePath = backups[0].archivePath;
        QString metadataPath = archivePath + ".json";
        QVERIFY(QFile::exists(archivePath));
        QVERIFY(QFile::exists(metadataPath));

        QSignalSpy spy(m_mgr, &SaveManager::backupDeleted);
        QVERIFY(m_mgr->deleteBackup(backups[0]));
        QCOMPARE(spy.count(), 1);

        QVERIFY(!QFile::exists(archivePath));
        QVERIFY(!QFile::exists(metadataPath));
    }

    // --- Update metadata ---

    void updateBackupMetadata_success()
    {
        createSaveFiles();
        GameInfo game = makeGame("meta-game", "Meta Game");
        m_mgr->createBackup(game, "Original Name", "Original notes");

        QList<BackupInfo> backups = m_mgr->getBackupsForGame("meta-game");
        QCOMPARE(backups.size(), 1);

        BackupInfo updated = backups[0];
        updated.displayName = "New Name";
        updated.notes = "New notes";

        QSignalSpy spy(m_mgr, &SaveManager::backupUpdated);
        QVERIFY(m_mgr->updateBackupMetadata(updated));
        QCOMPARE(spy.count(), 1);

        // Re-read and verify
        QList<BackupInfo> reloaded = m_mgr->getBackupsForGame("meta-game");
        QCOMPARE(reloaded[0].displayName, QString("New Name"));
        QCOMPARE(reloaded[0].notes, QString("New notes"));
    }

    void updateBackupMetadata_missingArchive()
    {
        BackupInfo fake;
        fake.archivePath = "/nonexistent.tar.gz";

        QSignalSpy errorSpy(m_mgr, &SaveManager::error);
        QVERIFY(!m_mgr->updateBackupMetadata(fake));
        QCOMPARE(errorSpy.count(), 1);
    }

    // --- Verify integrity ---

    void verifyBackup_validArchive()
    {
        createSaveFiles();
        GameInfo game = makeGame("verify-game", "Verify Game");
        m_mgr->createBackup(game, "Valid");

        QList<BackupInfo> backups = m_mgr->getBackupsForGame("verify-game");
        QCOMPARE(backups.size(), 1);

        QSignalSpy spy(m_mgr, &SaveManager::backupVerified);
        QVERIFY(m_mgr->verifyBackup(backups[0]));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][2].toBool(), true); // valid = true
    }

    void verifyBackup_corruptedArchive()
    {
        createSaveFiles();
        GameInfo game = makeGame("corrupt-game", "Corrupt Game");
        m_mgr->createBackup(game, "Corrupt");

        QList<BackupInfo> backups = m_mgr->getBackupsForGame("corrupt-game");
        QCOMPARE(backups.size(), 1);

        // Corrupt the archive by overwriting with garbage
        QFile f(backups[0].archivePath);
        if (!f.open(QIODevice::WriteOnly))
            qFatal("Failed to corrupt archive");
        f.write("this is not a valid tar.gz");
        f.close();

        QSignalSpy spy(m_mgr, &SaveManager::backupVerified);
        QVERIFY(!m_mgr->verifyBackup(backups[0]));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][2].toBool(), false); // valid = false
    }

    void verifyBackup_missingFile()
    {
        BackupInfo fake;
        fake.archivePath = "/nonexistent.tar.gz";
        fake.gameId = "fake";
        fake.id = "fake-id";

        QVERIFY(!m_mgr->verifyBackup(fake));
    }

    // --- getAllGameIdsWithBackups ---

    void getAllGameIdsWithBackups_multipleGames()
    {
        createSaveFiles();
        GameInfo game1 = makeGame("ids-game1", "Game 1");
        GameInfo game2 = makeGame("ids-game2", "Game 2");

        m_mgr->createBackup(game1, "Backup 1");
        m_mgr->createBackup(game2, "Backup 2");

        QStringList ids = m_mgr->getAllGameIdsWithBackups();
        QVERIFY(ids.contains("ids-game1"));
        QVERIFY(ids.contains("ids-game2"));
    }

    void getAllGameIdsWithBackups_empty()
    {
        QStringList ids = m_mgr->getAllGameIdsWithBackups();
        QVERIFY(ids.isEmpty());
    }

    // --- getGameNameFromBackups ---

    void getGameNameFromBackups_found()
    {
        createSaveFiles();
        GameInfo game = makeGame("name-game", "My Cool Game");
        m_mgr->createBackup(game, "Backup");

        QString name = m_mgr->getGameNameFromBackups("name-game");
        QCOMPARE(name, QString("My Cool Game"));
    }

    void getGameNameFromBackups_notFound()
    {
        QString name = m_mgr->getGameNameFromBackups("no-such-game");
        // Falls back to gameId
        QCOMPARE(name, QString("no-such-game"));
    }

    // --- Compression level ---

    void compressionLevel_applies()
    {
        createSaveFiles();
        m_mgr->setCompressionLevel(1); // Fast

        GameInfo game = makeGame("comp-game", "Compression Game");
        QVERIFY(m_mgr->createBackup(game, "Fast Backup"));

        QList<BackupInfo> backups = m_mgr->getBackupsForGame("comp-game");
        QCOMPARE(backups.size(), 1);
        QVERIFY(backups[0].size > 0);
    }

    void compressionLevel_invalidIgnored()
    {
        m_mgr->setCompressionLevel(0);  // Below range
        m_mgr->setCompressionLevel(10); // Above range
        // Should not crash, level stays at default
        createSaveFiles();
        GameInfo game = makeGame("comp2", "Comp2");
        QVERIFY(m_mgr->createBackup(game, "Default"));
    }

    // --- Profile backup ---

    void profileBackup_specificFiles()
    {
        createSaveFiles();
        GameInfo game = makeGame("prof-game", "Profile Game");

        SaveProfile profile;
        profile.id = 1;
        profile.name = "Slot 1";
        profile.files << "save.dat"; // Only backup one file

        QVERIFY(m_mgr->createBackup(game, "Profile Backup", "", profile));

        QList<BackupInfo> backups = m_mgr->getBackupsForGame("prof-game");
        QCOMPARE(backups.size(), 1);
        QCOMPARE(backups[0].profileName, QString("Slot 1"));
        QCOMPARE(backups[0].profileId, 1);
        QVERIFY(backups[0].size > 0);

        // Restore to a new location and verify only the profiled file is there
        QString restoreDir = m_tmpDir.path() + "/profile_restore";
        QVERIFY(m_mgr->restoreBackup(backups[0], restoreDir));
        QVERIFY(QFile::exists(restoreDir + "/save.dat"));
        // config.ini and subdir should NOT be in the archive
        QVERIFY(!QFile::exists(restoreDir + "/config.ini"));
    }

    // --- Backup directory ---

    void backupDirectory_setAndGet()
    {
        QString dir = m_tmpDir.path() + "/custom_backup_dir";
        m_mgr->setBackupDirectory(dir);
        QCOMPARE(m_mgr->getBackupDirectory(), dir);
        QVERIFY(QDir(dir).exists());
    }

    // --- isBusy ---

    void isBusy_initiallyFalse()
    {
        QVERIFY(!m_mgr->isBusy());
    }
};

QTEST_MAIN(TestSaveManager)
#include "test_savemanager.moc"
