#ifndef ADDGAMEDIALOG_H
#define ADDGAMEDIALOG_H

#include <QDialog>
#include <QString>
#include <QMap>
#include "steam/steamutils.h"

class QComboBox;
class QLineEdit;
class QPushButton;
class QStackedWidget;

class AddGameDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddGameDialog(QWidget *parent = nullptr);

    QString getGameName() const;
    QString getPlatform() const;
    QString getSteamAppId() const;
    QString getSavePath() const;

private slots:
    void onPlatformChanged(int index);
    void onBrowseSavePath();
    void onSteamGameSelected(int index);
    void onValidate();

private:
    void setupUI();

    QComboBox *m_platformCombo;
    QStackedWidget *m_stackedWidget;

    // Steam page widgets
    QComboBox *m_steamGameCombo;
    QLineEdit *m_steamSavePathEdit;
    QPushButton *m_steamBrowseButton;

    // Native/Custom page widgets
    QLineEdit *m_customNameEdit;
    QLineEdit *m_customSavePathEdit;
    QPushButton *m_customBrowseButton;

    QList<SteamAppInfo> m_steamGames;
};

#endif // ADDGAMEDIALOG_H
