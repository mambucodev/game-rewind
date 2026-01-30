#ifndef GAMEINFO_H
#define GAMEINFO_H

#include <QString>
#include <QStringList>
#include <QDateTime>

struct SaveProfile {
    int id;
    QString gameId;
    QString name;
    QStringList files; // relative paths from detectedSavePath

    SaveProfile()
        : id(-1) {}
};

struct GameInfo {
    QString id;
    QString name;
    QString platform;
    QString steamAppId;
    QStringList savePaths;
    QString detectedSavePath;
    QStringList alternativeSavePaths; // other valid paths (native/proton)
    QString source; // "database" or "manifest"
    bool isDetected;

    GameInfo()
        : isDetected(false) {}
};

struct BackupInfo {
    QString id;
    QString gameId;
    QString gameName;
    QString displayName;
    QString notes;  // User notes/description
    QDateTime timestamp;
    QString archivePath;
    qint64 size;
    QString profileName; // empty = "All files"
    int profileId;       // -1 = full directory backup

    BackupInfo()
        : size(0), profileId(-1) {}
};

#endif // GAMEINFO_H
