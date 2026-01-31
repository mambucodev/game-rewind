#include "gamedetector.h"
#include "core/database.h"
#include "steamutils.h"
#include "manifestmanager.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QtConcurrent>

GameDetector::GameDetector(QObject *parent)
    : QObject(parent)
{
    m_steamPath = SteamUtils::findSteamPath();
    m_steamLibraryFolders = SteamUtils::getLibraryFolders(m_steamPath);

    connect(&m_detectWatcher, &QFutureWatcher<QList<GameInfo>>::finished,
            this, &GameDetector::onAsyncDetectionFinished);
}

GameDetector::~GameDetector()
{
    if (m_detecting) {
        m_detectWatcher.waitForFinished();
    }
}

void GameDetector::setManifestManager(ManifestManager *manager)
{
    m_manifestManager = manager;
}

void GameDetector::setHiddenGameIds(const QSet<QString> &ids)
{
    m_hiddenGames = ids;
}

void GameDetector::setSavePathOverrides(const QMap<QString, QString> &overrides)
{
    m_savePathOverrides = overrides;
}

void GameDetector::loadCustomGames(Database *db)
{
    m_games.clear();
    m_detectedGames.clear();
    m_customSteamIds.clear();

    QList<GameInfo> customGames = db->getAllCustomGames();

    for (const GameInfo &game : customGames) {
        if (!game.steamAppId.isEmpty()) {
            m_customSteamIds.insert(game.steamAppId);
        }
        m_games.append(game);
    }

    qDebug() << "Loaded" << m_games.size() << "custom games from database";

    detectGames();
}

void GameDetector::loadGamesAsync(Database *db)
{
    if (m_detecting) {
        return;
    }

    m_games.clear();
    m_detectedGames.clear();
    m_customSteamIds.clear();

    QList<GameInfo> customGames = db->getAllCustomGames();
    for (const GameInfo &game : customGames) {
        if (!game.steamAppId.isEmpty()) {
            m_customSteamIds.insert(game.steamAppId);
        }
        m_games.append(game);
    }

    m_detecting = true;

    DetectionContext ctx;
    ctx.games = m_games;
    ctx.customSteamIds = m_customSteamIds;
    ctx.hiddenGames = m_hiddenGames;
    ctx.savePathOverrides = m_savePathOverrides;
    ctx.steamPath = m_steamPath;
    ctx.steamLibraryFolders = m_steamLibraryFolders;
    if (m_manifestManager && m_manifestManager->isLoaded()) {
        ctx.manifestLoaded = true;
        ctx.steamIdIndex = m_manifestManager->getSteamIdIndex();
    }

    m_detectWatcher.setFuture(QtConcurrent::run(&GameDetector::detectGamesInThread, ctx));
}

bool GameDetector::isDetecting() const
{
    return m_detecting;
}

void GameDetector::waitForDetection()
{
    if (m_detecting) {
        m_detectWatcher.waitForFinished();
    }
}

