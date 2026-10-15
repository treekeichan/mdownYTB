#include "downloadcore.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>
#include <algorithm>

namespace DownloadCore {
bool validUrl(const QString &text) {
    const QUrl url(text.trimmed(), QUrl::StrictMode);
    const QString host = url.host().toLower();
    const bool youtube = host == QStringLiteral("youtube.com") || host.endsWith(QStringLiteral(".youtube.com"));
    return url.isValid() && (url.scheme() == QStringLiteral("https") || url.scheme() == QStringLiteral("http"))
        && url.userInfo().isEmpty() && (url.port() == -1 || url.port() == 443 || url.port() == 80)
        && (youtube || host == QStringLiteral("youtu.be")) && !url.path().isEmpty() && url.path() != QStringLiteral("/");
}

QString findTool(const QString &name, const QString &configured) {
    if (!configured.trimmed().isEmpty()) {
        const QFileInfo file(configured);
        return file.isFile() && file.isExecutable() ? file.absoluteFilePath() : QString();
    }
    QString executable = name;
#ifdef Q_OS_WIN
    executable += QStringLiteral(".exe");
#endif
    const QString app = QCoreApplication::applicationDirPath();
    const QStringList folders = {app + QStringLiteral("/tools"), app, app + QStringLiteral("/../tools"), app + QStringLiteral("/../../tools")};
    for (const auto &folder : folders) {
        const QFileInfo file(QDir(folder).filePath(executable));
        if (file.isFile() && file.isExecutable()) return file.absoluteFilePath();
    }
    return QStandardPaths::findExecutable(executable);
}

QStringList baseArguments(const QString &proxy, const QString &node, const QString &cookiesFile,
                          const QString &browser, const QString &networkMode) {
    QStringList args = {QStringLiteral("--ignore-config"), QStringLiteral("--no-playlist"),
        QStringLiteral("--encoding"), QStringLiteral("utf-8"), QStringLiteral("--socket-timeout"),
        QStringLiteral("20"), QStringLiteral("--retries"), QStringLiteral("3")};
    if (networkMode == QStringLiteral("direct")) args << QStringLiteral("--proxy") << QString();
    else if (networkMode == QStringLiteral("custom") || (networkMode == QStringLiteral("system") && !proxy.trimmed().isEmpty()))
        args << QStringLiteral("--proxy") << proxy.trimmed();
    if (!node.isEmpty()) args << QStringLiteral("--js-runtimes") << QStringLiteral("node:") + node;
    if (!cookiesFile.isEmpty()) args << QStringLiteral("--cookies") << cookiesFile;
    else if (!browser.isEmpty()) args << QStringLiteral("--cookies-from-browser") << browser;
    return args;
}

QString validateCookiesFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return QStringLiteral("Cookies 文件不存在或无法读取，请重新选择。");
    if (file.size() > 8 * 1024 * 1024) return QStringLiteral("Cookies 文件超过 8 MB，请仅导出需要的网站会话。");
    QByteArray header = file.readLine().trimmed();
    if (header.startsWith("\xEF\xBB\xBF")) header.remove(0, 3);
    if (header != "# Netscape HTTP Cookie File" && header != "# HTTP Cookie File")
        return QStringLiteral("Cookies 文件格式不正确，请选择 Netscape 格式的 cookies.txt。");
    bool found = false;
    while (!file.atEnd()) {
        QByteArray line = file.readLine();
        while (line.endsWith('\n') || line.endsWith('\r')) line.chop(1);
        if (line.isEmpty() || (line.startsWith('#') && !line.startsWith("#HttpOnly_"))) continue;
        const auto fields = line.split('\t');
        bool expiryOk = false;
        if (fields.size() == 7) fields[4].toLongLong(&expiryOk);
        if (fields.size() != 7 || !expiryOk) return QStringLiteral("Cookies 文件中存在无效记录，请重新导出 Netscape 格式文件。");
        found = true;
    }
    return found ? QString() : QStringLiteral("Cookies 文件没有会话记录，请重新导出。");
}
bool needsVerification(const QString &error) {
    const auto lower = error.toLower();
    return lower.contains(QStringLiteral("sign in")) || lower.contains(QStringLiteral("not a bot"))
        || lower.contains(QStringLiteral("confirm you"));
}

