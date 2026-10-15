#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QThread>
#include <QDir>
#include <QJsonArray>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    QTextStream out(stdout); out.setEncoding(QStringConverter::Utf8);
    if (args.last().contains(QStringLiteral("verify-session"))) {
        if (!args.contains(QStringLiteral("--cookies"))) {
            QTextStream(stderr) << "ERROR: Sign in to confirm you're not a bot\n"; return 1;
        }
        const int cookieIndex = args.indexOf(QStringLiteral("--cookies"));
        if (cookieIndex + 1 >= args.size() || !QFile::exists(args[cookieIndex + 1])) return 5;
    }
    if (args.contains(QStringLiteral("--fail"))) { QTextStream(stderr) << "ERROR: unable to download https://secret.example/?token=private\n"; return 2; }
    if (args.contains(QStringLiteral("--wait"))) { QThread::sleep(30); return 0; }
    if (args.contains(QStringLiteral("--dump-single-json"))) {
        out << "{\"id\":\"test\",\"title\":\"\u4e2d\u6587\u89c6\u9891\",\"formats\":[]}\n"; out.flush(); return 0;
    }
    const int pathIndex = args.indexOf(QStringLiteral("--paths"));
    if (pathIndex >= 0 && pathIndex + 1 < args.size()) {
        const QString path = QDir(args[pathIndex + 1]).filePath(QStringLiteral("中文测试视频 [test].mp4"));
        QTextStream err(stderr); err.setEncoding(QStringConverter::Utf8);
        err << "__PROGRESS__{\"status\":\"downloading\",\"downloaded_bytes\":500,\"total_bytes\":1000,\"speed\":200,\"eta\":3}\n"; err.flush();
        QThread::msleep(400);
        QFile file(path); if (!file.open(QIODevice::WriteOnly)) return 3; file.write("test payload"); file.close();
        err << "__PROGRESS__{\"status\":\"finished\",\"downloaded_bytes\":1000,\"total_bytes\":1000}\n"; err.flush();
        const QByteArray array = QJsonDocument(QJsonArray{path}).toJson(QJsonDocument::Compact);
        out << "__FILE__" << QString::fromUtf8(array.mid(1, array.size() - 2)) << "\n"; out.flush(); return 0;
    }
    // Split both a line and a UTF-8 character across writes to exercise stream framing.
    const QByteArray message = QStringLiteral("中文输出\n").toUtf8();
#ifdef Q_OS_WIN
    DWORD written = 0; const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    WriteFile(handle, message.constData(), 2, &written, nullptr);
    QThread::msleep(40);
    WriteFile(handle, message.constData() + 2, DWORD(message.size() - 2), &written, nullptr);
#else
    QFile stream; stream.open(stdout, QIODevice::WriteOnly); stream.write(message.left(2)); stream.flush();
    QThread::msleep(40); stream.write(message.mid(2)); stream.flush();
#endif
    out << "__PROGRESS__{\"downloaded_bytes\":500,\"total_bytes\":1000,\"speed\":200,\"eta\":3}\n"; out.flush();
    out << "last line without newline"; out.flush(); return 0;
}
