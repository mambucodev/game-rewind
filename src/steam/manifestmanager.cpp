#include "manifestmanager.h"
#include "steamutils.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QDebug>
#include <QtConcurrent>
#include <yaml-cpp/yaml.h>

const QString ManifestManager::MANIFEST_URL =
    QStringLiteral("https://raw.githubusercontent.com/mtkennerly/ludusavi-manifest/master/data/manifest.yaml");

ManifestManager::ManifestManager(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &ManifestManager::onDownloadFinished);
    connect(&m_parseWatcher, &QFutureWatcher<QMap<int, ManifestGameEntry>>::finished,
            this, &ManifestManager::onAsyncParseFinished);
}

ManifestManager::~ManifestManager()
{
    if (m_parsing) {
        m_parseWatcher.waitForFinished();
    }
}

bool ManifestManager::loadCachedManifest()
{
    QString cachePath = getCachePath();

    if (!QFile::exists(cachePath)) {
        qDebug() << "No cached manifest found at" << cachePath;
        return false;
    }

    qDebug() << "Loading cached manifest from" << cachePath;
    return parseManifestFile(cachePath);
}

void ManifestManager::loadCachedManifestAsync()
{
    QString cachePath = getCachePath();

    if (!QFile::exists(cachePath)) {
        qDebug() << "No cached manifest found at" << cachePath;
        return;
    }

    if (m_parsing) {
        return;
    }

    m_parsing = true;
    qDebug() << "Loading cached manifest async from" << cachePath;
    m_parseWatcher.setFuture(QtConcurrent::run(&ManifestManager::parseManifestInThread, cachePath));
}

void ManifestManager::onAsyncParseFinished()
{
    m_parsing = false;
    QMap<int, ManifestGameEntry> result = m_parseWatcher.result();
    if (!result.isEmpty()) {
        m_steamIdIndex = result;
        m_loaded = true;
        qDebug() << "Async manifest parse complete:" << m_steamIdIndex.size() << "Steam games indexed";
        emit manifestReady();
    }
}

bool ManifestManager::isParsing() const
{
    return m_parsing;
}

void ManifestManager::checkForUpdates()
{
    if (m_downloading) {
        return;
    }

    m_downloading = true;

    QUrl url(MANIFEST_URL);
    QNetworkRequest request{url};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    // Use ETag for cache validation
    QString etagPath = getETagPath();
    if (QFile::exists(etagPath)) {
        QFile etagFile(etagPath);
        if (etagFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString etag = etagFile.readAll().trimmed();
            etagFile.close();
            if (!etag.isEmpty()) {
                request.setRawHeader("If-None-Match", etag.toUtf8());
            }
        }
    }

    qDebug() << "Checking for manifest updates...";
    m_networkManager->get(request);
}

void ManifestManager::onDownloadFinished(QNetworkReply *reply)
{
    m_downloading = false;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        // Network error - not critical, continue with cached data
        qWarning() << "Manifest download failed:" << reply->errorString();
        emit manifestUpdateFailed(reply->errorString());
        return;
    }

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (statusCode == 304) {
        // Not modified - cache is up to date
        qDebug() << "Manifest is up to date (304 Not Modified)";
        return;
    }

    if (statusCode != 200) {
        qWarning() << "Unexpected HTTP status:" << statusCode;
        emit manifestUpdateFailed(QString("HTTP %1").arg(statusCode));
        return;
    }

    QByteArray data = reply->readAll();
    if (data.isEmpty()) {
        qWarning() << "Received empty manifest data";
        emit manifestUpdateFailed("Empty response");
        return;
    }

    // Save manifest to cache
    QString cachePath = getCachePath();
    QDir().mkpath(QFileInfo(cachePath).absolutePath());

    QFile cacheFile(cachePath);
    if (!cacheFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Could not write manifest cache:" << cachePath;
        emit manifestUpdateFailed("Could not write cache file");
        return;
    }
    cacheFile.write(data);
    cacheFile.close();

    // Save ETag
    QString etag = reply->rawHeader("ETag");
    if (!etag.isEmpty()) {
        QFile etagFile(getETagPath());
        if (etagFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            etagFile.write(etag.toUtf8());
            etagFile.close();
        }
    }

    qDebug() << "Manifest downloaded and cached (" << data.size() << "bytes)";

    // Re-parse the manifest
    bool wasLoaded = m_loaded;
    if (parseManifestFile(cachePath)) {
        // Only signal if this is a new load or an update to existing data
        if (!wasLoaded || m_steamIdIndex.size() > 0) {
            emit manifestReady();
        }
    }
}

