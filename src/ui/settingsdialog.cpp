#include "settingsdialog.h"
#include "core/database.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QGroupBox>
#include <QMessageBox>
#include <QStandardPaths>

SettingsDialog::SettingsDialog(Database *database, QWidget *parent)
    : QDialog(parent)
    , m_database(database)
{
    setWindowTitle("Settings");
    setMinimumWidth(500);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // --- Backup group ---
    QGroupBox *backupGroup = new QGroupBox("Backup", this);
    QFormLayout *backupForm = new QFormLayout(backupGroup);

    QHBoxLayout *dirLayout = new QHBoxLayout();
    m_backupDirEdit = new QLineEdit(this);
    QPushButton *browseButton = new QPushButton("Browse...", this);
    connect(browseButton, &QPushButton::clicked, this, &SettingsDialog::onBrowseBackupDir);
    dirLayout->addWidget(m_backupDirEdit);
    dirLayout->addWidget(browseButton);
    backupForm->addRow("Backup Directory:", dirLayout);

    m_compressionCombo = new QComboBox(this);
    m_compressionCombo->addItem("Fast (gzip -1)", 1);
    m_compressionCombo->addItem("Default (gzip -6)", 6);
    m_compressionCombo->addItem("Best (gzip -9)", 9);
    backupForm->addRow("Compression Level:", m_compressionCombo);

    mainLayout->addWidget(backupGroup);

    // --- System Tray group ---
    QGroupBox *trayGroup = new QGroupBox("System Tray", this);
    QVBoxLayout *trayLayout = new QVBoxLayout(trayGroup);

    m_minimizeToTrayCheck = new QCheckBox("Minimize to system tray instead of closing", this);
    trayLayout->addWidget(m_minimizeToTrayCheck);

    m_autoBackupCheck = new QCheckBox("Auto-backup when save files change", this);
    trayLayout->addWidget(m_autoBackupCheck);

    QHBoxLayout *intervalLayout = new QHBoxLayout();
    QLabel *intervalLabel = new QLabel("Debounce interval:", this);
    m_autoBackupIntervalSpin = new QSpinBox(this);
    m_autoBackupIntervalSpin->setRange(10, 300);
    m_autoBackupIntervalSpin->setSuffix(" seconds");
    m_autoBackupIntervalSpin->setValue(30);
    intervalLayout->addWidget(intervalLabel);
    intervalLayout->addWidget(m_autoBackupIntervalSpin);
    intervalLayout->addStretch();
    trayLayout->addLayout(intervalLayout);

    mainLayout->addWidget(trayGroup);

    // --- Misc group ---
    QGroupBox *miscGroup = new QGroupBox("Miscellaneous", this);
    QVBoxLayout *miscLayout = new QVBoxLayout(miscGroup);

    QPushButton *resetOnboardingBtn = new QPushButton("Reset Onboarding Wizard", this);
    connect(resetOnboardingBtn, &QPushButton::clicked, this, &SettingsDialog::onResetOnboarding);
    miscLayout->addWidget(resetOnboardingBtn);

    mainLayout->addWidget(miscGroup);

    // --- Buttons ---
    mainLayout->addStretch();
    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);

    loadSettings();
}

void SettingsDialog::loadSettings()
{
    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                         + "/game-rewind";
    m_backupDirEdit->setText(m_database->getSetting("backup_directory", defaultDir));

    int compression = m_database->getSetting("compression_level", "6").toInt();
    int comboIdx = m_compressionCombo->findData(compression);
    if (comboIdx >= 0) m_compressionCombo->setCurrentIndex(comboIdx);

    m_minimizeToTrayCheck->setChecked(
        m_database->getSetting("minimize_to_tray", "0") == "1");
    m_autoBackupCheck->setChecked(
        m_database->getSetting("auto_backup_enabled", "0") == "1");
    m_autoBackupIntervalSpin->setValue(
        m_database->getSetting("auto_backup_interval", "30").toInt());
}

void SettingsDialog::saveSettings()
{
    m_database->setSetting("backup_directory", m_backupDirEdit->text().trimmed());
    m_database->setSetting("compression_level",
        QString::number(m_compressionCombo->currentData().toInt()));
    m_database->setSetting("minimize_to_tray",
        m_minimizeToTrayCheck->isChecked() ? "1" : "0");
    m_database->setSetting("auto_backup_enabled",
        m_autoBackupCheck->isChecked() ? "1" : "0");
    m_database->setSetting("auto_backup_interval",
        QString::number(m_autoBackupIntervalSpin->value()));
}

void SettingsDialog::onBrowseBackupDir()
{
    QString dir = QFileDialog::getExistingDirectory(this,
        "Select Backup Directory", m_backupDirEdit->text());
    if (!dir.isEmpty()) {
        m_backupDirEdit->setText(dir);
    }
}

void SettingsDialog::onResetOnboarding()
{
    m_database->setSetting("onboarding_completed", "0");
    QMessageBox::information(this, "Onboarding Reset",
        "The onboarding wizard will appear after you close Settings.");
    m_onboardingReset = true;
}

void SettingsDialog::onAccept()
{
    saveSettings();
    if (m_onboardingReset) {
        emit onboardingResetRequested();
    }
    accept();
}

QString SettingsDialog::backupDirectory() const
{
    return m_backupDirEdit->text().trimmed();
}

int SettingsDialog::compressionLevel() const
{
    return m_compressionCombo->currentData().toInt();
}

bool SettingsDialog::minimizeToTray() const
{
    return m_minimizeToTrayCheck->isChecked();
}

bool SettingsDialog::autoBackupEnabled() const
{
    return m_autoBackupCheck->isChecked();
}

int SettingsDialog::autoBackupIntervalSeconds() const
{
    return m_autoBackupIntervalSpin->value();
}
