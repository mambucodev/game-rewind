#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "gameconfigdialog.h"
#include "gameicon.h"
#include "backupitemdelegate.h"
#include "gamecarddelegate.h"
#include "backupdialog.h"
#include "addgamedialog.h"
#include "onboardingdialog.h"
#include "profiledialog.h"
#include "settingsdialog.h"
#include "bulkbackupdialog.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QMenu>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QStandardPaths>
#include <QDir>
#include <QTreeWidgetItem>
#include <QFont>
#include <QShortcut>
#include <QKeySequence>
#include <QLabel>
#include <QStackedWidget>
#include <QProgressBar>
#include <QFrame>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QCloseEvent>
#include <QApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_gameDetector(new GameDetector(this))
    , m_saveManager(new SaveManager(this))
    , m_manifestManager(new ManifestManager(this))
    , m_database(new Database(this))
{
    ui->setupUi(this);

    ui->splitter->setSizes(QList<int>() << 300 << 600);

    // Install custom delegates
    ui->backupsListWidget->setItemDelegate(new BackupItemDelegate(this));
    ui->gamesTreeWidget->setItemDelegate(new GameCardDelegate(this));

    // Enable right-click context menus
    ui->gamesTreeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->gamesTreeWidget, &QTreeWidget::customContextMenuRequested,
            this, &MainWindow::onGameContextMenu);
    ui->backupsListWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->backupsListWidget, &QListWidget::customContextMenuRequested,
            this, &MainWindow::onBackupContextMenu);

    setupEmptyStates();
    setupConnections();
    setupKeyboardShortcuts();

    // Add progress bar and storage usage label to status bar
    m_progressBar = new QProgressBar(this);
    m_progressBar->setMaximumWidth(200);
    m_progressBar->setMaximumHeight(16);
    m_progressBar->setRange(0, 0);  // Indeterminate
    m_progressBar->setVisible(false);
    ui->statusbar->addPermanentWidget(m_progressBar);

    m_storageLabel = new QLabel(this);
    m_storageLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    ui->statusbar->addPermanentWidget(m_storageLabel);

    // Open database
    if (!m_database->open()) {
        qCritical() << "Failed to open database";
    }

    // Apply saved settings
    QString savedBackupDir = m_database->getSetting("backup_directory");
    if (!savedBackupDir.isEmpty()) {
        m_saveManager->setBackupDirectory(savedBackupDir);
    }
    int compression = m_database->getSetting("compression_level", "6").toInt();
    m_saveManager->setCompressionLevel(compression);

    // Set up manifest manager
    m_gameDetector->setManifestManager(m_manifestManager);
    connect(m_manifestManager, &ManifestManager::manifestReady,
            this, &MainWindow::onManifestReady);

    // Fast startup: load cached detected games first (instant JSON read),
    // then parse the manifest in the background for a full refresh.
    loadGamesFromCache();
    updateStorageUsage();

    // Parse cached manifest in a background thread.
    // When done, onManifestReady() runs full detection and updates the cache.
    m_manifestManager->loadCachedManifestAsync();
    if (m_manifestManager->isParsing()) {
        ui->statusbar->showMessage("Loading game database...");
    } else {
        // No cached manifest (first run) -- run detection with custom games only
        loadGames();
        updateStorageUsage();
        showOnboardingIfNeeded();
    }

    setupTrayIcon();
    setupFileWatcher();

    // Check for manifest updates in background
    m_manifestManager->checkForUpdates();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupConnections()
{
    connect(ui->gamesTreeWidget, &QTreeWidget::currentItemChanged,
            this, &MainWindow::onGameSelected);
    connect(ui->backupsListWidget, &QListWidget::currentItemChanged,
            this, &MainWindow::onBackupSelected);

    // Toolbar actions
    connect(ui->actionCreateBackup, &QAction::triggered,
            this, &MainWindow::onCreateBackup);
    connect(ui->actionRestoreBackup, &QAction::triggered,
            this, &MainWindow::onRestoreBackup);
    connect(ui->actionDeleteBackup, &QAction::triggered,
            this, &MainWindow::onDeleteBackup);
    connect(ui->actionAddGame, &QAction::triggered,
            this, &MainWindow::onAddCustomGame);
    connect(ui->actionScanGame, &QAction::triggered,
            this, &MainWindow::onScanGame);
    connect(ui->actionBackUpAll, &QAction::triggered,
            this, &MainWindow::onBackUpAll);
    connect(ui->actionRefresh, &QAction::triggered,
            this, &MainWindow::onRefreshGames);
    connect(ui->actionManageConfigs, &QAction::triggered,
            this, &MainWindow::onManageConfigs);
    connect(ui->actionHiddenGames, &QAction::triggered,
            this, &MainWindow::onManageHiddenGames);
    connect(ui->actionSettings, &QAction::triggered,
            this, &MainWindow::onSettings);
    connect(ui->actionAbout, &QAction::triggered,
            this, &MainWindow::onAbout);

    connect(m_saveManager, &SaveManager::backupCreated,
            this, &MainWindow::onBackupCreated);
    connect(m_saveManager, &SaveManager::backupRestored,
            this, &MainWindow::onBackupRestored);
    connect(m_saveManager, &SaveManager::backupDeleted,
            this, &MainWindow::onBackupDeleted);
    connect(m_saveManager, &SaveManager::backupUpdated,
            this, &MainWindow::onBackupUpdated);
    connect(m_saveManager, &SaveManager::operationStarted, this, [this](const QString &msg) {
        setOperationInProgress(true, msg);
    });
    connect(m_saveManager, &SaveManager::operationFinished, this, [this]() {
        setOperationInProgress(false);
    });
    connect(m_saveManager, &SaveManager::operationCancelled, this, [this]() {
        setOperationInProgress(false);
        ui->statusbar->showMessage("Operation cancelled", 3000);
    });
    connect(m_saveManager, &SaveManager::error,
            this, &MainWindow::onError);
}

