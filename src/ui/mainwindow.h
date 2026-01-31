#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include <QSystemTrayIcon>
#include "steam/gamedetector.h"
#include "core/savemanager.h"
#include "steam/manifestmanager.h"
#include "core/database.h"
#include "core/gameinfo.h"

class QLabel;
class QLineEdit;
class QProgressBar;
class QStackedWidget;
class QFileSystemWatcher;
class QTimer;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onGameSelected();
    void onBackupSelected(QListWidgetItem *current, QListWidgetItem *previous);
    void onCreateBackup();
    void onRestoreBackup();
    void onDeleteBackup();
    void onAddCustomGame();
    void onScanGame();
    void onRefreshGames();
    void onManageConfigs();
    void onSettings();
    void onAbout();

    void onBackupCreated(const QString &gameId, const QString &backupId);
    void onBackupRestored(const QString &gameId, const QString &backupId);
    void onBackupDeleted(const QString &gameId, const QString &backupId);
    void onError(const QString &message);

    void onManifestReady();
    void onGameContextMenu(const QPoint &pos);
    void onBackupContextMenu(const QPoint &pos);
    void onEditBackup();
    void onBackupUpdated(const QString &gameId, const QString &backupId);
    void onSearchTextChanged(const QString &text);
    void onBackUpAll();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onSaveDirectoryChanged(const QString &path);
    void onAutoBackupTimer();
    void onHideGame();
    void onManageHiddenGames();

private:
    void setupConnections();
    void setupKeyboardShortcuts();
    void setupEmptyStates();
    void showOnboardingIfNeeded();
    void updateGamesEmptyState();
    void updateBackupsEmptyState();
    void loadGames();
    void loadGamesFromCache();
    void populateGameTree(const QList<GameInfo> &games);
    void loadBackupsForGame(const QString &gameId);
    void updateGameCard(const QString &gameId);
    void setBackupsEnabled(bool enabled);
    void styleBackupItem(QListWidgetItem *item, const BackupInfo &backup);
    QString formatFileSize(qint64 bytes) const;
    QString formatTimestamp(const QDateTime &timestamp) const;
    GameInfo getCurrentGame() const;
    BackupInfo getCurrentBackup() const;

    void updateStorageUsage();
    void setOperationInProgress(bool inProgress, const QString &message = QString());
    void setupTrayIcon();
    void setupFileWatcher();
    void updateFileWatcher();
    void performAutoBackup(const QString &gameId);

    QMap<QString, QString> loadSavePathOverrides() const;
    void saveSavePathOverride(const QString &gameId, const QString &path);

    Ui::MainWindow *ui;
    GameDetector *m_gameDetector;
    SaveManager *m_saveManager;
    ManifestManager *m_manifestManager;
    Database *m_database;
    QString m_currentGameId;
    QLabel *m_storageLabel;
    QProgressBar *m_progressBar;
    QLineEdit *m_searchEdit;
    QStackedWidget *m_gamesStack;
    QLabel *m_gamesEmptyLabel;
    QStackedWidget *m_backupsStack;
    QLabel *m_backupsEmptyLabel;
    QList<GameInfo> m_bulkBackupQueue;
    void processNextBulkBackup();

    QSystemTrayIcon *m_trayIcon = nullptr;
    QFileSystemWatcher *m_fileWatcher = nullptr;
    QTimer *m_autoBackupTimer = nullptr;
    QMap<QString, QString> m_watchedPathToGameId;
    QSet<QString> m_pendingAutoBackups;
};

#endif // MAINWINDOW_H
