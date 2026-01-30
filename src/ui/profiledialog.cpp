#include "profiledialog.h"
#include "core/database.h"
#include "core/profiledetector.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLineEdit>
#include <QListWidget>
#include <QFileDialog>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QDir>

ProfileDialog::ProfileDialog(Database *database, const GameInfo &game, QWidget *parent)
    : QDialog(parent)
    , m_database(database)
    , m_game(game)
{
    setWindowTitle(QString("Manage Profiles - %1").arg(game.name));
    resize(600, 400);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QLabel *infoLabel = new QLabel(
        "Save profiles let you back up individual save slots instead of the entire "
        "save directory. Define profiles below, or click Auto-Detect to scan for save slots.");
    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);

    QLabel *pathLabel = new QLabel(QString("Save directory: %1").arg(game.detectedSavePath));
    pathLabel->setWordWrap(true);
    QPalette mutedPalette = pathLabel->palette();
    QColor mutedColor = mutedPalette.color(QPalette::WindowText);
    mutedColor.setAlphaF(0.6f);
    mutedPalette.setColor(QPalette::WindowText, mutedColor);
    pathLabel->setPalette(mutedPalette);
    mainLayout->addWidget(pathLabel);

    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(2);
    m_tableWidget->setHorizontalHeaderLabels({"Name", "Files"});
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->verticalHeader()->setVisible(false);
    mainLayout->addWidget(m_tableWidget);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *autoDetectButton = new QPushButton("Auto-Detect");
    QPushButton *addButton = new QPushButton("Add Profile");
    m_editButton = new QPushButton("Edit");
    m_deleteButton = new QPushButton("Delete");
    QPushButton *closeButton = new QPushButton("Close");

    m_editButton->setEnabled(false);
    m_deleteButton->setEnabled(false);

    buttonLayout->addWidget(autoDetectButton);
    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(m_editButton);
    buttonLayout->addWidget(m_deleteButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    mainLayout->addLayout(buttonLayout);

    connect(autoDetectButton, &QPushButton::clicked, this, &ProfileDialog::onAutoDetect);
    connect(addButton, &QPushButton::clicked, this, &ProfileDialog::onAddProfile);
    connect(m_editButton, &QPushButton::clicked, this, &ProfileDialog::onEditProfile);
    connect(m_deleteButton, &QPushButton::clicked, this, &ProfileDialog::onDeleteProfile);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_tableWidget, &QTableWidget::itemSelectionChanged, this, [this]() {
        bool has = !m_tableWidget->selectedItems().isEmpty();
        m_editButton->setEnabled(has);
        m_deleteButton->setEnabled(has);
    });
    connect(m_tableWidget, &QTableWidget::cellDoubleClicked, this, [this](int, int) {
        onEditProfile();
    });

    loadProfiles();
}

void ProfileDialog::loadProfiles()
{
    m_profiles = m_database->getProfilesForGame(m_game.id);

    m_tableWidget->setRowCount(m_profiles.size());
    for (int i = 0; i < m_profiles.size(); ++i) {
        m_tableWidget->setItem(i, 0, new QTableWidgetItem(m_profiles[i].name));
        m_tableWidget->setItem(i, 1, new QTableWidgetItem(m_profiles[i].files.join(", ")));
    }

    m_editButton->setEnabled(false);
    m_deleteButton->setEnabled(false);
}

