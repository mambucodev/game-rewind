#include "database.h"
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

Database::Database(QObject *parent)
    : QObject(parent)
    , m_connectionName("game-rewind-db")
{
    m_dbPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
               + "/game-rewind/games.db";
}

Database::~Database()
{
    close();
}

bool Database::open()
{
    QDir().mkpath(QFileInfo(m_dbPath).absolutePath());

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    db.setDatabaseName(m_dbPath);

    if (!db.open()) {
        qCritical() << "Failed to open database:" << db.lastError().text();
        return false;
    }

    // Enable WAL mode
    QSqlQuery query(db);
    query.exec("PRAGMA journal_mode=WAL");
    query.exec("PRAGMA foreign_keys=ON");

    if (!createTables()) {
        return false;
    }

    qDebug() << "Database opened:" << m_dbPath;
    return true;
}

void Database::close()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        QSqlDatabase::database(m_connectionName).close();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool Database::createTables()
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS schema_version ("
            "    version INTEGER NOT NULL"
            ")")) {
        qCritical() << "Failed to create schema_version table:" << query.lastError().text();
        return false;
    }

    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS custom_games ("
            "    id           TEXT PRIMARY KEY,"
            "    name         TEXT NOT NULL,"
            "    platform     TEXT NOT NULL DEFAULT 'custom',"
            "    steam_app_id TEXT,"
            "    save_paths   TEXT NOT NULL,"
            "    created_at   TEXT NOT NULL DEFAULT (datetime('now')),"
            "    updated_at   TEXT NOT NULL DEFAULT (datetime('now'))"
            ")")) {
        qCritical() << "Failed to create custom_games table:" << query.lastError().text();
        return false;
    }

    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS hidden_games ("
            "    game_id TEXT PRIMARY KEY,"
            "    name    TEXT NOT NULL DEFAULT ''"
            ")")) {
        qCritical() << "Failed to create hidden_games table:" << query.lastError().text();
        return false;
    }

    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS app_settings ("
            "    key   TEXT PRIMARY KEY,"
            "    value TEXT NOT NULL"
            ")")) {
        qCritical() << "Failed to create app_settings table:" << query.lastError().text();
        return false;
    }

    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS save_profiles ("
            "    id         INTEGER PRIMARY KEY AUTOINCREMENT,"
            "    game_id    TEXT NOT NULL,"
            "    name       TEXT NOT NULL,"
            "    files      TEXT NOT NULL,"
            "    created_at TEXT NOT NULL DEFAULT (datetime('now')),"
            "    updated_at TEXT NOT NULL DEFAULT (datetime('now')),"
            "    UNIQUE(game_id, name)"
            ")")) {
        qCritical() << "Failed to create save_profiles table:" << query.lastError().text();
        return false;
    }

    // Seed defaults on fresh install
    if (schemaVersion() == 0) {
        seedDefaults();
        setSchemaVersion(4);
    }

    // Migrate schema v1 -> v2 (hidden_games table already created above)
    if (schemaVersion() == 1) {
        setSchemaVersion(2);
    }

    // Migrate schema v2 -> v3 (app_settings table already created above)
    if (schemaVersion() == 2) {
        setSchemaVersion(3);
    }

    // Migrate schema v3 -> v4 (save_profiles table already created above)
    if (schemaVersion() == 3) {
        setSchemaVersion(4);
    }

    return true;
}

int Database::schemaVersion() const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    if (!query.exec("SELECT version FROM schema_version LIMIT 1")) {
        return 0;
    }

    if (query.next()) {
        return query.value(0).toInt();
    }

    return 0;
}

void Database::setSchemaVersion(int version)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    query.exec("DELETE FROM schema_version");
    query.prepare("INSERT INTO schema_version (version) VALUES (?)");
    query.addBindValue(version);
    query.exec();
}

void Database::seedDefaults()
{
    GameInfo minetest;
    minetest.id = "minetest";
    minetest.name = "Minetest";
    minetest.platform = "native";
    minetest.savePaths << "~/.minetest/worlds";
    minetest.source = "database";

    addCustomGame(minetest);
    qDebug() << "Seeded default games";
}

QList<GameInfo> Database::getAllCustomGames() const
{
    QList<GameInfo> games;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    if (!query.exec("SELECT id, name, platform, steam_app_id, save_paths FROM custom_games ORDER BY name")) {
        qWarning() << "Failed to query custom games:" << query.lastError().text();
        return games;
    }

    while (query.next()) {
        GameInfo game;
        game.id = query.value(0).toString();
        game.name = query.value(1).toString();
        game.platform = query.value(2).toString();
        game.steamAppId = query.value(3).toString();
        game.savePaths = deserializeSavePaths(query.value(4).toString());
        game.source = "database";
        games.append(game);
    }

    return games;
}

