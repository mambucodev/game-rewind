#include "gameconfigdialog.h"
#include "core/database.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QInputDialog>
#include <QDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QLabel>
#include <QDateTime>

GameConfigDialog::GameConfigDialog(Database *database, QWidget *parent)
    : QDialog(parent)
    , m_database(database)
{
    setWindowTitle("Manage Game Configurations");
    resize(800, 500);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QLabel *infoLabel = new QLabel(
        "Manage custom game configurations.\n\n"
        "These are games you have manually added that are not auto-detected from Steam.");
    infoLabel->setWordWrap(true);
    mainLayout->addWidget(infoLabel);

    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(4);
    m_tableWidget->setHorizontalHeaderLabels(QStringList() << "Name" << "Platform" << "Steam App ID" << "Save Paths");
    m_tableWidget->horizontalHeader()->setStretchLastSection(true);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mainLayout->addWidget(m_tableWidget);

    QHBoxLayout *buttonLayout = new QHBoxLayout();

    QPushButton *addButton = new QPushButton("Add Game", this);
    m_editButton = new QPushButton("Edit Game", this);
    m_deleteButton = new QPushButton("Delete Game", this);
    QPushButton *closeButton = new QPushButton("Close", this);

    m_editButton->setEnabled(false);
    m_deleteButton->setEnabled(false);

    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(m_editButton);
    buttonLayout->addWidget(m_deleteButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);

    mainLayout->addLayout(buttonLayout);

    connect(addButton, &QPushButton::clicked, this, &GameConfigDialog::onAddGame);
    connect(m_editButton, &QPushButton::clicked, this, &GameConfigDialog::onEditGame);
    connect(m_deleteButton, &QPushButton::clicked, this, &GameConfigDialog::onDeleteGame);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_tableWidget, &QTableWidget::itemSelectionChanged, this, [this]() {
        bool hasSelection = !m_tableWidget->selectedItems().isEmpty();
        m_editButton->setEnabled(hasSelection);
        m_deleteButton->setEnabled(hasSelection);
    });
    connect(m_tableWidget, &QTableWidget::cellDoubleClicked, this, &GameConfigDialog::onTableDoubleClicked);

    loadGames();
}

void GameConfigDialog::loadGames()
{
    m_games.clear();
    m_tableWidget->setRowCount(0);

    m_games = m_database->getAllCustomGames();

    for (const GameInfo &game : m_games) {
        int row = m_tableWidget->rowCount();
        m_tableWidget->insertRow(row);
        m_tableWidget->setItem(row, 0, new QTableWidgetItem(game.name));
        m_tableWidget->setItem(row, 1, new QTableWidgetItem(game.platform));
        m_tableWidget->setItem(row, 2, new QTableWidgetItem(game.steamAppId));
        m_tableWidget->setItem(row, 3, new QTableWidgetItem(game.savePaths.join(", ")));
    }
}

GameInfo GameConfigDialog::showGameEditor(const GameInfo &game)
{
    QDialog dialog(this);
    dialog.setWindowTitle(game.id.isEmpty() ? "Add Game" : "Edit Game");
    dialog.resize(500, 400);

    QFormLayout *formLayout = new QFormLayout();

    QLineEdit *nameEdit = new QLineEdit(&dialog);
    nameEdit->setText(game.name);
    formLayout->addRow("Game Name:", nameEdit);

    QLineEdit *idEdit = new QLineEdit(&dialog);
    idEdit->setText(game.id);
    if (!game.id.isEmpty()) {
        idEdit->setEnabled(false);
    }
    formLayout->addRow("ID:", idEdit);

    QComboBox *platformCombo = new QComboBox(&dialog);
    platformCombo->addItems(QStringList() << "steam" << "native" << "custom");
    platformCombo->setCurrentText(game.platform.isEmpty() ? "custom" : game.platform);
    formLayout->addRow("Platform:", platformCombo);

    QLineEdit *steamAppIdEdit = new QLineEdit(&dialog);
    steamAppIdEdit->setText(game.steamAppId);
    formLayout->addRow("Steam App ID:", steamAppIdEdit);

    QLabel *pathsLabel = new QLabel("Save Paths:", &dialog);
    formLayout->addRow(pathsLabel);

    QListWidget *pathsList = new QListWidget(&dialog);
    pathsList->addItems(game.savePaths);
    formLayout->addRow(pathsList);

    QHBoxLayout *pathButtonsLayout = new QHBoxLayout();
    QPushButton *addPathButton = new QPushButton("Add Path", &dialog);
    QPushButton *removePathButton = new QPushButton("Remove Path", &dialog);
    pathButtonsLayout->addWidget(addPathButton);
    pathButtonsLayout->addWidget(removePathButton);
    pathButtonsLayout->addStretch();
    formLayout->addRow(pathButtonsLayout);

    connect(addPathButton, &QPushButton::clicked, [&pathsList]() {
        bool ok;
        QString path = QInputDialog::getText(nullptr, "Add Save Path",
                                            "Path (use ~ for home, $STEAM for Steam dir):",
                                            QLineEdit::Normal, "", &ok);
        if (ok && !path.isEmpty()) {
            pathsList->addItem(path);
        }
    });

    connect(removePathButton, &QPushButton::clicked, [&pathsList]() {
        qDeleteAll(pathsList->selectedItems());
    });

    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    formLayout->addRow(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    dialog.setLayout(formLayout);

    GameInfo result;
    if (dialog.exec() == QDialog::Accepted) {
        result.name = nameEdit->text();
        result.id = idEdit->text().isEmpty() ?
                    "custom_" + QString::number(QDateTime::currentSecsSinceEpoch()) :
                    idEdit->text();
        result.platform = platformCombo->currentText();
        result.steamAppId = steamAppIdEdit->text();
        result.source = "database";

        for (int i = 0; i < pathsList->count(); ++i) {
            result.savePaths << pathsList->item(i)->text();
        }
    }

    return result;
}

void GameConfigDialog::onAddGame()
{
    GameInfo newGame = showGameEditor();

    if (newGame.name.isEmpty() || newGame.savePaths.isEmpty()) {
        return;
    }

    if (m_database->customGameExists(newGame.id)) {
        QMessageBox::warning(this, "Duplicate ID",
            "A game with this ID already exists. Please use a different ID.");
        return;
    }

    if (m_database->addCustomGame(newGame)) {
        loadGames();
        emit configsChanged();
    }
}

void GameConfigDialog::onEditGame()
{
    int row = m_tableWidget->currentRow();
    if (row < 0 || row >= m_games.size()) {
        return;
    }

    GameInfo edited = showGameEditor(m_games[row]);

    if (edited.name.isEmpty() || edited.savePaths.isEmpty()) {
        return;
    }

    if (m_database->updateCustomGame(edited)) {
        loadGames();
        emit configsChanged();
    }
}

void GameConfigDialog::onDeleteGame()
{
    int row = m_tableWidget->currentRow();
    if (row < 0 || row >= m_games.size()) {
        return;
    }

    GameInfo game = m_games[row];

    QMessageBox::StandardButton reply = QMessageBox::question(this,
        "Delete Game Configuration",
        QString("Are you sure you want to delete '%1' from the configuration?").arg(game.name),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) {
        return;
    }

    if (m_database->removeCustomGame(game.id)) {
        loadGames();
        emit configsChanged();
    }
}

void GameConfigDialog::onTableDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    if (row >= 0 && row < m_games.size()) {
        onEditGame();
    }
}
