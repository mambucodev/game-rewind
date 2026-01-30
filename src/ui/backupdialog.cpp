#include "backupdialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QDialogButtonBox>

BackupDialog::BackupDialog(const QList<SaveProfile> &profiles, QWidget *parent)
    : QDialog(parent)
    , m_profileCombo(nullptr)
    , m_profiles(profiles)
{
    setWindowTitle("Create Backup");
    setMinimumWidth(400);

    QVBoxLayout *layout = new QVBoxLayout(this);

    // Profile selection (only shown when profiles exist)
    if (!m_profiles.isEmpty()) {
        QLabel *profileLabel = new QLabel("Profile:", this);
        m_profileCombo = new QComboBox(this);
        m_profileCombo->addItem("All Files", -1);
        for (const SaveProfile &p : m_profiles) {
            QString label = QString("%1 (%2)").arg(p.name, p.files.join(", "));
            m_profileCombo->addItem(label, p.id);
        }
        layout->addWidget(profileLabel);
        layout->addWidget(m_profileCombo);
    }

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

SaveProfile BackupDialog::getSelectedProfile() const
{
    if (!m_profileCombo || m_profileCombo->currentData().toInt() == -1) {
        return SaveProfile();
    }

    int profileId = m_profileCombo->currentData().toInt();
    for (const SaveProfile &p : m_profiles) {
        if (p.id == profileId) {
            return p;
        }
    }
    return SaveProfile();
}