void MainWindow::setupKeyboardShortcuts()
{
    // Ctrl+B: Create backup
    QShortcut *backupShortcut = new QShortcut(QKeySequence("Ctrl+B"), this);
    connect(backupShortcut, &QShortcut::activated, this, &MainWindow::onCreateBackup);

    // Ctrl+R: Restore backup
    QShortcut *restoreShortcut = new QShortcut(QKeySequence("Ctrl+R"), this);
    connect(restoreShortcut, &QShortcut::activated, this, &MainWindow::onRestoreBackup);

    // Delete: Delete backup
    QShortcut *deleteShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), this);
    connect(deleteShortcut, &QShortcut::activated, this, &MainWindow::onDeleteBackup);

    // Ctrl+F: Focus search box
    QShortcut *findShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(findShortcut, &QShortcut::activated, this, [this]() {
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
    });

    // Escape: Clear search if focused
    QShortcut *escShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), m_searchEdit);
    connect(escShortcut, &QShortcut::activated, this, [this]() {
        if (m_searchEdit->hasFocus() && !m_searchEdit->text().isEmpty()) {
            m_searchEdit->clear();
        }
    });

    // F5: Refresh
    QShortcut *refreshShortcut = new QShortcut(QKeySequence(Qt::Key_F5), this);
    connect(refreshShortcut, &QShortcut::activated, this, &MainWindow::onRefreshGames);
}

void MainWindow::setupEmptyStates()
{
    // --- Games panel empty state ---
    m_gamesStack = new QStackedWidget(this);

    QVBoxLayout *leftLayout = qobject_cast<QVBoxLayout *>(ui->leftPanel->layout());
    int treeIndex = leftLayout->indexOf(ui->gamesTreeWidget);
    leftLayout->removeWidget(ui->gamesTreeWidget);
    m_gamesStack->addWidget(ui->gamesTreeWidget);    // page 0: tree

    QWidget *gamesEmptyPage = new QWidget(this);
    QVBoxLayout *gamesEmptyLayout = new QVBoxLayout(gamesEmptyPage);
    gamesEmptyLayout->setAlignment(Qt::AlignCenter);

    QLabel *gamesIconLabel = new QLabel(gamesEmptyPage);
    gamesIconLabel->setPixmap(
        QIcon::fromTheme("applications-games").pixmap(48, 48));
    gamesIconLabel->setAlignment(Qt::AlignCenter);
    gamesIconLabel->setEnabled(false);

    m_gamesEmptyLabel = new QLabel(
        "No games detected\n\n"
        "Click Add Game in the toolbar to add a game manually,\n"
        "or press Refresh to re-scan your system.",
        gamesEmptyPage);
    m_gamesEmptyLabel->setAlignment(Qt::AlignCenter);
    m_gamesEmptyLabel->setWordWrap(true);
    QFont gamesEmptyFont = m_gamesEmptyLabel->font();
    gamesEmptyFont.setBold(true);
    m_gamesEmptyLabel->setFont(gamesEmptyFont);

    QPalette pal = m_gamesEmptyLabel->palette();
    QColor muted = pal.color(QPalette::WindowText);
    muted.setAlphaF(0.45);
    pal.setColor(QPalette::WindowText, muted);
    m_gamesEmptyLabel->setPalette(pal);

    gamesEmptyLayout->addWidget(gamesIconLabel);
    gamesEmptyLayout->addSpacing(8);
    gamesEmptyLayout->addWidget(m_gamesEmptyLabel);

    m_gamesStack->addWidget(gamesEmptyPage);          // page 1: empty
    leftLayout->insertWidget(treeIndex, m_gamesStack);

    // Add search box between label and games stack
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search games...");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->addAction(QIcon::fromTheme("edit-find"), QLineEdit::LeadingPosition);
    leftLayout->insertWidget(1, m_searchEdit);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &MainWindow::onSearchTextChanged);

    // --- Backups panel empty state ---
    m_backupsStack = new QStackedWidget(this);

    QVBoxLayout *rightLayout = qobject_cast<QVBoxLayout *>(ui->rightPanel->layout());
    int listIndex = rightLayout->indexOf(ui->backupsListWidget);
    rightLayout->removeWidget(ui->backupsListWidget);
    m_backupsStack->addWidget(ui->backupsListWidget);  // page 0: list

    QWidget *backupsEmptyPage = new QWidget(this);
    QVBoxLayout *backupsEmptyLayout = new QVBoxLayout(backupsEmptyPage);
    backupsEmptyLayout->setAlignment(Qt::AlignCenter);

    QLabel *backupsIconLabel = new QLabel(backupsEmptyPage);
    backupsIconLabel->setPixmap(
        QIcon::fromTheme("document-save").pixmap(48, 48));
    backupsIconLabel->setAlignment(Qt::AlignCenter);
    backupsIconLabel->setEnabled(false);

    m_backupsEmptyLabel = new QLabel(
        "Select a game to view its backups",
        backupsEmptyPage);
    m_backupsEmptyLabel->setAlignment(Qt::AlignCenter);
    m_backupsEmptyLabel->setWordWrap(true);
    QFont backupsEmptyFont = m_backupsEmptyLabel->font();
    backupsEmptyFont.setBold(true);
    m_backupsEmptyLabel->setFont(backupsEmptyFont);
    m_backupsEmptyLabel->setPalette(pal);  // reuse muted palette

    backupsEmptyLayout->addWidget(backupsIconLabel);
    backupsEmptyLayout->addSpacing(8);
    backupsEmptyLayout->addWidget(m_backupsEmptyLabel);

    m_backupsStack->addWidget(backupsEmptyPage);        // page 1: empty
    rightLayout->insertWidget(listIndex, m_backupsStack);

    // Start with empty state for backups (no game selected yet)
    m_backupsStack->setCurrentIndex(1);
}

void MainWindow::showOnboardingIfNeeded()
{
    if (m_database->getSetting("onboarding_completed", "0") == "1") {
        return;
    }

    OnboardingDialog dialog(m_gameDetector->getDetectedGames(), this);
    dialog.exec();

    m_database->setSetting("onboarding_completed", "1");
}

void MainWindow::updateGamesEmptyState()
{
    bool hasGames = false;
    for (int i = 0; i < ui->gamesTreeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *category = ui->gamesTreeWidget->topLevelItem(i);
        if (category->childCount() > 0) {
            hasGames = true;
            break;
        }
    }
    m_gamesStack->setCurrentIndex(hasGames ? 0 : 1);
}

