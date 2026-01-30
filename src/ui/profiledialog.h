#ifndef PROFILEDIALOG_H
#define PROFILEDIALOG_H

#include <QDialog>
#include <QList>
#include "core/gameinfo.h"

class QTableWidget;
class QPushButton;
class Database;

class ProfileDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProfileDialog(Database *database, const GameInfo &game, QWidget *parent = nullptr);

signals:
    void profilesChanged();

private slots:
    void onAddProfile();
    void onEditProfile();
    void onDeleteProfile();
    void onAutoDetect();

private:
    void loadProfiles();
    bool showProfileEditor(SaveProfile &profile);

    Database *m_database;
    GameInfo m_game;
    QTableWidget *m_tableWidget;
    QPushButton *m_editButton;
    QPushButton *m_deleteButton;
    QList<SaveProfile> m_profiles;
};

#endif // PROFILEDIALOG_H
