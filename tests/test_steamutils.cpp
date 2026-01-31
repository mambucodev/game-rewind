#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QThread>
#include "steam/steamutils.h"

class TestSteamUtils : public QObject {
    Q_OBJECT

private:
    void writeFile(const QString &path, const QString &content)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
            qFatal("Failed to open %s for writing", qPrintable(path));
        QTextStream out(&f);
        out << content;
    }

private slots:
    // --- VDF parsing ---

    void parseVdf_simpleKeyValue()
    {
        QTemporaryDir tmp;
        QString path = tmp.path() + "/test.vdf";
        writeFile(path,
            "\"root\"\n"
            "{\n"
            "    \"key1\"    \"value1\"\n"
            "    \"key2\"    \"value2\"\n"
            "}\n");

        QVariantMap result = SteamUtils::parseVdf(path);
        QVariantMap root = result["root"].toMap();
        QCOMPARE(root["key1"].toString(), QString("value1"));
        QCOMPARE(root["key2"].toString(), QString("value2"));
    }

    void parseVdf_nestedBlocks()
    {
        QTemporaryDir tmp;
        QString path = tmp.path() + "/library.vdf";
        writeFile(path,
            "\"libraryfolders\"\n"
            "{\n"
            "    \"0\"\n"
            "    {\n"
            "        \"path\"    \"/home/user/.local/share/Steam\"\n"
            "        \"label\"   \"\"\n"
            "    }\n"
            "    \"1\"\n"
            "    {\n"
            "        \"path\"    \"/mnt/games/SteamLibrary\"\n"
            "        \"label\"   \"Games\"\n"
            "    }\n"
            "}\n");

        QVariantMap result = SteamUtils::parseVdf(path);
        QVariantMap lf = result["libraryfolders"].toMap();
        QCOMPARE(lf.size(), 2);

        QVariantMap entry0 = lf["0"].toMap();
        QCOMPARE(entry0["path"].toString(), QString("/home/user/.local/share/Steam"));

        QVariantMap entry1 = lf["1"].toMap();
        QCOMPARE(entry1["path"].toString(), QString("/mnt/games/SteamLibrary"));
        QCOMPARE(entry1["label"].toString(), QString("Games"));
    }

    void parseVdf_escapedQuotes()
    {
        QTemporaryDir tmp;
        QString path = tmp.path() + "/escaped.vdf";
        writeFile(path,
            "\"root\"\n"
            "{\n"
            "    \"name\"    \"hello \\\"world\\\"\"\n"
            "}\n");

        QVariantMap result = SteamUtils::parseVdf(path);
        QVariantMap root = result["root"].toMap();
        QCOMPARE(root["name"].toString(), QString("hello \\\"world\\\""));
    }

    void parseVdf_commentsSkipped()
    {
        QTemporaryDir tmp;
        QString path = tmp.path() + "/comments.vdf";
        writeFile(path,
            "// This is a comment\n"
            "\"root\"\n"
            "{\n"
            "    // Another comment\n"
            "    \"key\"    \"value\"\n"
            "}\n");

        QVariantMap result = SteamUtils::parseVdf(path);
        QVariantMap root = result["root"].toMap();
        QCOMPARE(root["key"].toString(), QString("value"));
    }

    void parseVdf_emptyFile()
    {
        QTemporaryDir tmp;
        QString path = tmp.path() + "/empty.vdf";
        writeFile(path, "");

        QVariantMap result = SteamUtils::parseVdf(path);
        QVERIFY(result.isEmpty());
    }

    void parseVdf_nonExistentFile()
    {
        QVariantMap result = SteamUtils::parseVdf("/nonexistent/path.vdf");
        QVERIFY(result.isEmpty());
    }

    void parseVdf_malformedInput()
    {
        QTemporaryDir tmp;
        QString path = tmp.path() + "/bad.vdf";
        writeFile(path, "{{{{ garbage \"\" }}}}");

        // Should not crash -- just return whatever it can parse
        QVariantMap result = SteamUtils::parseVdf(path);
        Q_UNUSED(result);
    }

    // --- App manifest parsing ---

    void parseAppManifest_normal()
    {
        QTemporaryDir tmp;
        QString path = tmp.path() + "/appmanifest_440.acf";
        writeFile(path,
            "\"AppState\"\n"
            "{\n"
            "    \"appid\"       \"440\"\n"
            "    \"name\"        \"Team Fortress 2\"\n"
            "    \"installdir\"  \"Team Fortress 2\"\n"
            "    \"StateFlags\"  \"4\"\n"
            "}\n");

        SteamAppInfo info = SteamUtils::parseAppManifest(path, "/steam/library");
        QCOMPARE(info.appId, QString("440"));
        QCOMPARE(info.name, QString("Team Fortress 2"));
        QCOMPARE(info.installDir, QString("Team Fortress 2"));
        QCOMPARE(info.libraryPath, QString("/steam/library"));
    }

    void parseAppManifest_missingFields()
    {
        QTemporaryDir tmp;
        QString path = tmp.path() + "/appmanifest_123.acf";
        writeFile(path,
            "\"AppState\"\n"
            "{\n"
            "    \"appid\"    \"123\"\n"
            "    \"StateFlags\"  \"4\"\n"
            "}\n");

        SteamAppInfo info = SteamUtils::parseAppManifest(path, "/lib");
        QCOMPARE(info.appId, QString("123"));
        QVERIFY(info.name.isEmpty());
        QVERIFY(info.installDir.isEmpty());
    }

    void parseAppManifest_emptyFile()
    {
        QTemporaryDir tmp;
        QString path = tmp.path() + "/appmanifest_0.acf";
        writeFile(path, "");

        SteamAppInfo info = SteamUtils::parseAppManifest(path, "/lib");
        QVERIFY(info.appId.isEmpty());
        QVERIFY(info.name.isEmpty());
    }

    void parseAppManifest_nonExistent()
    {
        SteamAppInfo info = SteamUtils::parseAppManifest("/nonexistent.acf", "/lib");
        QVERIFY(info.appId.isEmpty());
    }

    // --- findProtonPrefix ---

    void findProtonPrefix_found()
    {
        QTemporaryDir tmp;
        QString lib = tmp.path() + "/lib1";
        QDir().mkpath(lib + "/steamapps/compatdata/12345/pfx");

        QStringList folders = {lib};
        QString result = SteamUtils::findProtonPrefix("12345", folders);
        QCOMPARE(result, lib + "/steamapps/compatdata/12345/pfx");
    }

    void findProtonPrefix_notFound()
    {
        QTemporaryDir tmp;
        QString lib = tmp.path() + "/lib1";
        QDir().mkpath(lib + "/steamapps/compatdata/99999/pfx");

        QStringList folders = {lib};
        QString result = SteamUtils::findProtonPrefix("11111", folders);
        QVERIFY(result.isEmpty());
    }

    void findProtonPrefix_multipleLibraries()
    {
        QTemporaryDir tmp;
        QString lib1 = tmp.path() + "/lib1";
        QString lib2 = tmp.path() + "/lib2";
        QDir().mkpath(lib1 + "/steamapps");
        QDir().mkpath(lib2 + "/steamapps/compatdata/555/pfx");

        QStringList folders = {lib1, lib2};
        QString result = SteamUtils::findProtonPrefix("555", folders);
        QCOMPARE(result, lib2 + "/steamapps/compatdata/555/pfx");
    }

    // --- getSteamUserId ---

    void getSteamUserId_singleUser()
    {
        QTemporaryDir tmp;
        QString steamPath = tmp.path();
        QDir().mkpath(steamPath + "/userdata/12345678");

        QString userId = SteamUtils::getSteamUserId(steamPath);
        QCOMPARE(userId, QString("12345678"));
    }

    void getSteamUserId_noUsers()
    {
        QTemporaryDir tmp;
        QString steamPath = tmp.path();
        QDir().mkpath(steamPath + "/userdata");

        QString userId = SteamUtils::getSteamUserId(steamPath);
        QVERIFY(userId.isEmpty());
    }

    void getSteamUserId_emptyPath()
    {
        QString userId = SteamUtils::getSteamUserId("");
        QVERIFY(userId.isEmpty());
    }

    void getSteamUserId_multipleUsers()
    {
        QTemporaryDir tmp;
        QString steamPath = tmp.path();
        QDir().mkpath(steamPath + "/userdata/111");
        QDir().mkpath(steamPath + "/userdata/222");

        // Touch the second dir to make it more recent
        QThread::msleep(50);
        writeFile(steamPath + "/userdata/222/.touch", "");

        QString userId = SteamUtils::getSteamUserId(steamPath);
        // Should return the most recently modified
        QVERIFY(!userId.isEmpty());
    }

    // --- getLibraryFolders ---

    void getLibraryFolders_emptyPath()
    {
        QStringList folders = SteamUtils::getLibraryFolders("");
        QVERIFY(folders.isEmpty());
    }

    void getLibraryFolders_noVdf()
    {
        QTemporaryDir tmp;
        QString steamPath = tmp.path();
        QDir().mkpath(steamPath + "/steamapps");

        QStringList folders = SteamUtils::getLibraryFolders(steamPath);
        // Should at least return the steam path itself
        QCOMPARE(folders.size(), 1);
        QCOMPARE(folders.first(), steamPath);
    }

    void getLibraryFolders_withAdditionalLibraries()
    {
        QTemporaryDir tmp;
        QString steamPath = tmp.path() + "/steam";
        QString extraLib = tmp.path() + "/games";
        QDir().mkpath(steamPath + "/steamapps");
        QDir().mkpath(extraLib);

        writeFile(steamPath + "/steamapps/libraryfolders.vdf",
            "\"libraryfolders\"\n"
            "{\n"
            "    \"0\"\n"
            "    {\n"
            "        \"path\"    \"" + steamPath + "\"\n"
            "    }\n"
            "    \"1\"\n"
            "    {\n"
            "        \"path\"    \"" + extraLib + "\"\n"
            "    }\n"
            "}\n");

        QStringList folders = SteamUtils::getLibraryFolders(steamPath);
        QVERIFY(folders.size() >= 2);
        QCOMPARE(folders.first(), steamPath);
        QVERIFY(folders.contains(extraLib));
    }

    // --- scanInstalledGames ---

    void scanInstalledGames_findsGames()
    {
        QTemporaryDir tmp;
        QString lib = tmp.path();
        QDir().mkpath(lib + "/steamapps");

        writeFile(lib + "/steamapps/appmanifest_440.acf",
            "\"AppState\"\n{\n    \"appid\"    \"440\"\n    \"name\"    \"TF2\"\n    \"installdir\"    \"tf2\"\n}\n");
        writeFile(lib + "/steamapps/appmanifest_570.acf",
            "\"AppState\"\n{\n    \"appid\"    \"570\"\n    \"name\"    \"Dota 2\"\n    \"installdir\"    \"dota2\"\n}\n");

        QList<SteamAppInfo> games = SteamUtils::scanInstalledGames({lib});
        QCOMPARE(games.size(), 2);
        // Sorted alphabetically
        QCOMPARE(games[0].name, QString("Dota 2"));
        QCOMPARE(games[1].name, QString("TF2"));
    }

    void scanInstalledGames_emptyLibrary()
    {
        QTemporaryDir tmp;
        QString lib = tmp.path();
        QDir().mkpath(lib + "/steamapps");

        QList<SteamAppInfo> games = SteamUtils::scanInstalledGames({lib});
        QVERIFY(games.isEmpty());
    }
};

QTEST_MAIN(TestSteamUtils)
#include "test_steamutils.moc"
