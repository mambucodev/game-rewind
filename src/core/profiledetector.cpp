#include "profiledetector.h"
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QMap>
#include <algorithm>

QList<ProfileDetector::SuggestedProfile> ProfileDetector::detectProfiles(const QString &saveDir)
{
    QList<SuggestedProfile> results;

    results = detectNumberedFiles(saveDir);
    if (results.size() >= 2)
        return results;

    results = detectNumberedDirs(saveDir);
    if (results.size() >= 2)
        return results;

    results = detectCommonPatterns(saveDir);
    if (results.size() >= 2)
        return results;

    return {};
}

QList<ProfileDetector::SuggestedProfile> ProfileDetector::detectNumberedFiles(const QString &saveDir)
{
    QDir dir(saveDir);
    if (!dir.exists())
        return {};

    QStringList files = dir.entryList(QDir::Files, QDir::Name);

    // Pattern: prefix + number + optional suffix (e.g., "user1.dat", "save_03.sav")
    QRegularExpression re(R"(^(.+?)(\d+)(\..+)?$)");

    // Group files by their pattern key (prefix + suffix with number replaced)
    // patternKey -> { number -> filename }
    QMap<QString, QMap<int, QString>> groups;

    for (const QString &file : files) {
        QRegularExpressionMatch match = re.match(file);
        if (!match.hasMatch())
            continue;

        QString prefix = match.captured(1);
        int number = match.captured(2).toInt();
        QString suffix = match.captured(3); // may be empty

        QString patternKey = prefix + "{N}" + suffix;
        groups[patternKey][number] = file;
    }

    // Find the largest pattern group with >= 2 files
    QString bestPattern;
    int bestSize = 0;
    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
        if (it.value().size() >= 2 && it.value().size() > bestSize) {
            bestPattern = it.key();
            bestSize = it.value().size();
        }
    }

    if (bestPattern.isEmpty())
        return {};

    const QMap<int, QString> &primary = groups[bestPattern];

    // Check if other pattern groups share the same numbers (correlated files)
    QList<QString> correlatedPatterns;
    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
        if (it.key() == bestPattern)
            continue;
        if (it.value().size() < 2)
            continue;

        // Check if this group's numbers are a subset of the primary group's numbers
        bool correlated = true;
        for (auto numIt = it.value().constBegin(); numIt != it.value().constEnd(); ++numIt) {
            if (!primary.contains(numIt.key())) {
                correlated = false;
                break;
            }
        }
        if (correlated)
            correlatedPatterns.append(it.key());
    }

    // Build profiles from the primary group, attaching correlated files
    QList<SuggestedProfile> results;
    int slotIndex = 1;
    for (auto it = primary.constBegin(); it != primary.constEnd(); ++it) {
        SuggestedProfile sp;
        sp.name = QString("Slot %1").arg(slotIndex++);
        sp.files.append(it.value());

        for (const QString &corrPattern : correlatedPatterns) {
            if (groups[corrPattern].contains(it.key())) {
                sp.files.append(groups[corrPattern][it.key()]);
            }
        }

        results.append(sp);
        if (results.size() >= 20)
            break;
    }

    return results;
}

QList<ProfileDetector::SuggestedProfile> ProfileDetector::detectNumberedDirs(const QString &saveDir)
{
    QDir dir(saveDir);
    if (!dir.exists())
        return {};

    QStringList dirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    QRegularExpression re(R"(^(slot|save|profile|savegame|data)[-_]?(\d+)$)",
                          QRegularExpression::CaseInsensitiveOption);

    // number -> directory name
    QMap<int, QString> matches;
    for (const QString &d : dirs) {
        QRegularExpressionMatch match = re.match(d);
        if (match.hasMatch()) {
            int number = match.captured(2).toInt();
            matches[number] = d;
        }
    }

    if (matches.size() < 2)
        return {};

    QList<SuggestedProfile> results;
    int slotIndex = 1;
    for (auto it = matches.constBegin(); it != matches.constEnd(); ++it) {
        SuggestedProfile sp;
        sp.name = QString("Slot %1").arg(slotIndex++);
        sp.files.append(it.value());
        results.append(sp);
        if (results.size() >= 20)
            break;
    }

    return results;
}

QList<ProfileDetector::SuggestedProfile> ProfileDetector::detectCommonPatterns(const QString &saveDir)
{
    QDir dir(saveDir);
    if (!dir.exists())
        return {};

    QStringList files = dir.entryList(QDir::Files, QDir::Name);

    // Look for files sharing an extension where basenames follow a slot-like pattern
    // e.g., SaveSlot1.sav, SaveSlot2.sav or profile_1.dat, profile_2.dat
    QRegularExpression re(R"(^(.+?)[-_]?(\d+)\.(\w+)$)");

    // extension -> { number -> filename }
    QMap<QString, QMap<int, QString>> byExtension;

    for (const QString &file : files) {
        QRegularExpressionMatch match = re.match(file);
        if (!match.hasMatch())
            continue;

        QString ext = match.captured(3).toLower();
        int number = match.captured(2).toInt();
        byExtension[ext][number] = file;
    }

    // Find the extension group with the most matches (>= 2)
    QString bestExt;
    int bestSize = 0;
    for (auto it = byExtension.constBegin(); it != byExtension.constEnd(); ++it) {
        if (it.value().size() >= 2 && it.value().size() > bestSize) {
            bestExt = it.key();
            bestSize = it.value().size();
        }
    }

    if (bestExt.isEmpty())
        return {};

    QList<SuggestedProfile> results;
    int slotIndex = 1;
    const QMap<int, QString> &group = byExtension[bestExt];
    for (auto it = group.constBegin(); it != group.constEnd(); ++it) {
        SuggestedProfile sp;
        sp.name = QString("Slot %1").arg(slotIndex++);
        sp.files.append(it.value());
        results.append(sp);
        if (results.size() >= 20)
            break;
    }

    return results;
}