QMap<int, ManifestGameEntry> ManifestManager::parseManifestInThread(const QString &filePath)
{
    QMap<int, ManifestGameEntry> index;
    try {
        YAML::Node root = YAML::LoadFile(filePath.toStdString());
        if (!root.IsMap()) {
            return index;
        }

        for (auto it = root.begin(); it != root.end(); ++it) {
            QString gameName = QString::fromStdString(it->first.as<std::string>());
            YAML::Node gameNode = it->second;
            if (!gameNode.IsMap()) continue;

            ManifestGameEntry entry;
            entry.name = gameName;

            if (gameNode["steam"] && gameNode["steam"]["id"]) {
                entry.steamId = gameNode["steam"]["id"].as<int>(0);
            }

            if (gameNode["installDir"] && gameNode["installDir"].IsMap()) {
                for (auto dirIt = gameNode["installDir"].begin();
                     dirIt != gameNode["installDir"].end(); ++dirIt) {
                    entry.installDirs << QString::fromStdString(dirIt->first.as<std::string>());
                }
            }

            if (gameNode["files"] && gameNode["files"].IsMap()) {
                for (auto fileIt = gameNode["files"].begin();
                     fileIt != gameNode["files"].end(); ++fileIt) {
                    ManifestFileEntry fileEntry;
                    fileEntry.path = QString::fromStdString(fileIt->first.as<std::string>());

                    YAML::Node fileNode = fileIt->second;
                    if (fileNode.IsMap()) {
                        if (fileNode["tags"] && fileNode["tags"].IsSequence()) {
                            for (const auto &tag : fileNode["tags"]) {
                                fileEntry.tags << QString::fromStdString(tag.as<std::string>());
                            }
                        }
                        if (fileNode["when"] && fileNode["when"].IsSequence()) {
                            for (const auto &constraint : fileNode["when"]) {
                                FileConstraint fc;
                                if (constraint["os"]) {
                                    fc.os = QString::fromStdString(constraint["os"].as<std::string>());
                                }
                                if (constraint["store"]) {
                                    fc.store = QString::fromStdString(constraint["store"].as<std::string>());
                                }
                                fileEntry.when.append(fc);
                            }
                        }
                    }
                    entry.files.append(fileEntry);
                }
            }

            if (entry.steamId > 0) {
                index[entry.steamId] = entry;
            }
        }
    } catch (const YAML::Exception &e) {
        qWarning() << "YAML parse error:" << e.what();
    }
    return index;
}

bool ManifestManager::parseManifestFile(const QString &filePath)
{
    QMap<int, ManifestGameEntry> result = parseManifestInThread(filePath);
    if (result.isEmpty()) {
        return false;
    }
    m_steamIdIndex = result;
    m_loaded = true;
    qDebug() << "Parsed manifest:" << m_steamIdIndex.size() << "Steam games indexed";
    return true;
}

ManifestGameEntry ManifestManager::findBySteamId(int steamAppId) const
{
    return m_steamIdIndex.value(steamAppId, ManifestGameEntry());
}

