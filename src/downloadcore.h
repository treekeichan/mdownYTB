#pragma once
#include <QJsonObject>
#include <QStringList>

namespace DownloadCore {
struct Options {
    QString url, directory, container = QStringLiteral("mp4"), proxy, ffmpeg, node;
    int height = 0;
    bool audioOnly = false;
    QString subtitles;
    QString cookiesFile, browser, networkMode = QStringLiteral("system");
};
bool validUrl(const QString &text);
QString findTool(const QString &name, const QString &configured = {});
QStringList baseArguments(const QString &proxy, const QString &node, const QString &cookiesFile = {},
                          const QString &browser = {}, const QString &networkMode = QStringLiteral("system"));
QString validateCookiesFile(const QString &path);
bool needsVerification(const QString &error);
QStringList downloadArguments(const Options &options);
QList<int> availableHeights(const QJsonObject &metadata, bool mp4);
QString readableBytes(double bytes);
QString safeLog(QString text);
QString failureHint(const QString &error);
}
