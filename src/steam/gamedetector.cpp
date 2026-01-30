#include "gamedetector.h"
#include "core/database.h"
#include "steamutils.h"
#include "manifestmanager.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

GameDetector::GameDetector(QObject *parent)
    : QObject(parent)
{
    m_steamPath = SteamUtils::findSteamPath();
    m_steamLibraryFolders = SteamUtils::getLibraryFolders(m_steamPath);
}

void GameDetector::setManifestManager(ManifestManager *manager)
{
    m_manifestManager = manager;
}

void GameDetector::setHiddenGameIds(const QSet<QString> &ids)
{
    m_hiddenGames = ids;
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

    commonPaths << home + "/.local/share/" + gameName.toLower()
                << home + "/.config/" + gameName.toLower()
                << home + "/Documents/My Games/" + gameName
                << home + "/.steam/steam/steamapps/compatdata";

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

        QStringList savePaths = m_manifestManager->getLinuxSavePaths(entry, steamGame.libraryPath);

        QString detectedPath;
        for (const QString &path : savePaths) {
            if (QFileInfo::exists(path)) {
                detectedPath = path;
                break;
            }
        }

        // Proton fallback: if no Linux-native save path found,
        // check if this game runs via Proton and try Windows paths
        // expanded into the Proton prefix.
        if (detectedPath.isEmpty()) {
            QString protonPrefix = SteamUtils::findProtonPrefix(steamGame.appId, m_steamLibraryFolders);
            if (!protonPrefix.isEmpty()) {
                QStringList protonPaths = m_manifestManager->getProtonSavePaths(
                    entry, protonPrefix, steamGame.libraryPath);
                for (const QString &path : protonPaths) {
                    if (QFileInfo::exists(path)) {
                        detectedPath = path;
                        break;
                    }
                }
            }
        }

        if (!detectedPath.isEmpty()) {
            GameInfo game;
            game.id = "steam_" + steamGame.appId;
            game.name = steamGame.name;
            game.platform = "steam";
            game.steamAppId = steamGame.appId;
            game.source = "manifest";
            game.detectedSavePath = detectedPath;
            game.isDetected = true;

            m_detectedGames.append(game);
            manifestDetected++;
        }
    }

    qDebug() << "Phase 2: Detected" << manifestDetected << "games from Ludusavi manifest";
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
