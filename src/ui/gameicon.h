#ifndef GAMEICON_H
#define GAMEICON_H

#include <QIcon>
#include <QString>
#include <QPixmap>
#include "core/gameinfo.h"

class GameIconProvider {
public:
    static QIcon getIconForGame(const GameInfo &game);
    static QIcon getPlatformIcon(const QString &platform);
    static QPixmap getHighResCapsule(const GameInfo &game);

private:
    static QIcon loadSteamIcon(const QString &steamAppId, const QString &gameName);
    static QPixmap loadSteamCapsule(const QString &steamAppId);
    static QIcon loadFromDesktopFile(const QString &gameName);
};

#endif // GAMEICON_H
