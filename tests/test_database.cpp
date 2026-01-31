#include <QTest>
#include <QTemporaryDir>
#include <QSqlDatabase>
#include "core/database.h"
#include "core/gameinfo.h"

class TestDatabase : public QObject {
    Q_OBJECT

private:
    QTemporaryDir m_tmpDir;
    Database *m_db = nullptr;

    Database *createTestDb()
    {
        // Database uses QStandardPaths which we can't easily override,
        // so we set XDG_DATA_HOME to redirect it
        qputenv("XDG_DATA_HOME", m_tmpDir.path().toUtf8());
        auto *db = new Database(this);
        if (!db->open()) {
            qWarning() << "Failed to open database";
            delete db;
            return nullptr;
        }
        return db;
    }

private slots:
    void init()
    {
        m_db = createTestDb();
    }

    void cleanup()
    {
        if (m_db) {
            m_db->close();
            delete m_db;
            m_db = nullptr;
        }
        // Clean up connections
        QSqlDatabase::removeDatabase("game-rewind-db");
    }

    // --- Open / Schema ---

    void openCreatesSchema()
    {
        QVERIFY(m_db != nullptr);
        QVERIFY(QFile::exists(m_db->databasePath()));
    }

    // --- Custom Game CRUD ---

    void addAndGetCustomGame()
    {
        GameInfo game;
        game.id = "test-game";
        game.name = "Test Game";
        game.platform = "native";
        game.savePaths << "/home/user/.saves/test";

        QVERIFY(m_db->addCustomGame(game));
        QVERIFY(m_db->customGameExists("test-game"));

        GameInfo retrieved = m_db->getCustomGame("test-game");
        QCOMPARE(retrieved.id, game.id);
        QCOMPARE(retrieved.name, game.name);
        QCOMPARE(retrieved.platform, game.platform);
        QCOMPARE(retrieved.savePaths, game.savePaths);
        QCOMPARE(retrieved.source, QString("database"));
    }

    void getAllCustomGames()
    {
        GameInfo g1;
        g1.id = "game-b";
        g1.name = "Beta Game";
        g1.platform = "native";
        g1.savePaths << "/saves/b";

        GameInfo g2;
        g2.id = "game-a";
        g2.name = "Alpha Game";
        g2.platform = "steam";
        g2.savePaths << "/saves/a";

        m_db->addCustomGame(g1);
        m_db->addCustomGame(g2);

        QList<GameInfo> all = m_db->getAllCustomGames();
        // Default Minetest seed + our 2 = 3, sorted by name
        // Alpha Game, Beta Game, Minetest
        QVERIFY(all.size() >= 2);

        // Verify sorting: find positions
        int alphaIdx = -1, betaIdx = -1;
        for (int i = 0; i < all.size(); ++i) {
            if (all[i].id == "game-a") alphaIdx = i;
            if (all[i].id == "game-b") betaIdx = i;
        }
        QVERIFY(alphaIdx >= 0);
        QVERIFY(betaIdx >= 0);
        QVERIFY(alphaIdx < betaIdx); // Alpha before Beta
    }

    void updateCustomGame()
    {
        GameInfo game;
        game.id = "update-test";
        game.name = "Original";
        game.platform = "native";
        game.savePaths << "/saves/orig";
        m_db->addCustomGame(game);

        game.name = "Updated";
        game.savePaths.clear();
        game.savePaths << "/saves/new";
        QVERIFY(m_db->updateCustomGame(game));

        GameInfo retrieved = m_db->getCustomGame("update-test");
        QCOMPARE(retrieved.name, QString("Updated"));
        QCOMPARE(retrieved.savePaths.first(), QString("/saves/new"));
    }

    void removeCustomGame()
    {
        GameInfo game;
        game.id = "remove-test";
        game.name = "To Remove";
        game.platform = "native";
        game.savePaths << "/saves/rm";
        m_db->addCustomGame(game);

        QVERIFY(m_db->customGameExists("remove-test"));
        QVERIFY(m_db->removeCustomGame("remove-test"));
        QVERIFY(!m_db->customGameExists("remove-test"));
    }

    void removeNonexistentGame()
    {
        QVERIFY(!m_db->removeCustomGame("doesnt-exist"));
    }

    void getCustomGameNonexistent()
    {
        GameInfo result = m_db->getCustomGame("no-such-game");
        QVERIFY(result.id.isEmpty());
    }

    // --- Hidden Games ---

    void hideAndUnhideGame()
    {
        QVERIFY(m_db->hideGame("hidden-1", "Hidden Game"));
        QVERIFY(m_db->isGameHidden("hidden-1"));

        QSet<QString> ids = m_db->getHiddenGameIds();
        QVERIFY(ids.contains("hidden-1"));

        QVERIFY(m_db->unhideGame("hidden-1"));
        QVERIFY(!m_db->isGameHidden("hidden-1"));
    }

