#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidgetItem>
#include "steam/gamedetector.h"
#include "core/savemanager.h"
#include "steam/manifestmanager.h"
#include "core/database.h"
#include "core/gameinfo.h"

class QLabel;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

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
    void onHideGame();
    void onManageHiddenGames();

private:
    void setupConnections();
    void setupKeyboardShortcuts();
    void loadGames();
    void loadBackupsForGame(const QString &gameId);
    void updateGameCard(const QString &gameId);
    void setBackupsEnabled(bool enabled);
    void styleBackupItem(QListWidgetItem *item, const BackupInfo &backup);
    QString formatFileSize(qint64 bytes) const;
    QString formatTimestamp(const QDateTime &timestamp) const;
    GameInfo getCurrentGame() const;
    BackupInfo getCurrentBackup() const;

    void updateStorageUsage();

    Ui::MainWindow *ui;
    GameDetector *m_gameDetector;
    SaveManager *m_saveManager;
    ManifestManager *m_manifestManager;
    Database *m_database;
    QString m_currentGameId;
    QLabel *m_storageLabel;
};

#endif // MAINWINDOW_H
