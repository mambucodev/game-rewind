#ifndef BULKBACKUPDIALOG_H
#define BULKBACKUPDIALOG_H

#include <QDialog>
#include <QList>
#include "core/gameinfo.h"

class QTreeWidget;
class QLabel;
class SaveManager;

class BulkBackupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BulkBackupDialog(const QList<GameInfo> &games,
                               SaveManager *saveManager,
                               QWidget *parent = nullptr);

    QList<GameInfo> getSelectedGames() const;

private slots:
    void onSelectAll();
    void onSelectStale();
    void onDeselectAll();

private:
    void populateList();

    QTreeWidget *m_treeWidget;
    QLabel *m_summaryLabel;
    QList<GameInfo> m_games;
    SaveManager *m_saveManager;

    static const int STALE_DAYS = 7;
};

#endif // BULKBACKUPDIALOG_H
