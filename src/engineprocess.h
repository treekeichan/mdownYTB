#pragma once
#include <QObject>
#include <QProcess>

class EngineProcess final : public QObject {
    Q_OBJECT
public:
    explicit EngineProcess(QObject *parent = nullptr);
    ~EngineProcess() override;
    void start(const QString &program, const QStringList &arguments);
    void cancel();
    bool running() const;
signals:
    void outputLine(const QByteArray &line);
    void diagnostic(const QString &line);
    void completed(bool success, const QString &error);
private:
    void consume(bool final = false);
    void consumeErrors(bool final = false);
    void finish(bool success, const QString &error);
    void closeJob();
    QProcess process;
    QByteArray buffer, errorBuffer;
    QString errors;
    bool cancelled = false, done = true;
    void *job = nullptr;
};
