#ifndef GAMECARDDELEGATE_H
#define GAMECARDDELEGATE_H

#include <QStyledItemDelegate>
#include <QPainter>
#include <QPixmap>

class GameCardDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit GameCardDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

private:
    QPixmap roundedPixmap(const QPixmap &source, int radius) const;
};

// Custom roles for storing game data
namespace GameCardRoles {
    const int GameIdRole = Qt::UserRole + 1;
    const int GameNameRole = Qt::UserRole + 2;
    const int GameIconRole = Qt::UserRole + 3;
    const int BackupCountRole = Qt::UserRole + 4;
    const int TotalSizeRole = Qt::UserRole + 5;
    const int SavePathRole = Qt::UserRole + 6;
    const int PlatformRole = Qt::UserRole + 7;
    const int IsCategoryRole = Qt::UserRole + 8;
}

#endif // GAMECARDDELEGATE_H