QList<GameInfo> GameDetector::detectGamesInThread(DetectionContext ctx)
{
    QList<GameInfo> detected;

    // Phase 1: Custom games
    for (const GameInfo &game : ctx.games) {
        if (ctx.hiddenGames.contains(game.id)) {
            continue;
        }

        GameInfo det = game;
        det.isDetected = false;

        for (const QString &savePath : game.savePaths) {
            QString expanded = savePath;
            if (expanded.startsWith("~")) {
                expanded.replace(0, 1, QDir::homePath());
            }
            expanded.replace("$HOME", QDir::homePath());
            if (!ctx.steamPath.isEmpty()) {
                expanded.replace("$STEAM", ctx.steamPath);
            }

            if (QFileInfo(expanded).exists()) {
                det.detectedSavePath = expanded;
                det.isDetected = true;
                break;
            }
        }

        if (det.isDetected) {
            // Check installed (simplified inline)
            bool installed = true;
            if (det.platform == "steam" && !det.steamAppId.isEmpty()) {
                if (ctx.steamPath.isEmpty()) {
                    installed = false;
                } else {
                    installed = false;
                    for (const QString &lib : ctx.steamLibraryFolders) {
                        if (QFile::exists(lib + "/steamapps/appmanifest_" + det.steamAppId + ".acf")) {
                            installed = true;
                            break;
                        }
                    }
                }
            }
            if (installed) {
                detected.append(det);
            }
        }
    }

    qDebug() << "Async Phase 1: Detected" << detected.size() << "custom games";

    // Phase 2: Manifest games
    if (ctx.manifestLoaded) {
        QList<SteamAppInfo> installedGames = SteamUtils::scanInstalledGames(ctx.steamLibraryFolders);

        for (const SteamAppInfo &steamGame : installedGames) {
            if (ctx.customSteamIds.contains(steamGame.appId)) continue;
            if (ctx.hiddenGames.contains("steam_" + steamGame.appId)) continue;

            int appId = steamGame.appId.toInt();
            if (appId <= 0) continue;

            ManifestGameEntry entry = ctx.steamIdIndex.value(appId);
            if (entry.name.isEmpty()) continue;

            QStringList allValidPaths;

#ifdef Q_OS_WIN
            QStringList winPaths = ManifestManager::getWindowsSavePaths(entry, steamGame.libraryPath);
            for (const QString &path : winPaths) {
                if (QFileInfo::exists(path) && !allValidPaths.contains(path)) {
                    allValidPaths.append(path);
                }
            }
#else
            QStringList linuxPaths = ManifestManager::getLinuxSavePaths(entry, steamGame.libraryPath);
            for (const QString &path : linuxPaths) {
                if (QFileInfo::exists(path) && !allValidPaths.contains(path)) {
                    allValidPaths.append(path);
                }
            }

            QString protonPrefix = SteamUtils::findProtonPrefix(steamGame.appId, ctx.steamLibraryFolders);
            if (!protonPrefix.isEmpty()) {
                QStringList protonPaths = ManifestManager::getProtonSavePaths(
                    entry, protonPrefix, steamGame.libraryPath);
                for (const QString &path : protonPaths) {
                    if (QFileInfo::exists(path) && !allValidPaths.contains(path)) {
                        allValidPaths.append(path);
                    }
                }
            }
#endif

            if (allValidPaths.isEmpty()) continue;

            GameInfo game;
            game.id = "steam_" + steamGame.appId;
            game.name = steamGame.name;
            game.platform = "steam";
            game.steamAppId = steamGame.appId;
            game.source = "manifest";
            game.isDetected = true;

            QString overridePath = ctx.savePathOverrides.value(game.id);
            if (!overridePath.isEmpty() && allValidPaths.contains(overridePath)) {
                game.detectedSavePath = overridePath;
            } else {
                game.detectedSavePath = allValidPaths.first();
            }

            for (const QString &p : allValidPaths) {
                if (p != game.detectedSavePath) {
                    game.alternativeSavePaths.append(p);
                }
            }

            detected.append(game);
        }
    }

    qDebug() << "Async detection total:" << detected.size() << "games";
    return detected;
}

void GameDetector::onAsyncDetectionFinished()
{
    m_detecting = false;
    m_detectedGames = m_detectWatcher.result();
    saveCachedGames();
    qDebug() << "Async detection finished:" << m_detectedGames.size() << "games";
    emit detectionFinished();
}

QList<GameInfo> GameDetector::getDetectedGames() const
{
    return m_detectedGames;
}

GameInfo GameDetector::getGameById(const QString &id) const
{
    for (const GameInfo &game : m_detectedGames) {
        if (game.id == id) {
            return game;
        }
    }
    return GameInfo();
}

