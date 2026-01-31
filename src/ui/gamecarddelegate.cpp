#include "gamecarddelegate.h"
#include <QApplication>
#include <QDateTime>
#include <QPainterPath>
#include <QStyle>

GameCardDelegate::GameCardDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void GameCardDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // Get data from model
    bool isCategory = index.data(GameCardRoles::IsCategoryRole).toBool();

    // Draw selection/hover background
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, option.palette.highlight());
    } else if (option.state & QStyle::State_MouseOver && !isCategory) {
        QColor hoverColor = option.palette.highlight().color();
        hoverColor.setAlpha(50);
        painter->fillRect(option.rect, hoverColor);
    }

    if (isCategory) {
        // Draw category header (simple text with icon)
        QRect rect = option.rect.adjusted(12, 0, 0, 0);

        QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
        if (!icon.isNull()) {
            QRect iconRect(rect.left(), rect.top() + 8, 24, 24);
            icon.paint(painter, iconRect);
            rect.adjust(32, 0, 0, 0);
        }

        QFont font = option.font;
        font.setBold(true);
        font.setPointSize(font.pointSize() + 1);
        painter->setFont(font);
        painter->setPen(option.palette.windowText().color());
        painter->drawText(rect, Qt::AlignVCenter, index.data(Qt::DisplayRole).toString());

    } else {
        // Draw game card
        QString gameName = index.data(GameCardRoles::GameNameRole).toString();
        QPixmap capsule = index.data(GameCardRoles::GameIconRole).value<QPixmap>();
        int backupCount = index.data(GameCardRoles::BackupCountRole).toInt();
        qint64 totalSize = index.data(GameCardRoles::TotalSizeRole).toLongLong();
        QString platform = index.data(GameCardRoles::PlatformRole).toString();

        // Card dimensions - use vertical capsule (portrait)
        int padding = 12;
        int capsuleWidth = 54;   // Portrait width (2:3 ratio)
        int capsuleHeight = 80;  // Portrait height
        int spacing = 12;

        QRect cardRect = option.rect.adjusted(padding, 4, -padding, -4);

        // Draw capsule with rounded corners (centered vertically)
        QRect capsuleRect(cardRect.left(), cardRect.top() + (cardRect.height() - capsuleHeight) / 2,
                          capsuleWidth, capsuleHeight);

        if (!capsule.isNull()) {
            // Scale keeping aspect ratio, don't crop
            QPixmap scaled = capsule.scaled(capsuleWidth, capsuleHeight,
                                           Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation);

            // Center the scaled image in capsuleRect
            int xOffset = (capsuleWidth - scaled.width()) / 2;
            int yOffset = (capsuleHeight - scaled.height()) / 2;

            QRect drawRect(capsuleRect.left() + xOffset,
                          capsuleRect.top() + yOffset,
                          scaled.width(),
                          scaled.height());

            QPixmap rounded = roundedPixmap(scaled, 6);
            painter->drawPixmap(drawRect, rounded);
        } else {
            // Draw placeholder
            painter->setPen(option.palette.mid().color());
            painter->setBrush(option.palette.button());
            painter->drawRoundedRect(capsuleRect, 6, 6);
        }

        // Text area
        QRect textRect = cardRect.adjusted(capsuleWidth + spacing, 0, 0, 0);

        // Draw game title (semi-bold)
        QFont titleFont = option.font;
        titleFont.setWeight(QFont::DemiBold);
        titleFont.setPointSize(titleFont.pointSize() + 1);
        painter->setFont(titleFont);
        painter->setPen(option.palette.windowText().color());

        QRect titleRect = textRect.adjusted(0, 8, 0, 0);
        painter->drawText(titleRect, Qt::AlignTop | Qt::AlignLeft, gameName);

        // Calculate title height for metadata positioning
        QFontMetrics titleFm(titleFont);
        int titleHeight = titleFm.height();

        // Draw metadata (backup count and size)
        QFont metaFont = option.font;
        metaFont.setPointSize(metaFont.pointSize() - 1);
        painter->setFont(metaFont);

        QColor metaColor = option.palette.windowText().color();
        metaColor.setAlphaF(0.6);
        painter->setPen(metaColor);

        QRect metaRect = textRect.adjusted(0, titleHeight + 12, 0, 0);

        // Format size
        QString sizeStr;
        if (totalSize >= 1024 * 1024 * 1024) {
            sizeStr = QString::number(totalSize / (1024.0 * 1024 * 1024), 'f', 1) + " GB";
        } else if (totalSize >= 1024 * 1024) {
            sizeStr = QString::number(totalSize / (1024.0 * 1024), 'f', 1) + " MB";
        } else if (totalSize >= 1024) {
            sizeStr = QString::number(totalSize / 1024.0, 'f', 1) + " KB";
        } else if (totalSize > 0) {
            sizeStr = QString::number(totalSize) + " bytes";
        } else {
            sizeStr = "—";
        }

        QString backupText = QString("%1 backup%2").arg(backupCount).arg(backupCount != 1 ? "s" : "");
        QString metaText = QString("%1  •  %2").arg(backupText).arg(sizeStr);

        painter->drawText(metaRect, Qt::AlignTop | Qt::AlignLeft, metaText);

        // Draw platform badge (optional)
        if (!platform.isEmpty() && platform != "custom") {
            QRect platformRect = textRect.adjusted(0, titleHeight + 32, 0, 0);
            QFont platformFont = option.font;
            platformFont.setPointSize(platformFont.pointSize() - 2);
            painter->setFont(platformFont);

            QColor badgeColor = option.palette.mid().color();
            badgeColor.setAlphaF(0.3);

            QString platformText = platform.toUpper();
            QFontMetrics platformFm(platformFont);
            int badgeWidth = platformFm.horizontalAdvance(platformText) + 12;
            int badgeHeight = platformFm.height() + 4;

            QRect badgeRect(platformRect.left(), platformRect.top(), badgeWidth, badgeHeight);
            painter->setBrush(badgeColor);
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(badgeRect, 3, 3);

            painter->setPen(option.palette.windowText().color());
            painter->drawText(badgeRect, Qt::AlignCenter, platformText);
        }

        // Draw last backup timestamp (right-aligned, same row as platform badge)
        QDateTime lastBackup = index.data(GameCardRoles::LastBackupRole).toDateTime();
        QFont lastBackupFont = option.font;
        lastBackupFont.setPointSize(lastBackupFont.pointSize() - 2);
        painter->setFont(lastBackupFont);

        QColor lastBackupColor = option.palette.windowText().color();
        lastBackupColor.setAlphaF(0.45);
        painter->setPen(lastBackupColor);

        QString lastBackupText;
        if (!lastBackup.isValid()) {
            lastBackupText = "Never backed up";
        } else {
            qint64 secsAgo = lastBackup.secsTo(QDateTime::currentDateTime());
            if (secsAgo < 60) {
                lastBackupText = "Last: just now";
            } else if (secsAgo < 3600) {
                lastBackupText = QString("Last: %1 min ago").arg(secsAgo / 60);
            } else if (secsAgo < 86400) {
                lastBackupText = QString("Last: %1h ago").arg(secsAgo / 3600);
            } else if (secsAgo < 604800) {
                int days = secsAgo / 86400;
                lastBackupText = QString("Last: %1 day%2 ago").arg(days).arg(days > 1 ? "s" : "");
            } else {
                lastBackupText = "Last: " + lastBackup.toString("MMM d");
            }
        }

        QFontMetrics lastFm(lastBackupFont);
        int lastTextWidth = lastFm.horizontalAdvance(lastBackupText);
        QRect lastBackupRect(textRect.right() - lastTextWidth,
                             textRect.top() + titleHeight + 32,
                             lastTextWidth, lastFm.height());
        painter->drawText(lastBackupRect, Qt::AlignRight | Qt::AlignTop, lastBackupText);
    }

    painter->restore();
}

QSize GameCardDelegate::sizeHint(const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    Q_UNUSED(option);

    bool isCategory = index.data(GameCardRoles::IsCategoryRole).toBool();

    if (isCategory) {
        return QSize(0, 40); // Categories are shorter
    } else {
        return QSize(0, 96); // Game cards taller for vertical capsules
    }
}

QPixmap GameCardDelegate::roundedPixmap(const QPixmap &source, int radius) const
{
    if (source.isNull()) {
        return QPixmap();
    }

    QPixmap rounded(source.size());
    rounded.fill(Qt::transparent);

    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QPainterPath path;
    path.addRoundedRect(0, 0, source.width(), source.height(), radius, radius);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, source);

    return rounded;
}