void MainWindow::updateBackupsEmptyState()
{
    if (m_currentGameId.isEmpty()) {
        m_backupsEmptyLabel->setText("Select a game to view its backups");
        m_backupsStack->setCurrentIndex(1);
    } else if (ui->backupsListWidget->count() == 0) {
        m_backupsEmptyLabel->setText(
            "No backups yet\n\n"
            "Click Create Backup in the toolbar or press Ctrl+B\n"
            "to create your first backup.");
        m_backupsStack->setCurrentIndex(1);
    } else {
        m_backupsStack->setCurrentIndex(0);
    }
}

void MainWindow::loadGames()
{
    // Migrate legacy JSON configs if they exist (idempotent)
    QString legacyConfigDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                              + "/game-rewind/configs";
    if (QDir(legacyConfigDir).exists()) {
        m_database->migrateFromJson(legacyConfigDir);
    }

    // Load overrides and hidden games, then detect
    m_gameDetector->setHiddenGameIds(m_database->getHiddenGameIds());
    m_gameDetector->setSavePathOverrides(loadSavePathOverrides());
    m_gameDetector->loadCustomGames(m_database);

    QList<GameInfo> games = m_gameDetector->getDetectedGames();

    // Filter out hidden games from cache-loaded entries
    QSet<QString> hidden = m_database->getHiddenGameIds();
    QList<GameInfo> filtered;
    for (const GameInfo &game : games) {
        if (!hidden.contains(game.id)) {
            filtered.append(game);
        }
    }

    populateGameTree(filtered);

    // Save cache so next launch is instant
    m_gameDetector->saveCachedGames();

    updateFileWatcher();
}

void MainWindow::loadGamesFromCache()
{
    if (!m_gameDetector->loadCachedGames()) {
        return;
    }

    QList<GameInfo> games = m_gameDetector->getDetectedGames();

    // Filter out hidden games
    QSet<QString> hidden = m_database->getHiddenGameIds();
    QList<GameInfo> filtered;
    for (const GameInfo &game : games) {
        if (!hidden.contains(game.id)) {
            filtered.append(game);
        }
    }

    populateGameTree(filtered);
}

void MainWindow::populateGameTree(const QList<GameInfo> &games)
{
    ui->gamesTreeWidget->clear();

    // Group games by platform
    QMap<QString, QList<GameInfo>> platformGames;
    for (const GameInfo &game : games) {
        platformGames[game.platform].append(game);
    }

    // Platform display names and order
    QStringList platformOrder = {"steam", "native", "custom"};
    QMap<QString, QString> platformNames = {
        {"steam", "Steam"},
        {"native", "Native"},
        {"custom", "Custom"}
    };

    for (const QString &platform : platformOrder) {
        if (!platformGames.contains(platform)) {
            continue;
        }

        // Create platform category
        QTreeWidgetItem *categoryItem = new QTreeWidgetItem(ui->gamesTreeWidget);
        categoryItem->setText(0, platformNames[platform]);
        categoryItem->setIcon(0, GameIconProvider::getPlatformIcon(platform));
        categoryItem->setData(0, GameCardRoles::IsCategoryRole, true);
        categoryItem->setData(0, Qt::UserRole, QString()); // Empty ID for categories
        categoryItem->setExpanded(true);

        // Add games under this platform
        for (const GameInfo &game : platformGames[platform]) {
            // Get backup stats
            QList<BackupInfo> backups = m_saveManager->getBackupsForGame(game.id);
            int backupCount = backups.size();
            qint64 totalSize = 0;
            for (const BackupInfo &backup : backups) {
                totalSize += backup.size;
            }

            // Get full-resolution capsule image (no scaling down)
            QPixmap capsule = GameIconProvider::getHighResCapsule(game);

            QTreeWidgetItem *gameItem = new QTreeWidgetItem(categoryItem);
            gameItem->setData(0, GameCardRoles::IsCategoryRole, false);
            gameItem->setData(0, GameCardRoles::GameIdRole, game.id);
            gameItem->setData(0, GameCardRoles::GameNameRole, game.name);
            gameItem->setData(0, GameCardRoles::GameIconRole, capsule);
            gameItem->setData(0, GameCardRoles::BackupCountRole, backupCount);
            gameItem->setData(0, GameCardRoles::TotalSizeRole, totalSize);
            gameItem->setData(0, GameCardRoles::SavePathRole, game.detectedSavePath);
            gameItem->setData(0, GameCardRoles::PlatformRole, game.platform);
            QDateTime lastBackupTime;
            if (!backups.isEmpty()) {
                lastBackupTime = backups.first().timestamp;
            }
            gameItem->setData(0, GameCardRoles::LastBackupRole, lastBackupTime);
            gameItem->setData(0, Qt::UserRole, game.id); // Keep for compatibility
            gameItem->setToolTip(0, game.detectedSavePath);
        }
    }

    // Detect orphaned games (backups exist but game no longer detected)
    QSet<QString> detectedIds;
    for (const GameInfo &game : games) {
        detectedIds.insert(game.id);
    }

    QStringList allBackupGameIds = m_saveManager->getAllGameIdsWithBackups();
    QStringList orphanedIds;
    for (const QString &id : allBackupGameIds) {
        if (!detectedIds.contains(id) && !m_database->isGameHidden(id)) {
            orphanedIds.append(id);
        }
    }

    if (!orphanedIds.isEmpty()) {
        QTreeWidgetItem *orphanCategory = new QTreeWidgetItem(ui->gamesTreeWidget);
        orphanCategory->setText(0, "Undetected");
        orphanCategory->setIcon(0, QIcon::fromTheme("dialog-warning"));
        orphanCategory->setData(0, GameCardRoles::IsCategoryRole, true);
        orphanCategory->setData(0, Qt::UserRole, QString());
        orphanCategory->setExpanded(true);

        for (const QString &orphanId : orphanedIds) {
            QList<BackupInfo> backups = m_saveManager->getBackupsForGame(orphanId);
            int backupCount = backups.size();
            qint64 totalSize = 0;
            for (const BackupInfo &backup : backups) {
                totalSize += backup.size;
            }
            QDateTime lastBackupTime;
            if (!backups.isEmpty()) {
                lastBackupTime = backups.first().timestamp;
            }

            QString gameName = m_saveManager->getGameNameFromBackups(orphanId);

            QTreeWidgetItem *gameItem = new QTreeWidgetItem(orphanCategory);
            gameItem->setData(0, GameCardRoles::IsCategoryRole, false);
            gameItem->setData(0, GameCardRoles::GameIdRole, orphanId);
            gameItem->setData(0, GameCardRoles::GameNameRole, gameName);
            gameItem->setData(0, GameCardRoles::GameIconRole, QPixmap());
            gameItem->setData(0, GameCardRoles::BackupCountRole, backupCount);
            gameItem->setData(0, GameCardRoles::TotalSizeRole, totalSize);
            gameItem->setData(0, GameCardRoles::SavePathRole, QString());
            gameItem->setData(0, GameCardRoles::PlatformRole, "undetected");
            gameItem->setData(0, GameCardRoles::LastBackupRole, lastBackupTime);
            gameItem->setData(0, Qt::UserRole, orphanId);
            gameItem->setToolTip(0, "This game is no longer detected. Its backups are still available.");
        }
    }

    ui->statusbar->showMessage(QString("Detected %1 games").arg(games.size()));

    updateGamesEmptyState();
}

