#include "engineprocess.h"
#include "downloadcore.h"
#ifdef Q_OS_WIN
#include <windows.h>
#endif

EngineProcess::EngineProcess(QObject *parent) : QObject(parent) {
#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#endif
    connect(&process, &QProcess::started, this, [this] {
#ifdef Q_OS_WIN
        job = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (job && SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
            const HANDLE handle = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, DWORD(process.processId()));
            if (!handle || !AssignProcessToJobObject(job, handle)) {
                closeJob();
                diagnostic(QStringLiteral("进程组隔离不可用，停止时可能需要手动结束媒体工具。"));
            }
            if (handle) CloseHandle(handle);
        } else closeJob();
#endif
    });
    connect(&process, &QProcess::readyReadStandardOutput, this, [this] { consume(); });
    connect(&process, &QProcess::readyReadStandardError, this, [this] { consumeErrors(); });
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) finish(false, QStringLiteral("无法启动下载引擎：") + process.errorString());
    });
    connect(&process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        consume(true);
        consumeErrors(true);
        if (cancelled) finish(false, QStringLiteral("已停止，临时文件已保留，可重试续传。"));
        else if (code == 0 && status == QProcess::NormalExit) finish(true, {});
        else finish(false, DownloadCore::failureHint(errors.isEmpty() ? QStringLiteral("引擎异常退出，代码 %1").arg(code) : errors.trimmed()));
    });
}
EngineProcess::~EngineProcess() {
    process.disconnect(this);
    closeJob();
    if (process.state() != QProcess::NotRunning) { process.kill(); process.waitForFinished(3000); }
}
bool EngineProcess::running() const { return !done; }
void EngineProcess::start(const QString &program, const QStringList &arguments) {
    if (!done) return;
    buffer.clear(); errorBuffer.clear(); errors.clear(); cancelled = false; done = false;
    process.start(program, arguments);
}
void EngineProcess::cancel() {
    if (done) return;
    cancelled = true;
    closeJob();
    process.kill();
}
void EngineProcess::closeJob() {
#ifdef Q_OS_WIN
    if (job) CloseHandle(job);
#endif
    job = nullptr;
}
void EngineProcess::consume(bool final) {
    buffer += process.readAllStandardOutput();
    if (buffer.size() > 32 * 1024 * 1024) {
        errors = QStringLiteral("引擎输出超出 32 MB 限制。");
        buffer.clear(); process.kill(); return;
    }
    qsizetype index;
    while ((index = buffer.indexOf('\n')) >= 0) {
        const auto line = buffer.left(index).trimmed();
        buffer.remove(0, index + 1);
        if (!line.isEmpty()) emit outputLine(line);
    }
    if (final && !buffer.trimmed().isEmpty()) { emit outputLine(buffer.trimmed()); buffer.clear(); }
}
void EngineProcess::finish(bool success, const QString &error) {
    if (done) return;
    done = true; closeJob(); emit completed(success, error);
}
void EngineProcess::consumeErrors(bool final) {
    errorBuffer += process.readAllStandardError();
    if (errorBuffer.size() > 1024 * 1024) errorBuffer = errorBuffer.right(16000);
    const auto consumeLine = [this](const QByteArray &line) {
        if (line.startsWith("__PROGRESS__") || line.startsWith("__FILE__")) { emit outputLine(line); return; }
        const QString text = DownloadCore::safeLog(QString::fromUtf8(line));
        if (text.isEmpty()) return;
        errors = (errors + text + QLatin1Char('\n')).right(16000);
        emit diagnostic(text);
    };
    qsizetype index;
    while ((index = errorBuffer.indexOf('\n')) >= 0) {
        consumeLine(errorBuffer.left(index).trimmed()); errorBuffer.remove(0, index + 1);
    }
    if (final && !errorBuffer.trimmed().isEmpty()) { consumeLine(errorBuffer.trimmed()); errorBuffer.clear(); }
}
