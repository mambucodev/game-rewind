#ifndef GAMEDETECTOR_H
#define GAMEDETECTOR_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QFutureWatcher>
#include "core/gameinfo.h"
#include "manifestmanager.h"
class Database;

class GameDetector : public QObject {
    Q_OBJECT

public:
    explicit GameDetector(QObject *parent = nullptr);
    ~GameDetector() override;

    void setManifestManager(ManifestManager *manager);
    void setHiddenGameIds(const QSet<QString> &ids);
    void setSavePathOverrides(const QMap<QString, QString> &overrides);
    void loadCustomGames(Database *db);
    void loadGamesAsync(Database *db);
    bool isDetecting() const;
    void waitForDetection();
    QList<GameInfo> getDetectedGames() const;
    GameInfo getGameById(const QString &id) const;
    QString scanForSavePath(const QString &gameName, const QString &hint = QString());

    bool loadCachedGames();
    void saveCachedGames() const;

signals:
    void detectionFinished();

private slots:
    void onAsyncDetectionFinished();

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
    QMap<QString, QString> m_savePathOverrides;
    ManifestManager *m_manifestManager = nullptr;

    // Async detection
    struct DetectionContext {
        QList<GameInfo> games;
        QSet<QString> customSteamIds;
        QSet<QString> hiddenGames;
        QMap<QString, QString> savePathOverrides;
        QString steamPath;
        QStringList steamLibraryFolders;
        bool manifestLoaded = false;
        QMap<int, ManifestGameEntry> steamIdIndex;
    };
    static QList<GameInfo> detectGamesInThread(DetectionContext ctx);
    QFutureWatcher<QList<GameInfo>> m_detectWatcher;
    bool m_detecting = false;
};

#endif // GAMEDETECTOR_H