void MainWindow::loadBackupsForGame(const QString &gameId)
{
    ui->backupsListWidget->clear();
    setBackupsEnabled(false);

    QList<BackupInfo> backups = m_saveManager->getBackupsForGame(gameId);

    for (const BackupInfo &backup : backups) {
        QListWidgetItem *item = new QListWidgetItem();
        item->setData(Qt::UserRole, backup.id);
        styleBackupItem(item, backup);
        ui->backupsListWidget->addItem(item);
    }

    setBackupsEnabled(true);
    updateBackupsEmptyState();
}

void MainWindow::updateGameCard(const QString &gameId)
{
    // Find the game item in the tree
    QTreeWidgetItemIterator it(ui->gamesTreeWidget);
    while (*it) {
        QTreeWidgetItem *item = *it;
        if (item->data(0, GameCardRoles::GameIdRole).toString() == gameId) {
            // Recalculate backup stats
            QList<BackupInfo> backups = m_saveManager->getBackupsForGame(gameId);
            int backupCount = backups.size();
            qint64 totalSize = 0;
            for (const BackupInfo &backup : backups) {
                totalSize += backup.size;
            }

            // Update the item data
            item->setData(0, GameCardRoles::BackupCountRole, backupCount);
            item->setData(0, GameCardRoles::TotalSizeRole, totalSize);
            QDateTime lastBackupTime;
            if (!backups.isEmpty()) {
                lastBackupTime = backups.first().timestamp;
            }
            item->setData(0, GameCardRoles::LastBackupRole, lastBackupTime);

            // Force repaint
            ui->gamesTreeWidget->update(ui->gamesTreeWidget->indexFromItem(item));
            break;
        }
        ++it;
    }
}

void MainWindow::setBackupsEnabled(bool enabled)
{
    ui->backupsListWidget->setEnabled(enabled);
}

void MainWindow::styleBackupItem(QListWidgetItem *item, const BackupInfo &backup)
{
    // Create multi-line text with styling
    QString profileTag;
    if (!backup.profileName.isEmpty()) {
        profileTag = QString(" <span style='color: #5599cc;'>[%1]</span>").arg(backup.profileName.toHtmlEscaped());
    }

    QString displayText = QString("<b>%1</b>%2<br><span style='color: gray; font-size: 9pt;'>%3 &bull; %4</span>")
                              .arg(backup.displayName.toHtmlEscaped())
                              .arg(profileTag)
                              .arg(formatTimestamp(backup.timestamp))
                              .arg(formatFileSize(backup.size));

    item->setText(displayText);
    item->setIcon(QIcon::fromTheme("document-save"));
    item->setSizeHint(QSize(0, 48)); // Make items taller

    // Build rich tooltip with notes if available
    QString tooltip = QString(
        "<html>"
        "<body style='white-space: pre-wrap;'>"
        "<p style='margin: 4px 0;'><b>%1</b></p>"
        "<p style='margin: 4px 0; color: gray;'>Created: %2</p>"
        "<p style='margin: 4px 0; color: gray;'>Size: %3</p>"
    ).arg(backup.displayName)
     .arg(backup.timestamp.toString("MMMM d, yyyy 'at' h:mm AP"))
     .arg(formatFileSize(backup.size));

    if (!backup.notes.isEmpty()) {
        tooltip += QString("<hr style='margin: 8px 0; border: 1px solid #444;'>"
                          "<p style='margin: 4px 0;'><b>Notes:</b></p>"
                          "<p style='margin: 4px 0;'>%1</p>")
                      .arg(backup.notes.toHtmlEscaped());
    }

    tooltip += "</body></html>";

    item->setToolTip(tooltip);
}

QString MainWindow::formatTimestamp(const QDateTime &timestamp) const
{
    QDateTime now = QDateTime::currentDateTime();
    qint64 secondsAgo = timestamp.secsTo(now);

    if (secondsAgo < 60) {
        return "Just now";
    } else if (secondsAgo < 3600) {
        int mins = secondsAgo / 60;
        return QString("%1 minute%2 ago").arg(mins).arg(mins > 1 ? "s" : "");
    } else if (secondsAgo < 86400) {
        int hours = secondsAgo / 3600;
        return QString("%1 hour%2 ago").arg(hours).arg(hours > 1 ? "s" : "");
    } else if (secondsAgo < 604800) {
        int days = secondsAgo / 86400;
        return QString("%1 day%2 ago").arg(days).arg(days > 1 ? "s" : "");
    } else {
        return timestamp.toString("MMM d, yyyy");
    }
}

QString MainWindow::formatFileSize(qint64 bytes) const
{
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;

    if (bytes >= GB) {
        return QString::number(bytes / GB) + " GB";
    } else if (bytes >= MB) {
        return QString::number(bytes / MB) + " MB";
    } else if (bytes >= KB) {
        return QString::number(bytes / KB) + " KB";
    } else {
        return QString::number(bytes) + " bytes";
    }
}

GameInfo MainWindow::getCurrentGame() const
{
    return m_gameDetector->getGameById(m_currentGameId);
}

