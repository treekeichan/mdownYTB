#pragma once
#include "downloadcore.h"
#include <QMainWindow>
#include <QJsonObject>
#include <QVector>

class QLineEdit;
class QLabel;
class QPushButton;
class QComboBox;
class QTableWidget;
class QPlainTextEdit;
class QCloseEvent;
class EngineProcess;
class QSettings;

struct DownloadTask {
    DownloadCore::Options options;
    QString title, state, file, error;
    int percent = 0;
};

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString &dataDirectory = {});
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    void parse();
    void enqueue();
    void startNext();
    void stopSelected();
    void retrySelected();
    void clearHistory();
    void updateRow(int row, const QString &speed = {}, const QString &eta = {});
    void refreshHeights();
    void showSettings();
    void refreshDependencies();
    void saveTasks();
    void loadTasks();
    QString historyPath() const;
    QSettings localSettings() const;
    QString tool(const QString &name) const;
    void log(const QString &text);
    DownloadCore::Options connectionOptions() const;
    QString connectionError(const DownloadCore::Options &options) const;
    QLineEdit *urlEdit, *directoryEdit;
    QLabel *titleLabel, *detailLabel, *dependencyLabel;
    QPushButton *parseButton, *addButton;
    QWidget *recoveryActions;
    QComboBox *modeBox, *qualityBox, *containerBox, *subtitleBox;
    QTableWidget *table;
    QPlainTextEdit *logs;
    EngineProcess *parser = nullptr, *downloader = nullptr;
    QJsonObject metadata;
    QString parsedUrl;
    QVector<DownloadTask> tasks;
    int activeRow = -1;
    bool closing = false;
    QString dataDirectory;
};