QStringList downloadArguments(const Options &o) {
    auto args = baseArguments(o.proxy, o.node, o.cookiesFile, o.browser, o.networkMode);
    args << QStringLiteral("--newline") << QStringLiteral("--progress") << QStringLiteral("--no-colors")
         << QStringLiteral("--progress-delta") << QStringLiteral("0.3")
         << QStringLiteral("--progress-template") << QStringLiteral("download:__PROGRESS__%(progress)j")
         << QStringLiteral("--print") << QStringLiteral("after_move:__FILE__%(filepath)j")
         << QStringLiteral("--no-simulate") << QStringLiteral("--continue") << QStringLiteral("--no-overwrites")
         << QStringLiteral("--windows-filenames") << QStringLiteral("--trim-filenames") << QStringLiteral("160")
         << QStringLiteral("--paths") << o.directory << QStringLiteral("--output");
    const QString variant = o.audioOnly ? QStringLiteral("audio")
        : o.container + (o.height > 0 ? QStringLiteral("-%1p").arg(o.height) : QStringLiteral("-best"));
    args << QStringLiteral("%(title).100s [%(id)s] [%1].%(ext)s").arg(variant);
    if (!o.ffmpeg.isEmpty()) args << QStringLiteral("--ffmpeg-location") << QFileInfo(o.ffmpeg).absolutePath();
    if (o.audioOnly) {
        args << QStringLiteral("-f") << QStringLiteral("ba[ext=m4a]/ba") << QStringLiteral("-x")
             << QStringLiteral("--audio-format") << QStringLiteral("m4a");
    } else {
        const QString height = o.height > 0 ? QStringLiteral("[height<=%1]").arg(o.height) : QString();
        const QString selector = o.container == QStringLiteral("mp4")
            ? QStringLiteral("bv%1[ext=mp4]+ba[ext=m4a]/b%1[ext=mp4]").arg(height)
            : QStringLiteral("bv%1+ba/b%1").arg(height);
        args << QStringLiteral("-f") << selector << QStringLiteral("--merge-output-format") << o.container;
    }
    if (!o.subtitles.isEmpty()) args << QStringLiteral("--write-subs") << QStringLiteral("--sub-langs")
        << o.subtitles << QStringLiteral("--sub-format") << QStringLiteral("srt/vtt/best");
    args << QStringLiteral("--") << o.url;
    return args;
}

QList<int> availableHeights(const QJsonObject &metadata, bool mp4) {
    QSet<int> heights;
    for (const auto &value : metadata.value(QStringLiteral("formats")).toArray()) {
        const auto f = value.toObject();
        const int h = f.value(QStringLiteral("height")).toInt();
        const auto codec = f.value(QStringLiteral("vcodec")).toString();
        if (h > 0 && !codec.isEmpty() && codec != QStringLiteral("none")
            && !f.value(QStringLiteral("has_drm")).toBool()
            && (!mp4 || f.value(QStringLiteral("ext")).toString() == QStringLiteral("mp4"))) heights.insert(h);
    }
    auto result = heights.values();
    std::sort(result.begin(), result.end(), std::greater<int>());
    return result;
}
QString readableBytes(double bytes) {
    if (bytes <= 0) return QStringLiteral("—");
    const QStringList units = {QStringLiteral("B"), QStringLiteral("KB"), QStringLiteral("MB"), QStringLiteral("GB")};
    int i = 0;
    while (bytes >= 1024 && i < units.size() - 1) { bytes /= 1024; ++i; }
    return QStringLiteral("%1 %2").arg(bytes, 0, 'f', i == 0 ? 0 : 1).arg(units[i]);
}
QString safeLog(QString text) {
    text.replace(QRegularExpression(QStringLiteral("(?:https?|socks5h?)://[^\\s]+")), QStringLiteral("[链接已隐藏]"));
    text.replace(QRegularExpression(QStringLiteral("(?im)^(.*(?:cookie|authorization)(?:\\s+header)?\\s*[:=]).*$")), QStringLiteral("\\1 [已隐藏]"));
    return text.right(16000);
}
QString failureHint(const QString &error) {
    const QString lower = error.toLower();
    if (needsVerification(error))
        return QStringLiteral("需要平台验证：请在浏览器打开该视频完成验证，再在设置中选择 Cookies 文件或浏览器会话，并核对网络出口。\n") + error;
    if (lower.contains(QStringLiteral("decrypt")) || lower.contains(QStringLiteral("cookie database")))
        return QStringLiteral("无法读取浏览器会话：浏览器可能正在占用数据库或系统无法解密，请改用 Cookies 文件。\n") + error;
    if (lower.contains(QStringLiteral("cookie")))
        return QStringLiteral("Cookies 不可用或已过期，请重新导出并在设置中选择文件。\n") + error;
    if (lower.contains(QStringLiteral("requested format")))
        return QStringLiteral("所选格式不可用，请重新解析，或尝试 MKV / 最佳可用。\n") + error;
    if (lower.contains(QStringLiteral("timed out")) || lower.contains(QStringLiteral("unable to download")))
        return QStringLiteral("请检查网络和设置中的代理地址，再重试。\n") + error;
    return error;
}
}