BackupInfo MainWindow::getCurrentBackup() const
{
    QListWidgetItem *item = ui->backupsListWidget->currentItem();
    if (!item) {
        return BackupInfo();
    }

    QString backupId = item->data(Qt::UserRole).toString();
    return m_saveManager->getBackupById(m_currentGameId, backupId);
}

void MainWindow::onGameSelected()
{
    QTreeWidgetItem *current = ui->gamesTreeWidget->currentItem();

    if (!current) {
        m_currentGameId.clear();
        ui->selectedGameLabel->setText("No game selected");
        ui->actionCreateBackup->setEnabled(false);
        ui->backupsListWidget->clear();
        updateBackupsEmptyState();
        return;
    }

    // Check if it's a category item (no game ID)
    QString gameId = current->data(0, Qt::UserRole).toString();
    if (gameId.isEmpty()) {
        m_currentGameId.clear();
        ui->selectedGameLabel->setText("Select a game from the list");
        ui->actionCreateBackup->setEnabled(false);
        ui->backupsListWidget->clear();
        updateBackupsEmptyState();
        return;
    }

    m_currentGameId = gameId;
    GameInfo game = getCurrentGame();

    if (game.id.isEmpty() && !m_currentGameId.isEmpty()) {
        // Orphaned game -- no longer detected
        QTreeWidgetItem *currentItem = ui->gamesTreeWidget->currentItem();
        QString name = currentItem ? currentItem->data(0, GameCardRoles::GameNameRole).toString() : m_currentGameId;
        ui->selectedGameLabel->setText(QString("Game: %1 (undetected)").arg(name));
        ui->actionCreateBackup->setEnabled(false);
    } else {
        ui->selectedGameLabel->setText(QString("Game: %1").arg(game.name));
        ui->actionCreateBackup->setEnabled(true);
    }

    loadBackupsForGame(m_currentGameId);
}

void MainWindow::onBackupSelected(QListWidgetItem *current, QListWidgetItem *previous)
{
    Q_UNUSED(previous);

    bool hasSelection = (current != nullptr);
    ui->actionRestoreBackup->setEnabled(hasSelection);
    ui->actionDeleteBackup->setEnabled(hasSelection);
}

void MainWindow::onCreateBackup()
{
    if (m_saveManager->isBusy()) return;

    GameInfo game = getCurrentGame();
    if (game.id.isEmpty()) {
        return;
    }

    QList<SaveProfile> profiles = m_database->getProfilesForGame(game.id);

    BackupDialog dialog(profiles, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    m_saveManager->createBackupAsync(game,
        dialog.getBackupName(), dialog.getBackupNotes(), dialog.getSelectedProfile());
}

void MainWindow::onRestoreBackup()
{
    if (m_saveManager->isBusy()) return;

    GameInfo game = getCurrentGame();
    BackupInfo backup = getCurrentBackup();

    if (backup.id.isEmpty()) {
        return;
    }

    QString gameName = game.name;
    if (gameName.isEmpty()) {
        QTreeWidgetItem *current = ui->gamesTreeWidget->currentItem();
        gameName = current ? current->data(0, GameCardRoles::GameNameRole).toString() : m_currentGameId;
    }

    QString message;
    if (backup.profileId != -1) {
        message = QString("Are you sure you want to restore '%1' (profile: %2)?\n\n"
                          "This will overwrite only the files in that profile for %3.")
                      .arg(backup.displayName, backup.profileName, gameName);
    } else {
        message = QString("Are you sure you want to restore the backup '%1'?\n\n"
                          "This will replace the current save files for %2.")
                      .arg(backup.displayName, gameName);
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this,
        "Restore Backup", message, QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    QString restorePath = game.detectedSavePath;
    if (restorePath.isEmpty()) {
        restorePath = QFileDialog::getExistingDirectory(this,
            "Select Restore Location",
            QDir::homePath(),
            QFileDialog::ShowDirsOnly);
        if (restorePath.isEmpty()) return;
    }

    m_saveManager->restoreBackupAsync(backup, restorePath);
}

void MainWindow::onDeleteBackup()
{
    BackupInfo backup = getCurrentBackup();
    if (backup.id.isEmpty()) {
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this,
        "Delete Backup",
        QString("Are you sure you want to delete the backup '%1'?")
            .arg(backup.displayName),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    if (m_saveManager->deleteBackup(backup)) {
        ui->statusbar->showMessage("Backup deleted successfully", 3000);
    } else {
        ui->statusbar->showMessage("Failed to delete backup", 3000);
    }
}

void MainWindow::onAddCustomGame()
{
    AddGameDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString gameName = dialog.getGameName();
    QString savePath = dialog.getSavePath();
    QString platform = dialog.getPlatform();
    QString steamAppId = dialog.getSteamAppId();

    GameInfo game;
    if (platform == "steam" && !steamAppId.isEmpty()) {
        game.id = "steam_" + steamAppId;
    } else {
        game.id = "custom_" + QString::number(QDateTime::currentSecsSinceEpoch());
    }
    game.name = gameName;
    game.platform = platform;
    game.steamAppId = steamAppId;
    game.savePaths << savePath;
    game.source = "database";

    if (m_database->addCustomGame(game)) {
        loadGames();
        ui->statusbar->showMessage("Game added successfully: " + gameName, 3000);
    } else {
        QMessageBox::warning(this, "Error", "Failed to save game configuration.");
    }
}

void MainWindow::onScanGame()
{
    bool ok;
    QString gameName = QInputDialog::getText(this,
                                             "Scan for Game",
                                             "Game name:",
                                             QLineEdit::Normal,
                                             "",
                                             &ok);

    if (!ok || gameName.isEmpty()) {
        return;
    }

    QString hint = QInputDialog::getText(this,
                                         "Scan for Game",
                                         "Path hint (optional, e.g., ~/.local/share/gamename):",
                                         QLineEdit::Normal,
                                         "",
                                         &ok);

    QString foundPath = m_gameDetector->scanForSavePath(gameName, hint);

    if (foundPath.isEmpty()) {
        QMessageBox::information(this,
                                "Scan Result",
                                "Could not find save path automatically. Please add manually.");
    } else {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            "Scan Result",
            QString("Found save path:\n%1\n\nAdd this game?").arg(foundPath),
            QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            GameInfo game;
            game.id = "custom_" + QString::number(QDateTime::currentSecsSinceEpoch());
            game.name = gameName;
            game.platform = "custom";
            game.savePaths << foundPath;
            game.source = "database";

            if (m_database->addCustomGame(game)) {
                loadGames();
                ui->statusbar->showMessage("Game added successfully", 3000);
            }
        }
    }
}

void MainWindow::onRefreshGames()
{
    loadGames();
}

void MainWindow::onManageConfigs()
{
    GameConfigDialog dialog(m_database, this);
    connect(&dialog, &GameConfigDialog::configsChanged, this, &MainWindow::onRefreshGames);
    dialog.exec();
}

void MainWindow::onBackupContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = ui->backupsListWidget->itemAt(pos);
    if (!item) return;

    QMenu menu(this);
    QAction *editAction = menu.addAction(QIcon::fromTheme("document-edit"), "Edit Backup...");
    menu.addSeparator();
    QAction *restoreAction = menu.addAction(QIcon::fromTheme("document-revert"), "Restore");
    QAction *deleteAction = menu.addAction(QIcon::fromTheme("edit-delete"), "Delete");
    menu.addSeparator();
    QAction *verifyAction = menu.addAction(QIcon::fromTheme("dialog-ok-apply"), "Verify Integrity");

    QAction *selected = menu.exec(ui->backupsListWidget->viewport()->mapToGlobal(pos));
    if (!selected) return;

    ui->backupsListWidget->setCurrentItem(item);

    if (selected == editAction) {
        onEditBackup();
    } else if (selected == restoreAction) {
        onRestoreBackup();
    } else if (selected == deleteAction) {
        onDeleteBackup();
    } else if (selected == verifyAction) {
        BackupInfo backup = getCurrentBackup();
        if (!backup.id.isEmpty()) {
            ui->statusbar->showMessage("Verifying backup...");
            bool valid = m_saveManager->verifyBackup(backup);
            if (valid) {
                ui->statusbar->showMessage("Backup integrity verified", 3000);
            } else {
                QMessageBox::warning(this, "Integrity Check",
                    "Backup archive may be corrupted or incomplete.");
                ui->statusbar->showMessage("Backup verification FAILED", 5000);
            }
        }
    }
}

void MainWindow::onEditBackup()
{
    BackupInfo backup = getCurrentBackup();
    if (backup.id.isEmpty()) return;

    QDialog dialog(this);
    dialog.setWindowTitle("Edit Backup");
    dialog.setMinimumWidth(400);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *nameLabel = new QLabel("Backup Name:", &dialog);
    QLineEdit *nameEdit = new QLineEdit(backup.displayName, &dialog);
    layout->addWidget(nameLabel);
    layout->addWidget(nameEdit);

    QLabel *notesLabel = new QLabel("Notes:", &dialog);
    QTextEdit *notesEdit = new QTextEdit(&dialog);
    notesEdit->setPlainText(backup.notes);
    notesEdit->setMaximumHeight(80);
    layout->addWidget(notesLabel);
    layout->addWidget(notesEdit);

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) return;

    backup.displayName = nameEdit->text().trimmed();
    if (backup.displayName.isEmpty()) {
        backup.displayName = backup.timestamp.toString("yyyy-MM-dd HH:mm:ss");
    }
    backup.notes = notesEdit->toPlainText().trimmed();

    m_saveManager->updateBackupMetadata(backup);
}

