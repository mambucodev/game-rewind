#ifndef STEAMUTILS_H
#define STEAMUTILS_H

#include <QString>
#include <QStringList>
#include <QVariantMap>

struct SteamAppInfo {
    QString appId;
    QString name;
    QString installDir;
    QString libraryPath;
};

class SteamUtils {
public:
    static QString findSteamPath();
    static QStringList getLibraryFolders(const QString &steamPath);
    static QList<SteamAppInfo> scanInstalledGames(const QStringList &libraryFolders);
    static SteamAppInfo parseAppManifest(const QString &manifestPath, const QString &libraryPath);
    static QVariantMap parseVdf(const QString &filePath);
    static QString getSteamUserId(const QString &steamPath);
    static QString findProtonPrefix(const QString &appId, const QStringList &libraryFolders);

private:
    static QVariantMap parseVdfContent(const QString &content, int &pos);
    static QString parseVdfString(const QString &content, int &pos);
    static void skipWhitespace(const QString &content, int &pos);
};

#endif // STEAMUTILS_H