QStringList ManifestManager::getLinuxSavePaths(const ManifestGameEntry &entry,
                                                const QString &steamLibraryPath)
{
    QStringList paths;

    // Windows-only placeholders to exclude
    static const QStringList windowsPlaceholders = {
        "<winAppData>", "<winLocalAppData>", "<winLocalAppDataLow>",
        "<winDocuments>", "<winPublic>", "<winProgramData>", "<winDir>"
    };

    for (const ManifestFileEntry &file : entry.files) {
        // Only include save-tagged entries
        if (!file.tags.contains("save")) {
            continue;
        }

        // Skip paths with Windows-only placeholders
        bool hasWindowsPlaceholder = false;
        for (const QString &wp : windowsPlaceholders) {
            if (file.path.contains(wp)) {
                hasWindowsPlaceholder = true;
                break;
            }
        }
        if (hasWindowsPlaceholder) {
            continue;
        }

        // Check OS constraints
        bool applicableToLinux = true;
        if (!file.when.isEmpty()) {
            // Has constraints - check if any allow Linux
            bool hasOsConstraint = false;
            bool linuxAllowed = false;
            for (const FileConstraint &fc : file.when) {
                if (!fc.os.isEmpty()) {
                    hasOsConstraint = true;
                    if (fc.os == "linux") {
                        linuxAllowed = true;
                    }
                }
            }
            // If there are OS constraints and none say linux, skip
            if (hasOsConstraint && !linuxAllowed) {
                applicableToLinux = false;
            }
        }

        if (!applicableToLinux) {
            continue;
        }

        QString expanded = expandManifestPath(file.path, entry, steamLibraryPath);
        if (!expanded.isEmpty()) {
            // Manifest paths may contain glob patterns (e.g. "*.dat", "*/Level.bin").
            // Strip glob components and use the deepest non-glob parent directory,
            // since that's what we actually want to detect and back up.
            while (expanded.contains('*') || expanded.contains('?')) {
                expanded = QFileInfo(expanded).absolutePath();
            }
            if (!paths.contains(expanded)) {
                paths << expanded;
            }
        }
    }

    return paths;
}

QString ManifestManager::expandManifestPath(const QString &path,
                                             const ManifestGameEntry &entry,
                                             const QString &steamLibraryPath)
{
    QString expanded = path;
    QString home = QDir::homePath();

    // Basic placeholders
    expanded.replace("<home>", home);
    expanded.replace("<osUserName>", qEnvironmentVariable("USER"));

    // XDG directories
    QString xdgData = qEnvironmentVariable("XDG_DATA_HOME");
    if (xdgData.isEmpty()) {
        xdgData = home + "/.local/share";
    }
    expanded.replace("<xdgData>", xdgData);

    QString xdgConfig = qEnvironmentVariable("XDG_CONFIG_HOME");
    if (xdgConfig.isEmpty()) {
        xdgConfig = home + "/.config";
    }
    expanded.replace("<xdgConfig>", xdgConfig);

    // Steam-specific placeholders
    QString steamPath = SteamUtils::findSteamPath();

    if (expanded.contains("<storeUserId>")) {
        QString userId = SteamUtils::getSteamUserId(steamPath);
        if (userId.isEmpty()) {
            return QString(); // Can't resolve without user ID
        }
        expanded.replace("<storeUserId>", userId);
    }

    // <root> and <game> / <base>
    QString root = steamLibraryPath.isEmpty()
                       ? QString()
                       : steamLibraryPath + "/steamapps/common";

    QString game = entry.installDirs.isEmpty() ? entry.name : entry.installDirs.first();

    if (expanded.contains("<base>")) {
        if (root.isEmpty()) {
            return QString(); // Can't resolve <base> without a library path
        }
        expanded.replace("<base>", root + "/" + game);
    }
    if (expanded.contains("<root>")) {
        if (root.isEmpty()) {
            return QString();
        }
        expanded.replace("<root>", root);
    }
    expanded.replace("<game>", game);

    return expanded;
}

