#include "steamutils.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>
#ifdef Q_OS_WIN
#include <QSettings>
#endif

QString SteamUtils::findSteamPath()
{
#ifdef Q_OS_WIN
    // Check Windows registry
    for (const QString &key : {"HKEY_LOCAL_MACHINE\\SOFTWARE\\Valve\\Steam",
                                "HKEY_CURRENT_USER\\SOFTWARE\\Valve\\Steam"}) {
        QSettings reg(key, QSettings::NativeFormat);
        QString path = reg.value("InstallPath").toString();
        if (!path.isEmpty() && QDir(path).exists()) {
            return path;
        }
    }
    // Fallback paths
    QStringList possiblePaths = {
        "C:/Program Files (x86)/Steam",
        "C:/Program Files/Steam"
    };
#else
    QString home = QDir::homePath();
    QStringList possiblePaths = {
        home + "/.steam/steam",
        home + "/.local/share/Steam",
        "/usr/share/steam"
    };
#endif

    for (const QString &path : possiblePaths) {
        if (QDir(path).exists()) {
            return path;
        }
    }

    return QString();
}

QStringList SteamUtils::getLibraryFolders(const QString &steamPath)
{
    QStringList folders;
    QSet<QString> canonicalPaths;

    if (steamPath.isEmpty()) {
        return folders;
    }

    // The primary Steam library is always steamPath itself
    QString canonicalSteam = QFileInfo(steamPath).canonicalFilePath();
    folders << steamPath;
    canonicalPaths.insert(canonicalSteam);

    // Parse libraryfolders.vdf for additional libraries
    QString vdfPath = steamPath + "/steamapps/libraryfolders.vdf";
    if (!QFile::exists(vdfPath)) {
        return folders;
    }

    QVariantMap vdf = parseVdf(vdfPath);
    QVariantMap libraryFolders = vdf.value("libraryfolders").toMap();

    for (auto it = libraryFolders.constBegin(); it != libraryFolders.constEnd(); ++it) {
        QVariantMap entry = it.value().toMap();
        QString path = entry.value("path").toString();
        if (path.isEmpty()) {
            continue;
        }
        // Deduplicate by canonical path (resolves symlinks)
        QString canonical = QFileInfo(path).canonicalFilePath();
        if (!canonical.isEmpty() && !canonicalPaths.contains(canonical)) {
            canonicalPaths.insert(canonical);
            folders << path;
        }
    }

    return folders;
}

QList<SteamAppInfo> SteamUtils::scanInstalledGames(const QStringList &libraryFolders)
{
    QList<SteamAppInfo> games;

    for (const QString &library : libraryFolders) {
        QString steamappsDir = library + "/steamapps";
        QDir dir(steamappsDir);

        if (!dir.exists()) {
            continue;
        }

        QStringList manifests = dir.entryList(QStringList() << "appmanifest_*.acf", QDir::Files);

        for (const QString &manifest : manifests) {
            QString manifestPath = dir.absoluteFilePath(manifest);
            SteamAppInfo game = parseAppManifest(manifestPath, library);

            if (!game.name.isEmpty() && !game.appId.isEmpty()) {
                games.append(game);
            }
        }
    }

    std::sort(games.begin(), games.end(), [](const SteamAppInfo &a, const SteamAppInfo &b) {
        return a.name.toLower() < b.name.toLower();
    });

    return games;
}

SteamAppInfo SteamUtils::parseAppManifest(const QString &manifestPath, const QString &libraryPath)
{
    SteamAppInfo game;
    game.libraryPath = libraryPath;

    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return game;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();

        if (line.startsWith("\"appid\"")) {
            int firstQuote = line.indexOf('"', 7);
            int secondQuote = line.indexOf('"', firstQuote + 1);
            if (firstQuote >= 0 && secondQuote > firstQuote) {
                game.appId = line.mid(firstQuote + 1, secondQuote - firstQuote - 1);
            }
        } else if (line.startsWith("\"name\"")) {
            int firstQuote = line.indexOf('"', 6);
            int secondQuote = line.indexOf('"', firstQuote + 1);
            if (firstQuote >= 0 && secondQuote > firstQuote) {
                game.name = line.mid(firstQuote + 1, secondQuote - firstQuote - 1);
            }
        } else if (line.startsWith("\"installdir\"")) {
            int firstQuote = line.indexOf('"', 12);
            int secondQuote = line.indexOf('"', firstQuote + 1);
            if (firstQuote >= 0 && secondQuote > firstQuote) {
                game.installDir = line.mid(firstQuote + 1, secondQuote - firstQuote - 1);
            }
        }

        if (!game.appId.isEmpty() && !game.name.isEmpty() && !game.installDir.isEmpty()) {
            break;
        }
    }

    file.close();
    return game;
}

