#ifndef BACKUPDIALOG_H
#define BACKUPDIALOG_H

#include <QDialog>
#include <QString>
#include <QList>
#include "core/gameinfo.h"

class QLineEdit;
class QTextEdit;
class QComboBox;

class BackupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BackupDialog(const QList<SaveProfile> &profiles = QList<SaveProfile>(),
                          QWidget *parent = nullptr);

    QString getBackupName() const;
    QString getBackupNotes() const;
    SaveProfile getSelectedProfile() const;

private:
    QLineEdit *m_nameEdit;
    QTextEdit *m_notesEdit;
    QComboBox *m_profileCombo;
    QList<SaveProfile> m_profiles;
};

#endif // BACKUPDIALOG_H
