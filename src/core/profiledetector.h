#ifndef PROFILEDETECTOR_H
#define PROFILEDETECTOR_H

#include <QString>
#include <QList>
#include <QStringList>

class ProfileDetector {
public:
    struct SuggestedProfile {
        QString name;
        QStringList files;
    };

    static QList<SuggestedProfile> detectProfiles(const QString &saveDir);

private:
    static QList<SuggestedProfile> detectNumberedFiles(const QString &saveDir);
    static QList<SuggestedProfile> detectNumberedDirs(const QString &saveDir);
    static QList<SuggestedProfile> detectCommonPatterns(const QString &saveDir);
};

#endif // PROFILEDETECTOR_H