QVariantMap SteamUtils::parseVdf(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open VDF file:" << filePath;
        return QVariantMap();
    }

    QString content = file.readAll();
    file.close();

    int pos = 0;
    QVariantMap result;

    skipWhitespace(content, pos);

    // Parse top-level: should be a key followed by a block
    while (pos < content.size()) {
        skipWhitespace(content, pos);
        if (pos >= content.size()) break;

        if (content[pos] == '"') {
            QString key = parseVdfString(content, pos);
            skipWhitespace(content, pos);

            if (pos < content.size() && content[pos] == '"') {
                // Key-value pair
                QString value = parseVdfString(content, pos);
                result[key] = value;
            } else if (pos < content.size() && content[pos] == '{') {
                // Nested block
                pos++; // skip '{'
                QVariantMap nested = parseVdfContent(content, pos);
                result[key] = nested;
            }
        } else {
            break;
        }
    }

    return result;
}

QVariantMap SteamUtils::parseVdfContent(const QString &content, int &pos)
{
    QVariantMap result;

    while (pos < content.size()) {
        skipWhitespace(content, pos);
        if (pos >= content.size()) break;

        if (content[pos] == '}') {
            pos++; // skip '}'
            break;
        }

        if (content[pos] == '"') {
            QString key = parseVdfString(content, pos);
            skipWhitespace(content, pos);

            if (pos < content.size() && content[pos] == '"') {
                QString value = parseVdfString(content, pos);
                result[key] = value;
            } else if (pos < content.size() && content[pos] == '{') {
                pos++; // skip '{'
                QVariantMap nested = parseVdfContent(content, pos);
                result[key] = nested;
            }
        } else {
            pos++; // skip unexpected character
        }
    }

    return result;
}

QString SteamUtils::parseVdfString(const QString &content, int &pos)
{
    if (pos >= content.size() || content[pos] != '"') {
        return QString();
    }

    pos++; // skip opening quote
    int start = pos;

    while (pos < content.size() && content[pos] != '"') {
        if (content[pos] == '\\' && pos + 1 < content.size()) {
            pos++; // skip escaped character
        }
        pos++;
    }

    QString result = content.mid(start, pos - start);

    if (pos < content.size()) {
        pos++; // skip closing quote
    }

    return result;
}

void SteamUtils::skipWhitespace(const QString &content, int &pos)
{
    while (pos < content.size() && content[pos].isSpace()) {
        pos++;
    }

    // Skip comments (// to end of line)
    if (pos + 1 < content.size() && content[pos] == '/' && content[pos + 1] == '/') {
        while (pos < content.size() && content[pos] != '\n') {
            pos++;
        }
        skipWhitespace(content, pos);
    }
}

QString SteamUtils::getSteamUserId(const QString &steamPath)
{
    if (steamPath.isEmpty()) {
        return QString();
    }

    QString userdataPath = steamPath + "/userdata";
    QDir userdataDir(userdataPath);

    if (!userdataDir.exists()) {
        return QString();
    }

    QStringList userDirs = userdataDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    if (userDirs.isEmpty()) {
        return QString();
    }

    // If only one user, return it
    if (userDirs.size() == 1) {
        return userDirs.first();
    }

    // Multiple users: pick the most recently modified
    QString mostRecent;
    QDateTime latestTime;

    for (const QString &dir : userDirs) {
        QFileInfo info(userdataPath + "/" + dir);
        if (latestTime.isNull() || info.lastModified() > latestTime) {
            latestTime = info.lastModified();
            mostRecent = dir;
        }
    }

    return mostRecent;
}

QString SteamUtils::findProtonPrefix(const QString &appId, const QStringList &libraryFolders)
{
    for (const QString &library : libraryFolders) {
        QString prefixPath = library + "/steamapps/compatdata/" + appId + "/pfx";
        if (QDir(prefixPath).exists()) {
            return prefixPath;
        }
    }
    return QString();
}
