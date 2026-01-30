#include "addgamedialog.h"
#include "steam/steamutils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QStackedWidget>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>

AddGameDialog::AddGameDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Add Game");
    setMinimumWidth(500);

    setupUI();

    // Detect Steam games
    QString steamPath = SteamUtils::findSteamPath();
    QStringList libraryFolders = SteamUtils::getLibraryFolders(steamPath);
    m_steamGames = SteamUtils::scanInstalledGames(libraryFolders);
    for (const SteamAppInfo &game : m_steamGames) {
        m_steamGameCombo->addItem(game.name, game.appId);
    }

    if (m_steamGames.isEmpty()) {
        m_steamGameCombo->addItem("No Steam games detected", "");
        m_steamGameCombo->setEnabled(false);
    }
}

void AddGameDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Platform selection
    QLabel *platformLabel = new QLabel("Platform:", this);
    m_platformCombo = new QComboBox(this);
    m_platformCombo->addItem("Steam", "steam");
    m_platformCombo->addItem("Native Linux", "native");
    m_platformCombo->addItem("Custom", "custom");

    mainLayout->addWidget(platformLabel);
    mainLayout->addWidget(m_platformCombo);

    // Stacked widget for different platform pages
    m_stackedWidget = new QStackedWidget(this);

    // Steam page
    QWidget *steamPage = new QWidget();
    QFormLayout *steamLayout = new QFormLayout(steamPage);

    m_steamGameCombo = new QComboBox();
    m_steamSavePathEdit = new QLineEdit();
    m_steamSavePathEdit->setPlaceholderText("Select save folder...");
    m_steamBrowseButton = new QPushButton("Browse...");

    QHBoxLayout *steamPathLayout = new QHBoxLayout();
    steamPathLayout->addWidget(m_steamSavePathEdit);
    steamPathLayout->addWidget(m_steamBrowseButton);

    steamLayout->addRow("Game:", m_steamGameCombo);
    steamLayout->addRow("Save Folder:", steamPathLayout);

    m_stackedWidget->addWidget(steamPage);

    // Native/Custom page
    QWidget *customPage = new QWidget();
    QFormLayout *customLayout = new QFormLayout(customPage);

    m_customNameEdit = new QLineEdit();
    m_customNameEdit->setPlaceholderText("Enter game name...");
    m_customSavePathEdit = new QLineEdit();
    m_customSavePathEdit->setPlaceholderText("Select save folder...");
    m_customBrowseButton = new QPushButton("Browse...");

    QHBoxLayout *customPathLayout = new QHBoxLayout();
    customPathLayout->addWidget(m_customSavePathEdit);
    customPathLayout->addWidget(m_customBrowseButton);

    customLayout->addRow("Game Name:", m_customNameEdit);
    customLayout->addRow("Save Folder:", customPathLayout);

    m_stackedWidget->addWidget(customPage);

    mainLayout->addWidget(m_stackedWidget);

    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    mainLayout->addWidget(buttonBox);

    // Connections
    connect(m_platformCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AddGameDialog::onPlatformChanged);
    connect(m_steamBrowseButton, &QPushButton::clicked,
            this, &AddGameDialog::onBrowseSavePath);
    connect(m_customBrowseButton, &QPushButton::clicked,
            this, &AddGameDialog::onBrowseSavePath);
    connect(m_steamGameCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AddGameDialog::onSteamGameSelected);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &AddGameDialog::onValidate);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void AddGameDialog::onPlatformChanged(int index)
{
    QString platform = m_platformCombo->itemData(index).toString();

    if (platform == "steam") {
        m_stackedWidget->setCurrentIndex(0);
    } else {
        m_stackedWidget->setCurrentIndex(1);
    }
}

void AddGameDialog::onBrowseSavePath()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Save Folder",
                                                     QDir::homePath(),
                                                     QFileDialog::ShowDirsOnly);

    if (!dir.isEmpty()) {
        if (m_stackedWidget->currentIndex() == 0) {
            m_steamSavePathEdit->setText(dir);
        } else {
            m_customSavePathEdit->setText(dir);
        }
    }
}

void AddGameDialog::onSteamGameSelected(int index)
{
    // Could auto-suggest save paths here based on game name
    Q_UNUSED(index);
}

void AddGameDialog::onValidate()
{
    QString name = getGameName();
    QString savePath = getSavePath();

    if (name.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Please enter a game name.");
        return;
    }

    if (savePath.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Please select a save folder.");
        return;
    }

    if (!QDir(savePath).exists()) {
        QMessageBox::warning(this, "Validation Error",
                            "The selected save folder does not exist.");
        return;
    }

    accept();
}

QString AddGameDialog::getGameName() const
{
    if (m_stackedWidget->currentIndex() == 0) {
        return m_steamGameCombo->currentText();
    } else {
        return m_customNameEdit->text().trimmed();
    }
}

QString AddGameDialog::getPlatform() const
{
    return m_platformCombo->currentData().toString();
}

QString AddGameDialog::getSteamAppId() const
{
    if (m_stackedWidget->currentIndex() == 0) {
        return m_steamGameCombo->currentData().toString();
    }
    return QString();
}

QString AddGameDialog::getSavePath() const
{
    if (m_stackedWidget->currentIndex() == 0) {
        return m_steamSavePathEdit->text().trimmed();
    } else {
        return m_customSavePathEdit->text().trimmed();
    }
}
