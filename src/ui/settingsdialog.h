#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QLineEdit;
class QComboBox;
class QCheckBox;
class QSpinBox;
class Database;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(Database *database, QWidget *parent = nullptr);

    QString backupDirectory() const;
    int compressionLevel() const;
    bool minimizeToTray() const;
    bool autoBackupEnabled() const;
    int autoBackupIntervalSeconds() const;

signals:
    void onboardingResetRequested();

private slots:
    void onBrowseBackupDir();
    void onResetOnboarding();
    void onAccept();

private:
    void loadSettings();
    void saveSettings();

    Database *m_database;
    bool m_onboardingReset = false;
    QLineEdit *m_backupDirEdit;
    QComboBox *m_compressionCombo;
    QCheckBox *m_minimizeToTrayCheck;
    QCheckBox *m_autoBackupCheck;
    QSpinBox  *m_autoBackupIntervalSpin;
};

#endif // SETTINGSDIALOG_H
