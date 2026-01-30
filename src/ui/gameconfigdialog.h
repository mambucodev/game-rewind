#ifndef GAMECONFIGDIALOG_H
#define GAMECONFIGDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include "core/gameinfo.h"

class Database;

class GameConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit GameConfigDialog(Database *database, QWidget *parent = nullptr);

signals:
    void configsChanged();

private slots:
    void onAddGame();
    void onEditGame();
    void onDeleteGame();
    void onTableDoubleClicked(int row, int column);

private:
    void loadGames();
    GameInfo showGameEditor(const GameInfo &game = GameInfo());

    Database *m_database;
    QTableWidget *m_tableWidget;
    QPushButton *m_editButton;
    QPushButton *m_deleteButton;
    QList<GameInfo> m_games;
};

#endif // GAMECONFIGDIALOG_H
