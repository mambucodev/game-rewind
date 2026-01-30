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