void ProfileDialog::onAutoDetect()
{
    if (m_game.detectedSavePath.isEmpty()) {
        QMessageBox::warning(this, "Auto-Detect", "No save directory detected for this game.");
        return;
    }

    QList<ProfileDetector::SuggestedProfile> suggestions =
        ProfileDetector::detectProfiles(m_game.detectedSavePath);

    if (suggestions.isEmpty()) {
        QMessageBox::information(this, "Auto-Detect",
            "No save slot patterns were detected in the save directory.");
        return;
    }

    QString message = QString("Detected %1 possible save slots:\n\n").arg(suggestions.size());
    for (const auto &s : suggestions) {
        message += QString("  %1: %2\n").arg(s.name, s.files.join(", "));
    }
    message += "\nAdd these as profiles?";

    if (QMessageBox::question(this, "Auto-Detect Results", message,
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    int added = 0;
    for (const auto &s : suggestions) {
        if (m_database->profileExists(m_game.id, s.name))
            continue;

        SaveProfile profile;
        profile.gameId = m_game.id;
        profile.name = s.name;
        profile.files = s.files;
        if (m_database->addProfile(profile) >= 0)
            added++;
    }

    loadProfiles();
    emit profilesChanged();

    if (added > 0) {
        QMessageBox::information(this, "Auto-Detect",
            QString("Added %1 profile(s).").arg(added));
    } else {
        QMessageBox::information(this, "Auto-Detect",
            "All detected profiles already exist.");
    }
}

void ProfileDialog::onAddProfile()
{
    SaveProfile profile;
    profile.gameId = m_game.id;

    if (showProfileEditor(profile)) {
        if (m_database->profileExists(m_game.id, profile.name)) {
            QMessageBox::warning(this, "Add Profile",
                QString("A profile named \"%1\" already exists.").arg(profile.name));
            return;
        }
        m_database->addProfile(profile);
        loadProfiles();
        emit profilesChanged();
    }
}

void ProfileDialog::onEditProfile()
{
    int row = m_tableWidget->currentRow();
    if (row < 0 || row >= m_profiles.size())
        return;

    SaveProfile profile = m_profiles[row];
    QString originalName = profile.name;

    if (showProfileEditor(profile)) {
        if (profile.name != originalName && m_database->profileExists(m_game.id, profile.name)) {
            QMessageBox::warning(this, "Edit Profile",
                QString("A profile named \"%1\" already exists.").arg(profile.name));
            return;
        }
        m_database->updateProfile(profile);
        loadProfiles();
        emit profilesChanged();
    }
}

void ProfileDialog::onDeleteProfile()
{
    int row = m_tableWidget->currentRow();
    if (row < 0 || row >= m_profiles.size())
        return;

    const SaveProfile &profile = m_profiles[row];

    if (QMessageBox::question(this, "Delete Profile",
            QString("Delete the profile \"%1\"?").arg(profile.name),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    m_database->removeProfile(profile.id);
    loadProfiles();
    emit profilesChanged();
}

bool ProfileDialog::showProfileEditor(SaveProfile &profile)
{
    QDialog editor(this);
    editor.setWindowTitle(profile.id == -1 ? "Add Profile" : "Edit Profile");
    editor.setMinimumWidth(450);

    QVBoxLayout *layout = new QVBoxLayout(&editor);

    // Name field
    QLabel *nameLabel = new QLabel("Profile Name:", &editor);
    QLineEdit *nameEdit = new QLineEdit(profile.name, &editor);
    nameEdit->setPlaceholderText("e.g., Slot 1");
    layout->addWidget(nameLabel);
    layout->addWidget(nameEdit);

    // Files list
    QLabel *filesLabel = new QLabel("Files (relative to save directory):", &editor);
    layout->addWidget(filesLabel);

    QListWidget *filesList = new QListWidget(&editor);
    for (const QString &f : profile.files) {
        filesList->addItem(f);
    }
    layout->addWidget(filesList);

    // File buttons
    QHBoxLayout *fileButtons = new QHBoxLayout();
    QPushButton *addFileBtn = new QPushButton("Add File");
    QPushButton *addDirBtn = new QPushButton("Add Directory");
    QPushButton *removeBtn = new QPushButton("Remove");
    removeBtn->setEnabled(false);
    fileButtons->addWidget(addFileBtn);
    fileButtons->addWidget(addDirBtn);
    fileButtons->addWidget(removeBtn);
    fileButtons->addStretch();
    layout->addLayout(fileButtons);

    QObject::connect(filesList, &QListWidget::itemSelectionChanged, [&]() {
        removeBtn->setEnabled(filesList->currentItem() != nullptr);
    });

    QObject::connect(addFileBtn, &QPushButton::clicked, [&]() {
        QString file = QFileDialog::getOpenFileName(&editor, "Select File",
            m_game.detectedSavePath);
        if (file.isEmpty())
            return;

        QDir baseDir(m_game.detectedSavePath);
        QString relative = baseDir.relativeFilePath(file);
        if (relative.startsWith("..")) {
            QMessageBox::warning(&editor, "Invalid File",
                "The file must be inside the save directory.");
            return;
        }
        filesList->addItem(relative);
    });

    QObject::connect(addDirBtn, &QPushButton::clicked, [&]() {
        QString dir = QFileDialog::getExistingDirectory(&editor, "Select Directory",
            m_game.detectedSavePath);
        if (dir.isEmpty())
            return;

        QDir baseDir(m_game.detectedSavePath);
        QString relative = baseDir.relativeFilePath(dir);
        if (relative.startsWith("..")) {
            QMessageBox::warning(&editor, "Invalid Directory",
                "The directory must be inside the save directory.");
            return;
        }
        filesList->addItem(relative);
    });

    QObject::connect(removeBtn, &QPushButton::clicked, [&]() {
        delete filesList->takeItem(filesList->currentRow());
    });

    // Dialog buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &editor);
    connect(buttonBox, &QDialogButtonBox::accepted, &editor, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &editor, &QDialog::reject);
    layout->addWidget(buttonBox);

    nameEdit->setFocus();

    if (editor.exec() != QDialog::Accepted)
        return false;

    QString name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, "Profile", "Profile name cannot be empty.");
        return false;
    }

    QStringList files;
    for (int i = 0; i < filesList->count(); ++i) {
        files.append(filesList->item(i)->text());
    }

    if (files.isEmpty()) {
        QMessageBox::warning(this, "Profile", "Profile must contain at least one file.");
        return false;
    }

    profile.name = name;
    profile.files = files;
    return true;
}
