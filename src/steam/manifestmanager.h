#ifndef MANIFESTMANAGER_H
#define MANIFESTMANAGER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>

struct FileConstraint {
    QString os;
    QString store;
};

struct ManifestFileEntry {
    QString path;
    QStringList tags;
    QList<FileConstraint> when;
};

struct ManifestGameEntry {
    QString name;
    int steamId = 0;
    QStringList installDirs;
    QList<ManifestFileEntry> files;
};

class ManifestManager : public QObject {
    Q_OBJECT

public:
    explicit ManifestManager(QObject *parent = nullptr);

    bool loadCachedManifest();
    void checkForUpdates();

    ManifestGameEntry findBySteamId(int steamAppId) const;
    QStringList getLinuxSavePaths(const ManifestGameEntry &entry,
                                  const QString &steamLibraryPath) const;
    QStringList getProtonSavePaths(const ManifestGameEntry &entry,
                                   const QString &protonPrefixPath,
                                   const QString &steamLibraryPath) const;

    bool isLoaded() const;

signals:
    void manifestReady();
    void manifestUpdateFailed(const QString &reason);

private slots:
    void onDownloadFinished(QNetworkReply *reply);

private:
    bool parseManifestFile(const QString &filePath);
    QString expandManifestPath(const QString &path,
                               const ManifestGameEntry &entry,
                               const QString &steamLibraryPath) const;
    QString expandProtonPath(const QString &path,
                              const ManifestGameEntry &entry,
                              const QString &protonPrefixPath,
                              const QString &steamLibraryPath) const;
    QString getCachePath() const;
    QString getETagPath() const;

    QMap<int, ManifestGameEntry> m_steamIdIndex;
    QNetworkAccessManager *m_networkManager;
    bool m_loaded = false;
    bool m_downloading = false;

    static const QString MANIFEST_URL;
};

#endif // MANIFESTMANAGER_H
