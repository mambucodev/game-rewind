#include "bulkbackupdialog.h"
#include "core/savemanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QDateTime>

BulkBackupDialog::BulkBackupDialog(const QList<GameInfo> &games,
                                     SaveManager *saveManager,
                                     QWidget *parent)
    : QDialog(parent)
    , m_games(games)
    , m_saveManager(saveManager)
{
    setWindowTitle("Back Up All Games");
    setMinimumSize(550, 450);

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *infoLabel = new QLabel(
        "Select the games you want to back up. Games with stale or no backups are pre-selected.", this);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    QHBoxLayout *selBtnLayout = new QHBoxLayout();
    QPushButton *selectAllBtn = new QPushButton("Select All", this);
    QPushButton *selectStaleBtn = new QPushButton("Select Stale Only", this);
    QPushButton *deselectAllBtn = new QPushButton("Deselect All", this);
    connect(selectAllBtn, &QPushButton::clicked, this, &BulkBackupDialog::onSelectAll);
    connect(selectStaleBtn, &QPushButton::clicked, this, &BulkBackupDialog::onSelectStale);
    connect(deselectAllBtn, &QPushButton::clicked, this, &BulkBackupDialog::onDeselectAll);
    selBtnLayout->addWidget(selectAllBtn);
    selBtnLayout->addWidget(selectStaleBtn);
    selBtnLayout->addWidget(deselectAllBtn);
    selBtnLayout->addStretch();
    layout->addLayout(selBtnLayout);

    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setHeaderLabels({"Game", "Last Backup", "Status"});
    m_treeWidget->header()->setStretchLastSection(false);
    m_treeWidget->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_treeWidget->setRootIsDecorated(false);
    layout->addWidget(m_treeWidget);

    m_summaryLabel = new QLabel(this);
    layout->addWidget(m_summaryLabel);

    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("Back Up Selected");
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    populateList();
    onSelectStale();
}

void BulkBackupDialog::populateList()
{
    m_treeWidget->clear();
    QDateTime now = QDateTime::currentDateTime();

    for (const GameInfo &game : m_games) {
        if (!game.isDetected || game.detectedSavePath.isEmpty()) continue;

        QList<BackupInfo> backups = m_saveManager->getBackupsForGame(game.id);

        QTreeWidgetItem *item = new QTreeWidgetItem(m_treeWidget);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, Qt::Unchecked);
        item->setText(0, game.name);
        item->setData(0, Qt::UserRole, game.id);

        if (backups.isEmpty()) {
            item->setText(1, "Never");
            item->setText(2, "No backups");
        } else {
            QDateTime last = backups.first().timestamp;
            int daysAgo = last.daysTo(now);
            if (daysAgo == 0) {
                item->setText(1, "Today");
            } else if (daysAgo == 1) {
                item->setText(1, "Yesterday");
            } else {
                item->setText(1, QString("%1 days ago").arg(daysAgo));
            }
            item->setText(2, daysAgo > STALE_DAYS ? "Stale" : "Recent");
        }
    }
}

void BulkBackupDialog::onSelectAll()
{
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        m_treeWidget->topLevelItem(i)->setCheckState(0, Qt::Checked);
    }
}

void BulkBackupDialog::onSelectStale()
{
    QDateTime now = QDateTime::currentDateTime();
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_treeWidget->topLevelItem(i);
        QString gameId = item->data(0, Qt::UserRole).toString();
        QList<BackupInfo> backups = m_saveManager->getBackupsForGame(gameId);

        bool stale = backups.isEmpty() ||
                     backups.first().timestamp.daysTo(now) > STALE_DAYS;
        item->setCheckState(0, stale ? Qt::Checked : Qt::Unchecked);
    }
}

void BulkBackupDialog::onDeselectAll()
{
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        m_treeWidget->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
    }
}

QList<GameInfo> BulkBackupDialog::getSelectedGames() const
{
    QList<GameInfo> selected;
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = m_treeWidget->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked) {
            QString gameId = item->data(0, Qt::UserRole).toString();
            for (const GameInfo &game : m_games) {
                if (game.id == gameId) {
                    selected.append(game);
                    break;
                }
            }
        }
    }
    return selected;
}
