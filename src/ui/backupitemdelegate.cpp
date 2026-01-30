#include "backupitemdelegate.h"
#include <QApplication>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QPalette>

BackupItemDelegate::BackupItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void BackupItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                               const QModelIndex &index) const
{
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    painter->save();

    // Draw background and selection
    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, opt.widget);

    // Get the text
    QString text = index.data(Qt::DisplayRole).toString();
    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();

    // Calculate rectangles
    QRect iconRect = opt.rect;
    iconRect.setWidth(opt.decorationSize.width() + 8);
    iconRect = iconRect.adjusted(4, 4, 0, -4);

    QRect textRect = opt.rect;
    textRect.setLeft(iconRect.right() + 4);
    textRect = textRect.adjusted(0, 4, -4, -4);

    // Draw icon
    if (!icon.isNull()) {
        QPixmap pixmap = icon.pixmap(opt.decorationSize);
        QRect pixmapRect = QRect(iconRect.topLeft(), opt.decorationSize);
        pixmapRect.moveCenter(iconRect.center());
        painter->drawPixmap(pixmapRect, pixmap);
    }

    // Draw HTML text
    QTextDocument doc;
    doc.setHtml(text);
    doc.setTextWidth(textRect.width());
    doc.setDefaultFont(opt.font);

    // Use palette-aware colors
    QPalette palette = opt.palette;
    QString styledText = text;

    // Replace 'gray' color with palette text color at reduced opacity
    QColor textColor = palette.color(QPalette::Text);
    QString grayColor = QString("rgba(%1, %2, %3, 0.6)")
                           .arg(textColor.red())
                           .arg(textColor.green())
                           .arg(textColor.blue());
    styledText.replace("color: gray", QString("color: %1").arg(grayColor));

    doc.setHtml(styledText);

    painter->translate(textRect.topLeft());
    QAbstractTextDocumentLayout::PaintContext ctx;

    // Set text color from palette
    ctx.palette = palette;

    doc.documentLayout()->draw(painter, ctx);

    painter->restore();
}

QSize BackupItemDelegate::sizeHint(const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(0, 52); // Fixed height for backup items
}
