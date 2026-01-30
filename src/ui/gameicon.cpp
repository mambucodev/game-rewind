#include "gameicon.h"
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QFileInfo>

QIcon GameIconProvider::getIconForGame(const GameInfo &game)
{
    QIcon icon;

    // Try Steam library cache first
    if (game.platform == "steam" && !game.steamAppId.isEmpty()) {
        icon = loadSteamIcon(game.steamAppId, game.name);
        if (!icon.isNull()) {
            return icon;
        }
    }

    // Try .desktop files
    icon = loadFromDesktopFile(game.name);
    if (!icon.isNull()) {
        return icon;
    }

    // Fall back to platform icon
    return getPlatformIcon(game.platform);
}

QPixmap GameIconProvider::getHighResCapsule(const GameInfo &game)
{
    // For Steam games, load full-resolution capsule
    if (game.platform == "steam" && !game.steamAppId.isEmpty()) {
        QPixmap capsule = loadSteamCapsule(game.steamAppId);
        if (!capsule.isNull()) {
            return capsule;
        }
    }

    // Fall back to icon converted to pixmap
    QIcon icon = getIconForGame(game);
    return icon.pixmap(64, 64);
}

QIcon GameIconProvider::getPlatformIcon(const QString &platform)
{
    if (platform == "steam") {
        return QIcon::fromTheme("steam", QIcon::fromTheme("applications-games"));
    } else if (platform == "native") {
        return QIcon::fromTheme("applications-games");
    } else {
        return QIcon::fromTheme("application-x-executable");
    }
}

QIcon GameIconProvider::loadSteamIcon(const QString &steamAppId, const QString &gameName)
{
    Q_UNUSED(gameName);

    QStringList steamPaths = {
        QDir::homePath() + "/.steam/steam",
        QDir::homePath() + "/.local/share/Steam"
    };

    for (const QString &steamPath : steamPaths) {
        QString appCacheDir = steamPath + "/appcache/librarycache/" + steamAppId + "/";

        if (!QDir(appCacheDir).exists()) {
            continue;
        }

        // Priority order: prefer vertical/portrait images for card display
        QStringList iconFormats = {
            "library_600x900.jpg",      // Portrait poster (best for vertical cards)
            "library_capsule.jpg",      // Square capsule
            "icon.jpg",                 // Direct icon if it exists
            "header.jpg",               // Horizontal header
            "logo.png"                  // Horizontal logo (last resort)
        };

        // First, try direct files in the app directory
        for (const QString &format : iconFormats) {
            QString iconPath = appCacheDir + format;
            if (QFile::exists(iconPath)) {
                QPixmap pixmap(iconPath);
                if (!pixmap.isNull()) {
                    pixmap = pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    return QIcon(pixmap);
                }
            }
        }

        // Then search in subdirectories (Steam sometimes stores icons in hash-named subdirs)
        QDir dir(appCacheDir);
        QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

        for (const QString &subdir : subdirs) {
            for (const QString &format : iconFormats) {
                QString iconPath = appCacheDir + subdir + "/" + format;
                if (QFile::exists(iconPath)) {
                    QPixmap pixmap(iconPath);
                    if (!pixmap.isNull()) {
                        pixmap = pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        return QIcon(pixmap);
                    }
                }
            }
        }
    }

    return QIcon();
}

QIcon GameIconProvider::loadFromDesktopFile(const QString &gameName)
{
    QStringList desktopDirs = {
        QDir::homePath() + "/.local/share/applications",
        "/usr/share/applications",
        "/usr/local/share/applications"
    };

    QString searchName = gameName.toLower().replace(" ", "");

    for (const QString &desktopDir : desktopDirs) {
        QDir dir(desktopDir);
        QStringList desktopFiles = dir.entryList(QStringList() << "*.desktop", QDir::Files);

        for (const QString &desktopFile : desktopFiles) {
            if (desktopFile.toLower().contains(searchName)) {
                QString filePath = dir.absoluteFilePath(desktopFile);
                QFile file(filePath);

                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    while (!file.atEnd()) {
                        QString line = file.readLine().trimmed();
                        if (line.startsWith("Icon=")) {
                            QString iconName = line.mid(5);
                            file.close();

                            // Try as theme icon first
                            QIcon icon = QIcon::fromTheme(iconName);
                            if (!icon.isNull()) {
                                return icon;
                            }

                            // Try as direct path
                            if (QFile::exists(iconName)) {
                                return QIcon(iconName);
                            }
                        }
                    }
                    file.close();
                }
            }
        }
    }

    return QIcon();
}

QPixmap GameIconProvider::loadSteamCapsule(const QString &steamAppId)
{
    QStringList steamPaths = {
        QDir::homePath() + "/.steam/steam",
        QDir::homePath() + "/.local/share/Steam"
    };

    for (const QString &steamPath : steamPaths) {
        QString appCacheDir = steamPath + "/appcache/librarycache/" + steamAppId + "/";

        if (!QDir(appCacheDir).exists()) {
            continue;
        }

        // Priority order for high-res capsules
        QStringList capsuleFormats = {
            "library_600x900.jpg",      // High-res portrait (600x900)
            "library_capsule.jpg",      // Square capsule
            "header.jpg"                // Horizontal header
        };

        // Try direct files
        for (const QString &format : capsuleFormats) {
            QString imagePath = appCacheDir + format;
            if (QFile::exists(imagePath)) {
                QPixmap pixmap(imagePath);
                if (!pixmap.isNull()) {
                    return pixmap; // Return full resolution
                }
            }
        }

        // Try subdirectories
        QDir dir(appCacheDir);
        QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

        for (const QString &subdir : subdirs) {
            for (const QString &format : capsuleFormats) {
                QString imagePath = appCacheDir + subdir + "/" + format;
                if (QFile::exists(imagePath)) {
                    QPixmap pixmap(imagePath);
                    if (!pixmap.isNull()) {
                        return pixmap; // Return full resolution
                    }
                }
            }
        }
    }

    return QPixmap();
}
