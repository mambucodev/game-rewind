#ifndef BACKUPDIALOG_H
#define BACKUPDIALOG_H

#include <QDialog>
#include <QString>

class QLineEdit;
class QTextEdit;

class BackupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BackupDialog(QWidget *parent = nullptr);

    QString getBackupName() const;
    QString getBackupNotes() const;

private:
    QLineEdit *m_nameEdit;
    QTextEdit *m_notesEdit;
};

#endif // BACKUPDIALOG_H
