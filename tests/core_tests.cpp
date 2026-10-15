#include "downloadcore.h"
#include "engineprocess.h"
#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QFile>

class CoreTests : public QObject {
    Q_OBJECT
    QString fake() { return QCoreApplication::applicationDirPath() + QStringLiteral("/fake_engine.exe"); }
private slots:
    void sessionAndNetworkArguments() {
        const QString cookies = QStringLiteral("D:/我的会话/with spaces/cookies.txt");
        auto args = DownloadCore::baseArguments({}, {}, cookies, {}, QStringLiteral("direct"));
        QCOMPARE(args[args.indexOf(QStringLiteral("--cookies")) + 1], cookies);
        QCOMPARE(args[args.indexOf(QStringLiteral("--proxy")) + 1], QString());
        QVERIFY(!args.contains(QStringLiteral("--cookies-from-browser")));
        args = DownloadCore::baseArguments({}, {}, {}, QStringLiteral("firefox:Profile 1"));
        QVERIFY(!args.contains(QStringLiteral("--cookies")));
        QVERIFY(args.contains(QStringLiteral("firefox:Profile 1")));
        QVERIFY(!args.contains(QStringLiteral("--proxy")));
        DownloadCore::Options options; options.cookiesFile = cookies; options.networkMode = QStringLiteral("custom");
        options.proxy = QStringLiteral("http://127.0.0.1:7890");
        args = DownloadCore::downloadArguments(options);
        QVERIFY(args.contains(cookies)); QVERIFY(args.contains(options.proxy));
    }
    void cookieValidationAndRedaction() {
        QTemporaryDir dir(QDir::currentPath() + QStringLiteral("/cookie-tests-XXXXXX")); QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("cookies.txt")); QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly)); file.write("{\"cookies\": []}"); file.close();
        QVERIFY(!DownloadCore::validateCookiesFile(path).isEmpty());
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write("# Netscape HTTP Cookie File\n#HttpOnly_.youtube.com\tTRUE\t/\tTRUE\t0\tSESSION\tsecret-value\n.youtube.com\tTRUE\t/\tTRUE\t0\tEMPTY\t\n"); file.close();
        QVERIFY(DownloadCore::validateCookiesFile(path).isEmpty());
        QVERIFY(!DownloadCore::validateCookiesFile(dir.filePath(QStringLiteral("missing.txt"))).isEmpty());
        const QString log = DownloadCore::safeLog(QStringLiteral("Cookie: SESSION=secret-value\nAuthorization: Bearer token-value\nhttps://example.org/?token=private"));
        QVERIFY(!log.contains(QStringLiteral("secret-value"))); QVERIFY(!log.contains(QStringLiteral("token-value"))); QVERIFY(!log.contains(QStringLiteral("private")));
        QVERIFY(DownloadCore::needsVerification(QStringLiteral("Sign in to confirm you’re not a bot")));
        QVERIFY(!DownloadCore::needsVerification(QStringLiteral("Connection timed out")));
        QVERIFY(DownloadCore::failureHint(QStringLiteral("Could not decrypt cookies")).contains(QStringLiteral("无法读取浏览器会话")));
    }
    void urlValidation() {
        QVERIFY(DownloadCore::validUrl(QStringLiteral("https://www.youtube.com/watch?v=abc&list=def")));
        QVERIFY(DownloadCore::validUrl(QStringLiteral("https://youtu.be/abc")));
        QVERIFY(DownloadCore::validUrl(QStringLiteral("https://m.youtube.com/shorts/abc")));
        QVERIFY(!DownloadCore::validUrl(QStringLiteral("https://youtube.com.attacker.test/watch?v=x")));
        QVERIFY(!DownloadCore::validUrl(QStringLiteral("https://attacker.test/youtube.com")));
        QVERIFY(!DownloadCore::validUrl(QStringLiteral("file:///etc/passwd")));
        QVERIFY(!DownloadCore::validUrl(QStringLiteral("--exec calc")));
        QVERIFY(!DownloadCore::validUrl(QStringLiteral("https://user:secret@youtube.com/watch?v=x")));
    }
    void formatFiltering() {
        const auto doc = QJsonDocument::fromJson(R"({"formats":[{"height":1080,"vcodec":"avc1","ext":"mp4"},{"height":1080,"vcodec":"avc1","ext":"mp4"},{"height":2160,"vcodec":"vp9","ext":"webm"},{"height":720,"vcodec":"avc1","ext":"mp4","has_drm":true},{"height":480,"vcodec":"none","ext":"mp4"}]})");
        QCOMPARE(DownloadCore::availableHeights(doc.object(), true), QList<int>{1080});
        QCOMPARE(DownloadCore::availableHeights(doc.object(), false), (QList<int>{2160, 1080}));
    }
    void argumentIsolation() {
        DownloadCore::Options o;
        o.url = QStringLiteral("https://youtu.be/abc?x=1&y=2"); o.directory = QStringLiteral("D:/中文目录/with spaces");
        o.height = 1080; o.ffmpeg = QStringLiteral("C:/media tools/ffmpeg.exe");
        auto args = DownloadCore::downloadArguments(o);
        QCOMPARE(args.last(), o.url); QCOMPARE(args[args.size() - 2], QStringLiteral("--"));
        QVERIFY(args.contains(o.directory)); QVERIFY(args.contains(QStringLiteral("bv[height<=1080][ext=mp4]+ba[ext=m4a]/b[height<=1080][ext=mp4]")));
        o.audioOnly = true; args = DownloadCore::downloadArguments(o);
        QVERIFY(args.contains(QStringLiteral("--audio-format"))); QVERIFY(!args.contains(QStringLiteral("--merge-output-format")));
    }
    void metadataProcess() {
        EngineProcess runner; QSignalSpy lines(&runner, &EngineProcess::outputLine); QSignalSpy completed(&runner, &EngineProcess::completed);
        runner.start(fake(), {QStringLiteral("--dump-single-json")});
        QVERIFY(completed.wait(10000)); QCOMPARE(completed.count(), 1); QVERIFY(completed[0][0].toBool());
        const auto data = QJsonDocument::fromJson(lines[0][0].toByteArray()).object();
        QCOMPARE(data.value(QStringLiteral("title")).toString(), QStringLiteral("中文视频"));
    }
    void fragmentedProcessOutput() {
        EngineProcess runner; QSignalSpy lines(&runner, &EngineProcess::outputLine); QSignalSpy completed(&runner, &EngineProcess::completed);
        runner.start(fake(), {}); QVERIFY(completed.wait(10000)); QVERIFY(completed[0][0].toBool());
        QCOMPARE(lines.count(), 3); QCOMPARE(QString::fromUtf8(lines[0][0].toByteArray()), QStringLiteral("中文输出"));
        QVERIFY(lines[1][0].toByteArray().startsWith("__PROGRESS__"));
        QCOMPARE(lines[2][0].toByteArray(), QByteArray("last line without newline"));
    }
    void processFailureAndRedaction() {
        EngineProcess runner; QSignalSpy completed(&runner, &EngineProcess::completed);
        runner.start(fake(), {QStringLiteral("--fail")}); QVERIFY(completed.wait(10000));
        QVERIFY(!completed[0][0].toBool()); const QString error = completed[0][1].toString();
        QVERIFY(!error.contains(QStringLiteral("private"))); QVERIFY(error.contains(QStringLiteral("代理")));
    }
    void failedToStart() {
        EngineProcess runner; QSignalSpy completed(&runner, &EngineProcess::completed);
        runner.start(QStringLiteral("Z:/nonexistent-engine-xyz.exe"), {});
        QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 1, 10000); QVERIFY(!completed[0][0].toBool()); QVERIFY(!runner.running());
    }
    void cancellation() {
        EngineProcess runner; QSignalSpy completed(&runner, &EngineProcess::completed);
        runner.start(fake(), {QStringLiteral("--wait")});
        QTest::qWait(150); QVERIFY(runner.running()); runner.cancel();
        QVERIFY(completed.wait(10000)); QCOMPARE(completed.count(), 1);
        QVERIFY(!completed[0][0].toBool()); QVERIFY(completed[0][1].toString().contains(QStringLiteral("已停止")));
    }
};
QTEST_GUILESS_MAIN(CoreTests)
#include "core_tests.moc"