void MainWindow::onBackupUpdated(const QString &gameId, const QString &backupId)
{
    Q_UNUSED(backupId);
    if (gameId == m_currentGameId) {
        loadBackupsForGame(gameId);
    }
}

void MainWindow::onBackUpAll()
{
    if (m_saveManager->isBusy()) {
        QMessageBox::warning(this, "Busy", "A backup operation is already in progress.");
        return;
    }

    QList<GameInfo> games = m_gameDetector->getDetectedGames();
    BulkBackupDialog dialog(games, m_saveManager, this);
    if (dialog.exec() != QDialog::Accepted) return;

    m_bulkBackupQueue = dialog.getSelectedGames();
    if (m_bulkBackupQueue.isEmpty()) return;

    ui->statusbar->showMessage(QString("Backing up %1 games...").arg(m_bulkBackupQueue.size()));
    processNextBulkBackup();
}

void MainWindow::processNextBulkBackup()
{
    if (m_bulkBackupQueue.isEmpty()) {
        ui->statusbar->showMessage("Bulk backup complete", 5000);
        return;
    }

    GameInfo game = m_bulkBackupQueue.takeFirst();

    QMetaObject::Connection *conn = new QMetaObject::Connection;
    *conn = connect(m_saveManager, &SaveManager::operationFinished, this, [this, conn]() {
        disconnect(*conn);
        delete conn;
        processNextBulkBackup();
    });

    QMetaObject::Connection *errConn = new QMetaObject::Connection;
    *errConn = connect(m_saveManager, &SaveManager::error, this, [this, errConn](const QString &) {
        disconnect(*errConn);
        delete errConn;
    });

    int remaining = m_bulkBackupQueue.size() + 1;
    ui->statusbar->showMessage(QString("Backing up %1 (%2 remaining)...")
        .arg(game.name).arg(remaining));

    m_saveManager->createBackupAsync(game);
}

void MainWindow::onSearchTextChanged(const QString &text)
{
    QString filter = text.trimmed();

    for (int i = 0; i < ui->gamesTreeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *category = ui->gamesTreeWidget->topLevelItem(i);
        int visibleChildren = 0;

        for (int j = 0; j < category->childCount(); ++j) {
            QTreeWidgetItem *child = category->child(j);
            QString gameName = child->data(0, GameCardRoles::GameNameRole).toString();
            bool matches = filter.isEmpty() || gameName.contains(filter, Qt::CaseInsensitive);
            child->setHidden(!matches);
            if (matches) visibleChildren++;
        }

        category->setHidden(visibleChildren == 0);
    }
}