GameInfo Database::getCustomGame(const QString &id) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    query.prepare("SELECT id, name, platform, steam_app_id, save_paths FROM custom_games WHERE id = ?");
    query.addBindValue(id);

    if (!query.exec() || !query.next()) {
        return GameInfo();
    }

    GameInfo game;
    game.id = query.value(0).toString();
    game.name = query.value(1).toString();
    game.platform = query.value(2).toString();
    game.steamAppId = query.value(3).toString();
    game.savePaths = deserializeSavePaths(query.value(4).toString());
    game.source = "database";
    return game;
}

bool Database::addCustomGame(const GameInfo &game)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    query.prepare("INSERT INTO custom_games (id, name, platform, steam_app_id, save_paths) "
                  "VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(game.id);
    query.addBindValue(game.name);
    query.addBindValue(game.platform);
    query.addBindValue(game.steamAppId.isEmpty() ? QVariant() : game.steamAppId);
    query.addBindValue(serializeSavePaths(game.savePaths));

    if (!query.exec()) {
        qWarning() << "Failed to add custom game:" << query.lastError().text();
        return false;
    }

    return true;
}

bool Database::updateCustomGame(const GameInfo &game)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    query.prepare("UPDATE custom_games SET name=?, platform=?, steam_app_id=?, "
                  "save_paths=?, updated_at=datetime('now') WHERE id=?");
    query.addBindValue(game.name);
    query.addBindValue(game.platform);
    query.addBindValue(game.steamAppId.isEmpty() ? QVariant() : game.steamAppId);
    query.addBindValue(serializeSavePaths(game.savePaths));
    query.addBindValue(game.id);

    if (!query.exec()) {
        qWarning() << "Failed to update custom game:" << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool Database::removeCustomGame(const QString &id)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    query.prepare("DELETE FROM custom_games WHERE id = ?");
    query.addBindValue(id);

    if (!query.exec()) {
        qWarning() << "Failed to remove custom game:" << query.lastError().text();
        return false;
    }

    return query.numRowsAffected() > 0;
}

bool Database::customGameExists(const QString &id) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    query.prepare("SELECT 1 FROM custom_games WHERE id = ?");
    query.addBindValue(id);

    return query.exec() && query.next();
}

int Database::migrateFromJson(const QString &configDir)
{
    QDir dir(configDir);
    if (!dir.exists()) {
        return 0;
    }

    int migrated = 0;
    QStringList jsonFiles = dir.entryList(QStringList() << "*.json", QDir::Files);

    for (const QString &fileName : jsonFiles) {
        QString filePath = dir.absoluteFilePath(fileName);
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();

        if (!doc.isObject()) {
            continue;
        }

        QJsonArray gamesArray = doc.object()["games"].toArray();
        for (const QJsonValue &gameValue : gamesArray) {
            QJsonObject gameObj = gameValue.toObject();

            QString id = gameObj["id"].toString();
            QString platform = gameObj["platform"].toString("custom");

            // Skip Minecraft (separate integration planned)
            if (id == "minecraft") {
                continue;
            }

            // Skip Steam games (handled by manifest now)
            if (platform == "steam") {
                continue;
            }

            // Skip if already in database (idempotent)
            if (customGameExists(id)) {
                continue;
            }

            GameInfo game;
            game.id = id;
            game.name = gameObj["name"].toString();
            game.platform = platform;
            game.steamAppId = gameObj["steamAppId"].toString();
            game.source = "database";

            QJsonArray pathsArray = gameObj["savePaths"].toArray();
            for (const QJsonValue &pathValue : pathsArray) {
                game.savePaths << pathValue.toString();
            }

            if (addCustomGame(game)) {
                migrated++;
            }
        }
    }

    if (migrated > 0) {
        qDebug() << "Migrated" << migrated << "games from legacy JSON configs";
    }

    return migrated;
}

QString Database::databasePath() const
{
    return m_dbPath;
}

bool Database::hideGame(const QString &gameId, const QString &name)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    query.prepare("INSERT OR REPLACE INTO hidden_games (game_id, name) VALUES (?, ?)");
    query.addBindValue(gameId);
    query.addBindValue(name);

    if (!query.exec()) {
        qWarning() << "Failed to hide game:" << query.lastError().text();
        return false;
    }
    return true;
}