QString GameDetector::scanForSavePath(const QString &gameName, const QString &hint)
{
    QStringList commonPaths;
    QString home = QDir::homePath();

#ifdef Q_OS_WIN
    QString appData = qEnvironmentVariable("APPDATA");
    if (appData.isEmpty()) appData = home + "/AppData/Roaming";
    QString localAppData = qEnvironmentVariable("LOCALAPPDATA");
    if (localAppData.isEmpty()) localAppData = home + "/AppData/Local";

    commonPaths << appData + "/" + gameName
                << localAppData + "/" + gameName
                << home + "/Documents/My Games/" + gameName
                << home + "/Saved Games/" + gameName;
#else
    commonPaths << home + "/.local/share/" + gameName.toLower()
                << home + "/.config/" + gameName.toLower()
                << home + "/Documents/My Games/" + gameName
                << home + "/.steam/steam/steamapps/compatdata";
#endif

    if (!hint.isEmpty()) {
        QString expandedHint = expandPath(hint);
        if (pathExists(expandedHint)) {
            return expandedHint;
        }
    }

    for (const QString &path : commonPaths) {
        if (pathExists(path)) {
            return path;
        }
    }

    return QString();
}

QString GameDetector::expandPath(const QString &path) const
{
    QString expanded = path;

    if (expanded.startsWith("~")) {
        expanded.replace(0, 1, QDir::homePath());
    }

    expanded.replace("$HOME", QDir::homePath());

    if (!m_steamPath.isEmpty()) {
        expanded.replace("$STEAM", m_steamPath);
    }

    return expanded;
}

bool GameDetector::pathExists(const QString &path) const
{
    QFileInfo info(expandPath(path));
    return info.exists();
}

void GameDetector::detectGames()
{
    // Phase 1: Detect games from custom database entries
    for (const GameInfo &game : m_games) {
        if (m_hiddenGames.contains(game.id)) {
            continue;
        }

        GameInfo detected = game;
        detected.isDetected = false;

        for (const QString &savePath : game.savePaths) {
            QString expanded = expandPath(savePath);

            if (pathExists(expanded)) {
                detected.detectedSavePath = expanded;
                detected.isDetected = true;
                break;
            }
        }

        if (detected.isDetected && isGameInstalled(detected)) {
            m_detectedGames.append(detected);
        }
    }

    qDebug() << "Phase 1: Detected" << m_detectedGames.size() << "custom games";

    // Phase 2: Detect games from Ludusavi manifest
    detectManifestGames();

    qDebug() << "Total detected:" << m_detectedGames.size() << "games";
}

void GameDetector::detectManifestGames()
{
    if (!m_manifestManager || !m_manifestManager->isLoaded()) {
        return;
    }

    QList<SteamAppInfo> installedGames = SteamUtils::scanInstalledGames(m_steamLibraryFolders);

    int manifestDetected = 0;

    for (const SteamAppInfo &steamGame : installedGames) {
        // Skip games already defined as custom games
        if (m_customSteamIds.contains(steamGame.appId)) {
            continue;
        }

        // Skip hidden games
        if (m_hiddenGames.contains("steam_" + steamGame.appId)) {
            continue;
        }

        int appId = steamGame.appId.toInt();
        if (appId <= 0) {
            continue;
        }

        ManifestGameEntry entry = m_manifestManager->findBySteamId(appId);
        if (entry.name.isEmpty()) {
            continue;
        }

        // Collect all existing save paths
        QStringList allValidPaths;

#ifdef Q_OS_WIN
        QStringList winPaths = ManifestManager::getWindowsSavePaths(entry, steamGame.libraryPath);
        for (const QString &path : winPaths) {
            if (QFileInfo::exists(path) && !allValidPaths.contains(path)) {
                allValidPaths.append(path);
            }
        }
#else
        QStringList linuxPaths = ManifestManager::getLinuxSavePaths(entry, steamGame.libraryPath);
        for (const QString &path : linuxPaths) {
            if (QFileInfo::exists(path) && !allValidPaths.contains(path)) {
                allValidPaths.append(path);
            }
        }

        QString protonPrefix = SteamUtils::findProtonPrefix(steamGame.appId, m_steamLibraryFolders);
        if (!protonPrefix.isEmpty()) {
            QStringList protonPaths = ManifestManager::getProtonSavePaths(
                entry, protonPrefix, steamGame.libraryPath);
            for (const QString &path : protonPaths) {
                if (QFileInfo::exists(path) && !allValidPaths.contains(path)) {
                    allValidPaths.append(path);
                }
            }
        }
#endif

        if (allValidPaths.isEmpty()) {
            continue;
        }

        GameInfo game;
        game.id = "steam_" + steamGame.appId;
        game.name = steamGame.name;
        game.platform = "steam";
        game.steamAppId = steamGame.appId;
        game.source = "manifest";
        game.isDetected = true;

        // Apply user override if one exists
        QString overridePath = m_savePathOverrides.value(game.id);
        if (!overridePath.isEmpty() && allValidPaths.contains(overridePath)) {
            game.detectedSavePath = overridePath;
        } else {
            game.detectedSavePath = allValidPaths.first();
        }

        // Store alternatives (all paths except the active one)
        for (const QString &p : allValidPaths) {
            if (p != game.detectedSavePath) {
                game.alternativeSavePaths.append(p);
            }
        }

        m_detectedGames.append(game);
        manifestDetected++;
    }

    qDebug() << "Phase 2: Detected" << manifestDetected << "games from Ludusavi manifest";
}

