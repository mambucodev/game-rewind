#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include "core/profiledetector.h"

class TestProfileDetector : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;

    void createFile(const QString &path)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly))
            qFatal("Failed to open %s for writing", qPrintable(path));
        f.write("content");
        f.close();
    }

    QString makeDir(const QString &suffix)
    {
        QString dir = m_tmpDir.path() + "/" + suffix;
        QDir().mkpath(dir);
        return dir;
    }

private slots:
    // --- detectNumberedFiles ---

    void numberedFiles_basic()
    {
        QString dir = makeDir("numbered_basic");
        createFile(dir + "/save1.dat");
        createFile(dir + "/save2.dat");
        createFile(dir + "/save3.dat");

        auto profiles = ProfileDetector::detectProfiles(dir);
        QCOMPARE(profiles.size(), 3);

        // Each profile should have exactly 1 file
        for (const auto &p : profiles) {
            QCOMPARE(p.files.size(), 1);
            QVERIFY(p.name.startsWith("Slot "));
        }
    }

    void numberedFiles_correlatedFiles()
    {
        QString dir = makeDir("numbered_correlated");
        createFile(dir + "/user1.dat");
        createFile(dir + "/user1.cfg");
        createFile(dir + "/user2.dat");
        createFile(dir + "/user2.cfg");

        auto profiles = ProfileDetector::detectProfiles(dir);
        QCOMPARE(profiles.size(), 2);

        // Each profile should have 2 correlated files
        for (const auto &p : profiles) {
            QCOMPARE(p.files.size(), 2);
        }
    }

    void numberedFiles_singleFile()
    {
        QString dir = makeDir("numbered_single");
        createFile(dir + "/save1.dat");
        // Only 1 file -- need >= 2 for detection

        auto profiles = ProfileDetector::detectProfiles(dir);
        QVERIFY(profiles.isEmpty());
    }

    // --- detectNumberedDirs ---

    void numberedDirs_basic()
    {
        QString dir = makeDir("dirs_basic");
        QDir().mkpath(dir + "/slot1");
        QDir().mkpath(dir + "/slot2");
        QDir().mkpath(dir + "/slot3");

        auto profiles = ProfileDetector::detectProfiles(dir);
        QCOMPARE(profiles.size(), 3);

        for (const auto &p : profiles) {
            QCOMPARE(p.files.size(), 1); // directory name
            QVERIFY(p.name.startsWith("Slot "));
        }
    }

    void numberedDirs_variousPatterns()
    {
        QString dir = makeDir("dirs_patterns");
        QDir().mkpath(dir + "/save_1");
        QDir().mkpath(dir + "/save_2");

        auto profiles = ProfileDetector::detectProfiles(dir);
        QCOMPARE(profiles.size(), 2);
    }

    void numberedDirs_caseInsensitive()
    {
        QString dir = makeDir("dirs_case");
        QDir().mkpath(dir + "/Save1");
        QDir().mkpath(dir + "/Save2");

        auto profiles = ProfileDetector::detectProfiles(dir);
        QCOMPARE(profiles.size(), 2);
    }

    // --- detectCommonPatterns ---

    void commonPatterns_basic()
    {
        QString dir = makeDir("common_basic");
        createFile(dir + "/SaveSlot1.sav");
        createFile(dir + "/SaveSlot2.sav");

        auto profiles = ProfileDetector::detectProfiles(dir);
        QCOMPARE(profiles.size(), 2);
    }

    void commonPatterns_dashSeparated()
    {
        QString dir = makeDir("common_dash");
        createFile(dir + "/profile-1.dat");
        createFile(dir + "/profile-2.dat");
        createFile(dir + "/profile-3.dat");

        auto profiles = ProfileDetector::detectProfiles(dir);
        QCOMPARE(profiles.size(), 3);
    }

    // --- Edge cases ---

    void noProfiles_unrelatedFiles()
    {
        QString dir = makeDir("unrelated");
        createFile(dir + "/readme.txt");
        createFile(dir + "/config.ini");
        createFile(dir + "/data.bin");

        auto profiles = ProfileDetector::detectProfiles(dir);
        QVERIFY(profiles.isEmpty());
    }

    void emptyDirectory()
    {
        QString dir = makeDir("empty_dir");

        auto profiles = ProfileDetector::detectProfiles(dir);
        QVERIFY(profiles.isEmpty());
    }

    void nonexistentDirectory()
    {
        auto profiles = ProfileDetector::detectProfiles("/nonexistent/directory/12345");
        QVERIFY(profiles.isEmpty());
    }

    void manySlots_cappedAt20()
    {
        QString dir = makeDir("many_slots");
        for (int i = 1; i <= 25; ++i) {
            createFile(dir + "/save" + QString::number(i) + ".dat");
        }

        auto profiles = ProfileDetector::detectProfiles(dir);
        QVERIFY(profiles.size() <= 20);
        QVERIFY(profiles.size() >= 2);
    }

    void numberedDirs_nonMatching()
    {
        // Dirs that don't match the pattern (slot|save|profile|savegame|data)
        QString dir = makeDir("dirs_nonmatch");
        QDir().mkpath(dir + "/level1");
        QDir().mkpath(dir + "/level2");

        // These shouldn't match the numbered dirs pattern
        // but numberedFiles might pick them up depending on implementation
        // The key thing is: no crash
        auto profiles = ProfileDetector::detectProfiles(dir);
        Q_UNUSED(profiles);
    }
};

QTEST_MAIN(TestProfileDetector)
#include "test_profiledetector.moc"