bool Database::unhideGame(const QString &gameId)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    query.prepare("DELETE FROM hidden_games WHERE game_id = ?");
    query.addBindValue(gameId);

    if (!query.exec()) {
        qWarning() << "Failed to unhide game:" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool Database::isGameHidden(const QString &gameId) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    query.prepare("SELECT 1 FROM hidden_games WHERE game_id = ?");
    query.addBindValue(gameId);

    return query.exec() && query.next();
}

QSet<QString> Database::getHiddenGameIds() const
{
    QSet<QString> ids;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    if (query.exec("SELECT game_id FROM hidden_games")) {
        while (query.next()) {
            ids.insert(query.value(0).toString());
        }
    }
    return ids;
}

QList<QPair<QString, QString>> Database::getHiddenGames() const
{
    QList<QPair<QString, QString>> games;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    if (query.exec("SELECT game_id, name FROM hidden_games ORDER BY name")) {
        while (query.next()) {
            games.append({query.value(0).toString(), query.value(1).toString()});
        }
    }
    return games;
}

QString Database::getSetting(const QString &key, const QString &defaultValue) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);
    query.prepare("SELECT value FROM app_settings WHERE key = ?");
    query.addBindValue(key);
    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return defaultValue;
}

bool Database::setSetting(const QString &key, const QString &value)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);
    query.prepare("INSERT OR REPLACE INTO app_settings (key, value) VALUES (?, ?)");
    query.addBindValue(key);
    query.addBindValue(value);
    if (!query.exec()) {
        qWarning() << "Failed to set setting" << key << ":" << query.lastError().text();
        return false;
    }
    return true;
}

QList<SaveProfile> Database::getProfilesForGame(const QString &gameId) const
{
    QList<SaveProfile> profiles;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    query.prepare("SELECT id, game_id, name, files FROM save_profiles WHERE game_id = ? ORDER BY name");
    query.addBindValue(gameId);

    if (query.exec()) {
        while (query.next()) {
            SaveProfile p;
            p.id = query.value(0).toInt();
            p.gameId = query.value(1).toString();
            p.name = query.value(2).toString();
            p.files = deserializeSavePaths(query.value(3).toString());
            profiles.append(p);
        }
    }
    return profiles;
}

SaveProfile Database::getProfile(int profileId) const
{
    SaveProfile p;
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    query.prepare("SELECT id, game_id, name, files FROM save_profiles WHERE id = ?");
    query.addBindValue(profileId);

    if (query.exec() && query.next()) {
        p.id = query.value(0).toInt();
        p.gameId = query.value(1).toString();
        p.name = query.value(2).toString();
        p.files = deserializeSavePaths(query.value(3).toString());
    }
    return p;
}

int Database::addProfile(const SaveProfile &profile)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    query.prepare("INSERT INTO save_profiles (game_id, name, files) VALUES (?, ?, ?)");
    query.addBindValue(profile.gameId);
    query.addBindValue(profile.name);
    query.addBindValue(serializeSavePaths(profile.files));

    if (!query.exec()) {
        qWarning() << "Failed to add profile:" << query.lastError().text();
        return -1;
    }
    return query.lastInsertId().toInt();
}

bool Database::updateProfile(const SaveProfile &profile)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    query.prepare("UPDATE save_profiles SET name = ?, files = ?, updated_at = datetime('now') WHERE id = ?");
    query.addBindValue(profile.name);
    query.addBindValue(serializeSavePaths(profile.files));
    query.addBindValue(profile.id);

    if (!query.exec()) {
        qWarning() << "Failed to update profile:" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool Database::removeProfile(int profileId)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    query.prepare("DELETE FROM save_profiles WHERE id = ?");
    query.addBindValue(profileId);

    if (!query.exec()) {
        qWarning() << "Failed to remove profile:" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool Database::profileExists(const QString &gameId, const QString &name) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);

    query.prepare("SELECT 1 FROM save_profiles WHERE game_id = ? AND name = ?");
    query.addBindValue(gameId);
    query.addBindValue(name);

    return query.exec() && query.next();
}

QString Database::serializeSavePaths(const QStringList &paths)
{
    QJsonArray array;
    for (const QString &path : paths) {
        array.append(path);
    }
    return QJsonDocument(array).toJson(QJsonDocument::Compact);
}

QStringList Database::deserializeSavePaths(const QString &json)
{
    QStringList paths;
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());

    if (doc.isArray()) {
        for (const QJsonValue &value : doc.array()) {
            paths << value.toString();
        }
    }

    return paths;
}
