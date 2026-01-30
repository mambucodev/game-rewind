#ifndef BACKUPITEMDELEGATE_H
#define BACKUPITEMDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QTextDocument>

class BackupItemDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit BackupItemDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};

#endif // BACKUPITEMDELEGATE_H