    void hideGameIdempotent()
    {
        QVERIFY(m_db->hideGame("idem-1", "Game"));
        QVERIFY(m_db->hideGame("idem-1", "Game Updated")); // INSERT OR REPLACE
        QVERIFY(m_db->isGameHidden("idem-1"));

        QList<QPair<QString, QString>> hidden = m_db->getHiddenGames();
        bool found = false;
        for (const auto &pair : hidden) {
            if (pair.first == "idem-1") {
                QCOMPARE(pair.second, QString("Game Updated"));
                found = true;
            }
        }
        QVERIFY(found);
    }

    void unhideNonexistent()
    {
        QVERIFY(!m_db->unhideGame("never-hidden"));
    }

    // --- App Settings ---

    void setAndGetSetting()
    {
        QVERIFY(m_db->setSetting("theme", "dark"));
        QCOMPARE(m_db->getSetting("theme"), QString("dark"));
    }

    void getSettingDefault()
    {
        QCOMPARE(m_db->getSetting("nonexistent", "fallback"), QString("fallback"));
    }

    void getSettingNoDefault()
    {
        QCOMPARE(m_db->getSetting("nonexistent"), QString());
    }

    void settingOverwrite()
    {
        m_db->setSetting("key", "first");
        m_db->setSetting("key", "second");
        QCOMPARE(m_db->getSetting("key"), QString("second"));
    }

    // --- Save Profiles ---

    void addAndGetProfile()
    {
        SaveProfile p;
        p.gameId = "prof-game";
        p.name = "Slot 1";
        p.files << "save1.dat" << "config.ini";

        int id = m_db->addProfile(p);
        QVERIFY(id > 0);

        SaveProfile retrieved = m_db->getProfile(id);
        QCOMPARE(retrieved.id, id);
        QCOMPARE(retrieved.gameId, QString("prof-game"));
        QCOMPARE(retrieved.name, QString("Slot 1"));
        QCOMPARE(retrieved.files.size(), 2);
        QVERIFY(retrieved.files.contains("save1.dat"));
    }

    void getProfilesForGame()
    {
        SaveProfile p1;
        p1.gameId = "multi-prof";
        p1.name = "Slot A";
        p1.files << "a.dat";
        m_db->addProfile(p1);

        SaveProfile p2;
        p2.gameId = "multi-prof";
        p2.name = "Slot B";
        p2.files << "b.dat";
        m_db->addProfile(p2);

        QList<SaveProfile> profiles = m_db->getProfilesForGame("multi-prof");
        QCOMPARE(profiles.size(), 2);
    }

    void updateProfile()
    {
        SaveProfile p;
        p.gameId = "upd-prof";
        p.name = "Original";
        p.files << "old.dat";
        p.id = m_db->addProfile(p);

        p.name = "Renamed";
        p.files.clear();
        p.files << "new.dat";
        QVERIFY(m_db->updateProfile(p));

        SaveProfile retrieved = m_db->getProfile(p.id);
        QCOMPARE(retrieved.name, QString("Renamed"));
        QCOMPARE(retrieved.files.first(), QString("new.dat"));
    }

    void removeProfile()
    {
        SaveProfile p;
        p.gameId = "rm-prof";
        p.name = "ToDelete";
        p.files << "x.dat";
        int id = m_db->addProfile(p);

        QVERIFY(m_db->removeProfile(id));
        SaveProfile retrieved = m_db->getProfile(id);
        QVERIFY(retrieved.id == -1 || retrieved.gameId.isEmpty());
    }

    void profileExistsUniqueness()
    {
        SaveProfile p;
        p.gameId = "uniq-game";
        p.name = "Slot 1";
        p.files << "s.dat";
        QVERIFY(m_db->addProfile(p) > 0);

        QVERIFY(m_db->profileExists("uniq-game", "Slot 1"));
        QVERIFY(!m_db->profileExists("uniq-game", "Slot 2"));
    }

    // --- Save path serialization (tested indirectly) ---

    void savePathsRoundtrip()
    {
        GameInfo game;
        game.id = "serial-test";
        game.name = "Serialization Test";
        game.platform = "native";
        game.savePaths << "/path/one" << "/path/two" << "/path/three";
        m_db->addCustomGame(game);

        GameInfo retrieved = m_db->getCustomGame("serial-test");
        QCOMPARE(retrieved.savePaths.size(), 3);
        QCOMPARE(retrieved.savePaths[0], QString("/path/one"));
        QCOMPARE(retrieved.savePaths[1], QString("/path/two"));
        QCOMPARE(retrieved.savePaths[2], QString("/path/three"));
    }

    void savePathsEmpty()
    {
        GameInfo game;
        game.id = "empty-paths";
        game.name = "No Paths";
        game.platform = "native";
        // Empty savePaths
        m_db->addCustomGame(game);

        GameInfo retrieved = m_db->getCustomGame("empty-paths");
        QVERIFY(retrieved.savePaths.isEmpty());
    }
};

QTEST_MAIN(TestDatabase)
#include "test_database.moc"