void MainWindow::onGameContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = ui->gamesTreeWidget->itemAt(pos);
    if (!item || !item->parent()) {
        return; // No item or it's a category header (no parent)
    }

    QString gameId = item->data(0, GameCardRoles::GameIdRole).toString();
    QString gameName = item->data(0, GameCardRoles::GameNameRole).toString();
    if (gameId.isEmpty()) {
        return;
    }

    GameInfo game = m_gameDetector->getGameById(gameId);

    QMenu menu(this);

    // Switch Save Path submenu (only if alternatives exist)
    QMenu *switchMenu = nullptr;
    if (!game.alternativeSavePaths.isEmpty()) {
        switchMenu = menu.addMenu("Switch Save Path");
        // Show current path (checked)
        QAction *currentAction = switchMenu->addAction(game.detectedSavePath);
        currentAction->setCheckable(true);
        currentAction->setChecked(true);
        currentAction->setEnabled(false);
        switchMenu->addSeparator();
        // Show alternatives
        for (const QString &altPath : game.alternativeSavePaths) {
            switchMenu->addAction(altPath);
        }
    }

    QAction *profilesAction = menu.addAction("Manage Profiles...");
    QAction *hideAction = menu.addAction("Hide Game");

    QAction *selected = menu.exec(ui->gamesTreeWidget->viewport()->mapToGlobal(pos));
    if (!selected) {
        return;
    }

    if (selected == profilesAction) {
        ProfileDialog dialog(m_database, game, this);
        dialog.exec();
    } else if (selected == hideAction) {
        m_database->hideGame(gameId, gameName);
        loadGames();
        ui->statusbar->showMessage(QString("Hidden: %1").arg(gameName), 3000);
    } else if (switchMenu && switchMenu->actions().contains(selected)) {
        // User picked an alternative save path
        QString newPath = selected->text();
        saveSavePathOverride(gameId, newPath);
        loadGames();
        // Re-select the game so the UI updates
        onGameSelected();
        ui->statusbar->showMessage(QString("Switched save path for %1").arg(gameName), 3000);
    }
}

void MainWindow::onHideGame()
{
    GameInfo game = getCurrentGame();
    if (game.id.isEmpty()) {
        return;
    }

    m_database->hideGame(game.id, game.name);
    loadGames();
    ui->statusbar->showMessage(QString("Hidden: %1").arg(game.name), 3000);
}

void MainWindow::onManageHiddenGames()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Hidden Games");
    dialog.resize(400, 300);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLabel *infoLabel = new QLabel("These games are hidden from the detected games list. "
                                   "Select a game and click Unhide to restore it.");
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    QListWidget *listWidget = new QListWidget(&dialog);
    layout->addWidget(listWidget);

    QList<QPair<QString, QString>> hiddenGames = m_database->getHiddenGames();
    for (const auto &[id, name] : hiddenGames) {
        QString displayText = name.isEmpty() ? id : name;
        QListWidgetItem *item = new QListWidgetItem(displayText, listWidget);
        item->setData(Qt::UserRole, id);
    }

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *unhideButton = new QPushButton("Unhide");
    unhideButton->setEnabled(false);
    QPushButton *closeButton = new QPushButton("Close");
    buttonLayout->addStretch();
    buttonLayout->addWidget(unhideButton);
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);

    connect(listWidget, &QListWidget::currentItemChanged, [unhideButton](QListWidgetItem *current) {
        unhideButton->setEnabled(current != nullptr);
    });

    connect(unhideButton, &QPushButton::clicked, [&]() {
        QListWidgetItem *current = listWidget->currentItem();
        if (!current) return;
        QString gameId = current->data(Qt::UserRole).toString();
        m_database->unhideGame(gameId);
        delete listWidget->takeItem(listWidget->row(current));
    });

    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    dialog.exec();

    // Refresh game list in case games were unhidden
    loadGames();
}

void MainWindow::onSettings()
{
    SettingsDialog dialog(m_database, this);
    if (dialog.exec() == QDialog::Accepted) {
        QString backupDir = dialog.backupDirectory();
        if (!backupDir.isEmpty()) {
            m_saveManager->setBackupDirectory(backupDir);
        }
        m_saveManager->setCompressionLevel(dialog.compressionLevel());

        if (m_trayIcon) {
            bool trayEnabled = m_database->getSetting("minimize_to_tray", "0") == "1";
            m_trayIcon->setVisible(trayEnabled);
        }
        updateFileWatcher();

        ui->statusbar->showMessage("Settings saved", 3000);
    }
}

void MainWindow::onAbout()
{
    QMessageBox::about(this,
                      "About Game Rewind",
                      "Game Rewind v1.0\n\n"
                      "A tool for managing game save backups.\n\n"
                      "Licensed under MIT License\n"
                      "Copyright (c) 2026");
}

void MainWindow::onBackupCreated(const QString &gameId, const QString &backupId)
{
    Q_UNUSED(backupId);

    if (gameId == m_currentGameId) {
        loadBackupsForGame(gameId);
    }
    updateGameCard(gameId);  // Update the game card stats
    updateStorageUsage();
}

void MainWindow::onBackupRestored(const QString &gameId, const QString &backupId)
{
    Q_UNUSED(gameId);
    Q_UNUSED(backupId);
}

void MainWindow::onBackupDeleted(const QString &gameId, const QString &backupId)
{
    Q_UNUSED(backupId);

    if (gameId == m_currentGameId) {
        loadBackupsForGame(gameId);
    }
    updateGameCard(gameId);  // Update the game card stats
    updateStorageUsage();
}

void MainWindow::onError(const QString &message)
{
    QMessageBox::critical(this, "Error", message);
}

void MainWindow::onManifestReady()
{
    // Manifest was freshly downloaded or updated - reload games to pick up new entries
    loadGames();
    updateStorageUsage();

    // Show onboarding after first manifest load (so the games grid has full data)
    showOnboardingIfNeeded();
}

void MainWindow::updateStorageUsage()
{
    qint64 totalSize = 0;
    int totalBackups = 0;

    // Calculate total storage used by all backups
    QList<GameInfo> games = m_gameDetector->getDetectedGames();
    for (const GameInfo &game : games) {
        QList<BackupInfo> backups = m_saveManager->getBackupsForGame(game.id);
        totalBackups += backups.size();
        for (const BackupInfo &backup : backups) {
            totalSize += backup.size;
        }
    }

    // Format and display
    QString storageText = QString("Storage: %1 (%2 backups)")
                             .arg(formatFileSize(totalSize))
                             .arg(totalBackups);

    m_storageLabel->setText(storageText);
}