QStringList ManifestManager::getProtonSavePaths(const ManifestGameEntry &entry,
                                                 const QString &protonPrefixPath,
                                                 const QString &steamLibraryPath)
{
    QStringList paths;

    for (const ManifestFileEntry &file : entry.files) {
        // Accept save or config tagged entries. In Proton prefixes,
        // saves and config are often co-located and the manifest's
        // distinction is not always accurate for Wine/Proton.
        if (!file.tags.contains("save") && !file.tags.contains("config")) {
            continue;
        }

        // Accept entries with no OS constraint or with os:windows.
        // Skip linux-only entries (already handled by getLinuxSavePaths).
        bool hasOsConstraint = false;
        bool windowsAllowed = false;
        for (const FileConstraint &fc : file.when) {
            if (!fc.os.isEmpty()) {
                hasOsConstraint = true;
                if (fc.os == "windows") {
                    windowsAllowed = true;
                }
            }
        }
        if (hasOsConstraint && !windowsAllowed) {
            continue;
        }

        QString expanded = expandProtonPath(file.path, entry, protonPrefixPath, steamLibraryPath);
        if (!expanded.isEmpty()) {
            while (expanded.contains('*') || expanded.contains('?')) {
                expanded = QFileInfo(expanded).absolutePath();
            }
            if (!paths.contains(expanded)) {
                paths << expanded;
            }
        }
    }

    return paths;
}

QString ManifestManager::expandProtonPath(const QString &path,
                                           const ManifestGameEntry &entry,
                                           const QString &protonPrefixPath,
                                           const QString &steamLibraryPath)
{
    QString expanded = path;
    QString protonHome = protonPrefixPath + "/drive_c/users/steamuser";

    // Windows placeholder mappings
    expanded.replace("<winAppData>",         protonHome + "/AppData/Roaming");
    expanded.replace("<winLocalAppData>",    protonHome + "/AppData/Local");
    expanded.replace("<winLocalAppDataLow>", protonHome + "/AppData/LocalLow");
    expanded.replace("<winDocuments>",       protonHome + "/Documents");
    expanded.replace("<winPublic>",          protonPrefixPath + "/drive_c/users/Public");
    expanded.replace("<winProgramData>",     protonPrefixPath + "/drive_c/ProgramData");
    expanded.replace("<winDir>",             protonPrefixPath + "/drive_c/windows");

    // <home> in Windows/Proton context maps to the Wine user home
    expanded.replace("<home>", protonHome);
    expanded.replace("<osUserName>", "steamuser");

    // Game install directory placeholders point to the real filesystem
    QString root = steamLibraryPath.isEmpty()
                       ? QString()
                       : steamLibraryPath + "/steamapps/common";
    QString game = entry.installDirs.isEmpty() ? entry.name : entry.installDirs.first();

    if (expanded.contains("<base>")) {
        if (root.isEmpty()) return QString();
        expanded.replace("<base>", root + "/" + game);
    }
    if (expanded.contains("<root>")) {
        if (root.isEmpty()) return QString();
        expanded.replace("<root>", root);
    }
    expanded.replace("<game>", game);

    if (expanded.contains("<storeUserId>")) {
        QString steamPath = SteamUtils::findSteamPath();
        QString userId = SteamUtils::getSteamUserId(steamPath);
        if (userId.isEmpty()) return QString();
        expanded.replace("<storeUserId>", userId);
    }

    // Discard paths with unresolved placeholders (e.g., <xdgData>)
    if (expanded.contains('<') && expanded.contains('>')) {
        return QString();
    }

    return expanded;
}

bool ManifestManager::isLoaded() const
{
    return m_loaded;
}

QMap<int, ManifestGameEntry> ManifestManager::getSteamIdIndex() const
{
    return m_steamIdIndex;
}

QString ManifestManager::getCachePath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + "/game-rewind/manifest.yaml";
}

QString ManifestManager::getETagPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + "/game-rewind/manifest.etag";
}