static QString cachePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + "/game-rewind/detected_games.json";
}

bool GameDetector::loadCachedGames()
{
    QFile file(cachePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isArray()) {
        return false;
    }

    m_detectedGames.clear();
    QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr) {
        QJsonObject obj = val.toObject();
        GameInfo game;
        game.id = obj["id"].toString();
        game.name = obj["name"].toString();
        game.platform = obj["platform"].toString();
        game.steamAppId = obj["steamAppId"].toString();
        game.detectedSavePath = obj["detectedSavePath"].toString();
        game.source = obj["source"].toString();
        game.isDetected = true;

        QJsonArray paths = obj["savePaths"].toArray();
        for (const QJsonValue &p : paths) {
            game.savePaths << p.toString();
        }

        QJsonArray altPaths = obj["alternativeSavePaths"].toArray();
        for (const QJsonValue &p : altPaths) {
            game.alternativeSavePaths << p.toString();
        }

        m_detectedGames.append(game);
    }

    qDebug() << "Loaded" << m_detectedGames.size() << "games from cache";
    return !m_detectedGames.isEmpty();
}

void GameDetector::saveCachedGames() const
{
    QJsonArray arr;
    for (const GameInfo &game : m_detectedGames) {
        QJsonObject obj;
        obj["id"] = game.id;
        obj["name"] = game.name;
        obj["platform"] = game.platform;
        obj["steamAppId"] = game.steamAppId;
        obj["detectedSavePath"] = game.detectedSavePath;
        obj["source"] = game.source;

        QJsonArray paths;
        for (const QString &p : game.savePaths) {
            paths.append(p);
        }
        obj["savePaths"] = paths;

        QJsonArray altPaths;
        for (const QString &p : game.alternativeSavePaths) {
            altPaths.append(p);
        }
        obj["alternativeSavePaths"] = altPaths;

        arr.append(obj);
    }

    QString path = cachePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
        file.close();
        qDebug() << "Saved" << m_detectedGames.size() << "games to cache";
    }
}

bool GameDetector::isGameInstalled(const GameInfo &game) const
{
    if (game.platform == "custom") {
        return true;
    }

    if (game.platform == "native") {
        return true;
    }

    if (game.platform == "steam" && !game.steamAppId.isEmpty()) {
        if (m_steamPath.isEmpty()) {
            return false;
        }

        for (const QString &library : m_steamLibraryFolders) {
            QString manifestPath = library + "/steamapps/appmanifest_" + game.steamAppId + ".acf";
            if (QFile::exists(manifestPath)) {
                return true;
            }
        }
        return false;
    }

    return true;
}
