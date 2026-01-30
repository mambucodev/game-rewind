#include "backupdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QDialogButtonBox>

BackupDialog::BackupDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Create Backup");
    setMinimumWidth(400);

    QVBoxLayout *layout = new QVBoxLayout(this);

    // Name field
    QLabel *nameLabel = new QLabel("Backup Name (optional):", this);
    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText("Leave empty for timestamp");

    layout->addWidget(nameLabel);
    layout->addWidget(m_nameEdit);

    // Notes field
    QLabel *notesLabel = new QLabel("Notes (optional):", this);
    m_notesEdit = new QTextEdit(this);
    m_notesEdit->setPlaceholderText("e.g., \"Before final boss\", \"100% completion\", etc.");
    m_notesEdit->setMaximumHeight(80);

    layout->addWidget(notesLabel);
    layout->addWidget(m_notesEdit);

    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    layout->addWidget(buttonBox);

    // Focus on name field
    m_nameEdit->setFocus();
}

QString BackupDialog::getBackupName() const
{
    return m_nameEdit->text().trimmed();
}

QString BackupDialog::getBackupNotes() const
{
    return m_notesEdit->toPlainText().trimmed();
}
