#ifndef GAMEINFO_H
#define GAMEINFO_H

#include <QString>
#include <QStringList>
#include <QDateTime>

struct GameInfo {
    QString id;
    QString name;
    QString platform;
    QString steamAppId;
    QStringList savePaths;
    QString detectedSavePath;
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

    BackupInfo()
        : size(0) {}
};

#endif // GAMEINFO_H
