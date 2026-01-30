#ifndef GAMEDETECTOR_H
#define GAMEDETECTOR_H

#include <QObject>
#include <QList>
#include <QSet>
#include <QString>
#include "core/gameinfo.h"

class ManifestManager;
class Database;

class GameDetector : public QObject {
    Q_OBJECT

public:
    explicit GameDetector(QObject *parent = nullptr);

    void setManifestManager(ManifestManager *manager);
    void setHiddenGameIds(const QSet<QString> &ids);
    void loadCustomGames(Database *db);
    QList<GameInfo> getDetectedGames() const;
    GameInfo getGameById(const QString &id) const;
    QString scanForSavePath(const QString &gameName, const QString &hint = QString());

private:
    QString expandPath(const QString &path) const;
    bool pathExists(const QString &path) const;
    void detectGames();
    void detectManifestGames();
    bool isGameInstalled(const GameInfo &game) const;

    QList<GameInfo> m_games;
    QList<GameInfo> m_detectedGames;
    QString m_steamPath;
    QStringList m_steamLibraryFolders;
    QSet<QString> m_customSteamIds;
    QSet<QString> m_hiddenGames;
    ManifestManager *m_manifestManager = nullptr;
};

#endif // GAMEDETECTOR_H
