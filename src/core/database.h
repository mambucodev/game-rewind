#ifndef DATABASE_H
#define DATABASE_H

#include <QObject>
#include <QString>
#include <QList>
#include <QSet>
#include "gameinfo.h"

class Database : public QObject {
    Q_OBJECT

public:
    explicit Database(QObject *parent = nullptr);
    ~Database();

    bool open();
    void close();

    QList<GameInfo> getAllCustomGames() const;
    GameInfo getCustomGame(const QString &id) const;
    bool addCustomGame(const GameInfo &game);
    bool updateCustomGame(const GameInfo &game);
    bool removeCustomGame(const QString &id);
    bool customGameExists(const QString &id) const;

    int migrateFromJson(const QString &configDir);

    bool hideGame(const QString &gameId, const QString &name);
    bool unhideGame(const QString &gameId);
    bool isGameHidden(const QString &gameId) const;
    QSet<QString> getHiddenGameIds() const;
    QList<QPair<QString, QString>> getHiddenGames() const;

    // Save profiles
    QList<SaveProfile> getProfilesForGame(const QString &gameId) const;
    SaveProfile getProfile(int profileId) const;
    int addProfile(const SaveProfile &profile);
    bool updateProfile(const SaveProfile &profile);
    bool removeProfile(int profileId);
    bool profileExists(const QString &gameId, const QString &name) const;

    // App settings (generic key-value store)
    QString getSetting(const QString &key, const QString &defaultValue = QString()) const;
    bool setSetting(const QString &key, const QString &value);

    QString databasePath() const;

private:
    bool createTables();
    int schemaVersion() const;
    void setSchemaVersion(int version);
    void seedDefaults();

    static QString serializeSavePaths(const QStringList &paths);
    static QStringList deserializeSavePaths(const QString &json);

    QString m_dbPath;
    QString m_connectionName;
};

#endif // DATABASE_H