void MainWindow::setOperationInProgress(bool inProgress, const QString &message)
{
    m_progressBar->setVisible(inProgress);

    GameInfo game = getCurrentGame();
    bool hasGame = !game.id.isEmpty() && game.isDetected && !game.detectedSavePath.isEmpty();
    ui->actionCreateBackup->setEnabled(!inProgress && hasGame);
    ui->actionRestoreBackup->setEnabled(!inProgress && ui->backupsListWidget->currentItem());
    ui->actionDeleteBackup->setEnabled(!inProgress && ui->backupsListWidget->currentItem());

    if (inProgress && !message.isEmpty()) {
        ui->statusbar->showMessage(message);
    }
}

void MainWindow::setupTrayIcon()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qDebug() << "System tray not available";
        return;
    }

    m_trayIcon = new QSystemTrayIcon(QIcon::fromTheme("document-save"), this);
    m_trayIcon->setToolTip("Game Rewind");

    QMenu *trayMenu = new QMenu(this);
    QAction *showAction = trayMenu->addAction("Show Game Rewind");
    connect(showAction, &QAction::triggered, this, [this]() {
        show();
        raise();
        activateWindow();
    });
    QAction *backupAllAction = trayMenu->addAction("Back Up All");
    connect(backupAllAction, &QAction::triggered, this, &MainWindow::onBackUpAll);
    trayMenu->addSeparator();
    QAction *quitAction = trayMenu->addAction("Quit");
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    m_trayIcon->setContextMenu(trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &MainWindow::onTrayActivated);

    bool trayEnabled = m_database->getSetting("minimize_to_tray", "0") == "1";
    m_trayIcon->setVisible(trayEnabled);
}

void MainWindow::setupFileWatcher()
{
    m_fileWatcher = new QFileSystemWatcher(this);
    connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged,
            this, &MainWindow::onSaveDirectoryChanged);

    m_autoBackupTimer = new QTimer(this);
    m_autoBackupTimer->setSingleShot(true);
    connect(m_autoBackupTimer, &QTimer::timeout,
            this, &MainWindow::onAutoBackupTimer);

    updateFileWatcher();
}

void MainWindow::updateFileWatcher()
{
    QStringList watched = m_fileWatcher->directories();
    if (!watched.isEmpty()) {
        m_fileWatcher->removePaths(watched);
    }
    m_watchedPathToGameId.clear();

    bool autoBackupEnabled = m_database->getSetting("auto_backup_enabled", "0") == "1";
    if (!autoBackupEnabled) return;

    QList<GameInfo> games = m_gameDetector->getDetectedGames();
    for (const GameInfo &game : games) {
        if (!game.isDetected || game.detectedSavePath.isEmpty()) continue;
        if (QDir(game.detectedSavePath).exists()) {
            m_fileWatcher->addPath(game.detectedSavePath);
            m_watchedPathToGameId.insert(game.detectedSavePath, game.id);
        }
    }

    qDebug() << "File watcher: monitoring" << m_watchedPathToGameId.size() << "save directories";
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        if (isVisible()) {
            hide();
        } else {
            show();
            raise();
            activateWindow();
        }
    }
}

void MainWindow::onSaveDirectoryChanged(const QString &path)
{
    QString gameId = m_watchedPathToGameId.value(path);
    if (gameId.isEmpty()) return;

    m_pendingAutoBackups.insert(gameId);

    int intervalSecs = m_database->getSetting("auto_backup_interval", "30").toInt();
    m_autoBackupTimer->start(intervalSecs * 1000);

    qDebug() << "Save directory changed for" << gameId << "- auto-backup in" << intervalSecs << "s";
}

void MainWindow::onAutoBackupTimer()
{
    if (m_pendingAutoBackups.isEmpty()) return;
    if (m_saveManager->isBusy()) {
        m_autoBackupTimer->start(10000);
        return;
    }

    QString gameId = *m_pendingAutoBackups.begin();
    m_pendingAutoBackups.remove(gameId);

    performAutoBackup(gameId);
}

void MainWindow::performAutoBackup(const QString &gameId)
{
    GameInfo game = m_gameDetector->getGameById(gameId);
    if (game.id.isEmpty() || !game.isDetected) return;

    qDebug() << "Auto-backing up" << game.name;

    m_saveManager->createBackupAsync(game, "Auto-backup");

    QMetaObject::Connection *conn = new QMetaObject::Connection;
    *conn = connect(m_saveManager, &SaveManager::operationFinished, this, [this, conn, game]() {
        disconnect(*conn);
        delete conn;

        if (m_trayIcon && m_trayIcon->isVisible()) {
            m_trayIcon->showMessage("Game Rewind",
                QString("Auto-backup created for %1").arg(game.name),
                QSystemTrayIcon::Information, 3000);
        }

        if (!m_pendingAutoBackups.isEmpty()) {
            QTimer::singleShot(1000, this, &MainWindow::onAutoBackupTimer);
        }
    });
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    bool minimizeToTray = m_database->getSetting("minimize_to_tray", "0") == "1";

    if (minimizeToTray && m_trayIcon && m_trayIcon->isVisible()) {
        hide();
        m_trayIcon->showMessage("Game Rewind",
            "Application minimized to system tray. Right-click the tray icon for options.",
            QSystemTrayIcon::Information, 2000);
        event->ignore();
    } else {
        event->accept();
    }
}

QMap<QString, QString> MainWindow::loadSavePathOverrides() const
{
    QMap<QString, QString> overrides;
    QString json = m_database->getSetting("save_path_overrides");
    if (json.isEmpty())
        return overrides;

    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject())
        return overrides;

    QJsonObject obj = doc.object();
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        overrides.insert(it.key(), it.value().toString());
    }
    return overrides;
}

void MainWindow::saveSavePathOverride(const QString &gameId, const QString &path)
{
    QMap<QString, QString> overrides = loadSavePathOverrides();
    overrides.insert(gameId, path);

    QJsonObject obj;
    for (auto it = overrides.constBegin(); it != overrides.constEnd(); ++it) {
        obj.insert(it.key(), it.value());
    }

    m_database->setSetting("save_path_overrides",
                           QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}
