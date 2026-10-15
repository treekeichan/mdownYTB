#include "mainwindow.h"
#include "engineprocess.h"
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QSplitter>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <memory>
#include <QVersionNumber>

MainWindow::MainWindow(const QString &directory) {
    dataDirectory = directory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) : directory;
    QDir().mkpath(dataDirectory);
    setWindowTitle(QStringLiteral("mdownYTB · YouTube 视频下载"));
    resize(1080, 800); setMinimumSize(800, 640);
    auto *central = new QWidget;
    setCentralWidget(central);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(24, 20, 24, 18); layout->setSpacing(14);
    auto *header = new QHBoxLayout;
    auto *brand = new QLabel(QStringLiteral("mdownYTB"));
    brand->setObjectName(QStringLiteral("brand"));
    auto *version = new QLabel(QStringLiteral("本地下载工作台  /  v0.2.1"));
    version->setObjectName(QStringLiteral("secondary"));
    auto *settingsButton = new QPushButton(QStringLiteral("设置与依赖"));
    header->addWidget(brand); header->addWidget(version); header->addStretch(); header->addWidget(settingsButton);
    layout->addLayout(header);

    auto *urlRow = new QHBoxLayout;
    urlEdit = new QLineEdit;
    urlEdit->setObjectName(QStringLiteral("urlInput"));
    urlEdit->setPlaceholderText(QStringLiteral("粘贴 YouTube 视频或 Shorts 链接"));
    urlEdit->setClearButtonEnabled(true);
    parseButton = new QPushButton(QStringLiteral("解析链接"));
    parseButton->setObjectName(QStringLiteral("primary"));
    urlRow->addWidget(urlEdit, 1); urlRow->addWidget(parseButton); layout->addLayout(urlRow);
    auto *preview = new QWidget; preview->setObjectName(QStringLiteral("preview"));
    auto *previewLayout = new QVBoxLayout(preview);
    previewLayout->setContentsMargins(18, 16, 18, 16);
    titleLabel = new QLabel(QStringLiteral("从一个视频链接开始"));
    titleLabel->setObjectName(QStringLiteral("videoTitle")); titleLabel->setWordWrap(true);
    titleLabel->setTextFormat(Qt::PlainText);
    detailLabel = new QLabel(QStringLiteral("解析后查看标题、时长及可用清晰度。当前版本下载单视频。"));
    detailLabel->setTextFormat(Qt::PlainText); detailLabel->setWordWrap(true);
    previewLayout->addWidget(titleLabel); previewLayout->addWidget(detailLabel); layout->addWidget(preview);
    recoveryActions = new QWidget;
    recoveryActions->setObjectName(QStringLiteral("recoveryActions"));
    auto *recoveryLayout = new QHBoxLayout(recoveryActions);
    recoveryLayout->setContentsMargins(0, 0, 0, 0);
    auto *openVideo = new QPushButton(QStringLiteral("打开视频完成验证"));
    auto *configureSession = new QPushButton(QStringLiteral("配置 Cookies / 网络"));
    auto *parseAgain = new QPushButton(QStringLiteral("重新解析"));
    recoveryLayout->addWidget(openVideo); recoveryLayout->addWidget(configureSession); recoveryLayout->addWidget(parseAgain); recoveryLayout->addStretch();
    recoveryActions->hide(); layout->addWidget(recoveryActions);
    connect(openVideo, &QPushButton::clicked, this, [this] {
        if (DownloadCore::validUrl(urlEdit->text())) QDesktopServices::openUrl(QUrl(urlEdit->text().trimmed()));
    });
    connect(configureSession, &QPushButton::clicked, this, &MainWindow::showSettings);
    connect(parseAgain, &QPushButton::clicked, this, &MainWindow::parse);

    auto *options = new QHBoxLayout;
    modeBox = new QComboBox; modeBox->addItems({QStringLiteral("视频 + 音频"), QStringLiteral("仅音频（M4A）")});
    qualityBox = new QComboBox; qualityBox->addItem(QStringLiteral("最佳可用"), 0);
    containerBox = new QComboBox; containerBox->addItems({QStringLiteral("MP4"), QStringLiteral("MKV")});
    subtitleBox = new QComboBox; subtitleBox->addItem(QStringLiteral("不下载字幕"), QString());
    subtitleBox->addItem(QStringLiteral("中文（若有人工字幕）"), QStringLiteral("zh.*"));
    subtitleBox->addItem(QStringLiteral("英文（若有人工字幕）"), QStringLiteral("en.*"));
    const QList<QPair<QString, QComboBox *>> fields = {
        {QStringLiteral("下载内容"), modeBox}, {QStringLiteral("清晰度上限"), qualityBox},
        {QStringLiteral("封装格式"), containerBox}, {QStringLiteral("字幕"), subtitleBox}};
    for (const auto &field : fields) {
        auto *column = new QVBoxLayout;
        column->setSpacing(6); column->addWidget(new QLabel(field.first)); column->addWidget(field.second);
        options->addLayout(column, 1);
    }
    layout->addLayout(options);
    auto *pathRow = new QHBoxLayout;
    directoryEdit = new QLineEdit;
    directoryEdit->setObjectName(QStringLiteral("directoryInput"));
    const QString defaultDirectory = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + QStringLiteral("/YouTube");
    directoryEdit->setText(localSettings().value(QStringLiteral("directory"), defaultDirectory).toString());
    auto *browse = new QPushButton(QStringLiteral("选择目录"));
    addButton = new QPushButton(QStringLiteral("加入下载队列")); addButton->setObjectName(QStringLiteral("primary")); addButton->setEnabled(false);
    pathRow->addWidget(new QLabel(QStringLiteral("保存到"))); pathRow->addWidget(directoryEdit, 1);
    pathRow->addWidget(browse); pathRow->addWidget(addButton); layout->addLayout(pathRow);

    auto *queueBar = new QHBoxLayout;
    queueBar->addWidget(new QLabel(QStringLiteral("下载队列 / 历史记录"))); queueBar->addStretch();
    auto *stop = new QPushButton(QStringLiteral("停止选中"));
    auto *retry = new QPushButton(QStringLiteral("重试选中"));
    auto *open = new QPushButton(QStringLiteral("打开目录"));
    auto *clear = new QPushButton(QStringLiteral("清空历史记录"));
    clear->setToolTip(QStringLiteral("清除已完成、失败和已停止的记录；保留下载中及等待任务，不删除文件。"));
    queueBar->addWidget(stop); queueBar->addWidget(retry); queueBar->addWidget(open); queueBar->addWidget(clear); layout->addLayout(queueBar);
    connect(clear, &QPushButton::clicked, this, &MainWindow::clearHistory);
    auto *splitter = new QSplitter(Qt::Vertical);
    table = new QTableWidget(0, 6);
    table->setObjectName(QStringLiteral("taskTable"));
    table->setHorizontalHeaderLabels({QStringLiteral("视频 / 文件"), QStringLiteral("状态"), QStringLiteral("进度"), QStringLiteral("速度"), QStringLiteral("剩余"), QStringLiteral("格式")});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setMinimumSectionSize(80);
    for (int i = 1; i < 6; ++i) table->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    table->verticalHeader()->hide(); table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection); table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true); table->setWordWrap(false);
    logs = new QPlainTextEdit; logs->setReadOnly(true); logs->setMaximumBlockCount(350);
    logs->setPlaceholderText(QStringLiteral("运行日志与错误详情显示在这里。"));
    splitter->addWidget(table); splitter->addWidget(logs); splitter->setSizes({260, 90}); layout->addWidget(splitter, 1);
    dependencyLabel = new QLabel; dependencyLabel->setWordWrap(true); layout->addWidget(dependencyLabel);
    auto *footer = new QLabel(QStringLiteral("顺序下载 · 停止后保留临时文件 · 请下载你拥有或获授权的内容"));
    footer->setObjectName(QStringLiteral("secondary")); layout->addWidget(footer);
    setStyleSheet(QStringLiteral(
        "QMainWindow { background: #f5f7fb; } QWidget { color: #202a3b; font-family: 'Microsoft YaHei'; font-size: 13px; }"
        "QLabel#brand { font-size: 25px; font-weight: 700; } QLabel#secondary { color: #65748b; }"
        "QWidget#preview { background: #e9effa; border-radius: 8px; } QLabel#videoTitle { font-size: 17px; font-weight: 600; }"
        "QLineEdit, QComboBox { background: white; color: #202a3b; border: 1px solid #cbd4e1; border-radius: 5px; padding: 8px; min-height: 20px; }"
        "QComboBox QAbstractItemView { background: white; color: #202a3b; selection-background-color: #e0eaff; selection-color: #142c55; }"
        "QPushButton { background: white; border: 1px solid #cbd4e1; padding: 9px 14px; border-radius: 5px; }"
        "QPushButton:hover { background: #e8eef9; } QPushButton#primary { background: #285ed7; color: white; border-color: #285ed7; }"
        "QPushButton#primary:hover { background: #204eba; } QPushButton:disabled, QPushButton#primary:disabled { color: #8895aa; background: #e8ecf2; border-color: #d5dce8; }"
        "QTableWidget { background: white; alternate-background-color: #f8faff; border: 1px solid #dce3ed; gridline-color: #e8edf5; selection-background-color: #deebff; selection-color: #152c53; }"
        "QHeaderView::section { background: #edf1f7; padding: 10px 6px; border: none; border-bottom: 1px solid #dce3ed; }"
        "QProgressBar { border: none; background: #e6ecf6; border-radius: 3px; text-align: center; color: #193966; min-width: 100px; }"
        "QProgressBar::chunk { background: #83b0ff; border-radius: 3px; }"
        "QPlainTextEdit { background: #f0f3f8; border: 1px solid #dce3ed; border-radius: 5px; padding: 6px; }"
        "QDialog { background: #f5f7fb; }"));

    connect(settingsButton, &QPushButton::clicked, this, &MainWindow::showSettings);
    connect(parseButton, &QPushButton::clicked, this, &MainWindow::parse);
    connect(urlEdit, &QLineEdit::returnPressed, this, &MainWindow::parse);
    connect(urlEdit, &QLineEdit::textChanged, this, [this] { addButton->setEnabled(!parsedUrl.isEmpty() && parsedUrl == urlEdit->text().trimmed()); });
    connect(addButton, &QPushButton::clicked, this, &MainWindow::enqueue);
    connect(browse, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getExistingDirectory(this, QStringLiteral("选择下载目录"), directoryEdit->text());
        if (!path.isEmpty()) directoryEdit->setText(QDir::toNativeSeparators(path));
    });
    connect(containerBox, &QComboBox::currentIndexChanged, this, &MainWindow::refreshHeights);
    connect(modeBox, &QComboBox::currentIndexChanged, this, [this](int index) {
        qualityBox->setEnabled(index == 0); containerBox->setEnabled(index == 0);
    });
    connect(stop, &QPushButton::clicked, this, &MainWindow::stopSelected);
    connect(retry, &QPushButton::clicked, this, &MainWindow::retrySelected);
    connect(open, &QPushButton::clicked, this, [this] {
        const int row = table->currentRow();
        const QString folder = row >= 0 ? tasks[row].options.directory : directoryEdit->text();
        if (QFileInfo(folder).isDir()) QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
        else QMessageBox::information(this, QStringLiteral("目录不存在"), QStringLiteral("下载目录尚未创建。"));
    });
    connect(table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        if (!tasks[row].error.isEmpty()) QMessageBox::information(this, tasks[row].title, tasks[row].error);
        else if (tasks[row].state == QStringLiteral("已完成") && QFileInfo::exists(tasks[row].file))
            QDesktopServices::openUrl(QUrl::fromLocalFile(tasks[row].file));
    });
    loadTasks(); refreshDependencies();
}

QString MainWindow::tool(const QString &name) const {
    return DownloadCore::findTool(name, localSettings().value(QStringLiteral("tools/") + name).toString());
}
void MainWindow::refreshDependencies() {
    const QStringList names = {QStringLiteral("yt-dlp"), QStringLiteral("ffmpeg"), QStringLiteral("ffprobe"), QStringLiteral("node")};
    QStringList status;
    for (const auto &name : names) status << name + (tool(name).isEmpty() ? QStringLiteral("：未找到") : QStringLiteral("：已找到"));
    dependencyLabel->setText(status.join(QStringLiteral("    ·    ")));
}
void MainWindow::log(const QString &text) {
    logs->appendPlainText(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss  ")) + DownloadCore::safeLog(text));
}
void MainWindow::parse() {
    if (parser && parser->running()) return;
    const QString url = urlEdit->text().trimmed();
    if (!DownloadCore::validUrl(url)) { QMessageBox::information(this, QStringLiteral("检查链接"), QStringLiteral("请输入完整的 YouTube 视频链接，例如 https://www.youtube.com/watch?v=…")); return; }
    const auto engine = tool(QStringLiteral("yt-dlp"));
    if (engine.isEmpty()) { showSettings(); return; }
    const auto connection = connectionOptions();
    const auto configError = connectionError(connection);
    if (!configError.isEmpty()) {
        titleLabel->setText(QStringLiteral("请检查会话配置")); detailLabel->setText(configError);
        recoveryActions->show(); addButton->setEnabled(false); log(configError); return;
    }
    recoveryActions->hide();
    parsedUrl.clear(); metadata = {}; addButton->setEnabled(false); parseButton->setEnabled(false);
    titleLabel->setText(QStringLiteral("正在解析视频…")); detailLabel->setText(QStringLiteral("正在读取视频信息，网络较慢时可能需要等待。"));
    auto *runner = new EngineProcess(this); parser = runner;
    auto *timeout = new QTimer(runner); timeout->setSingleShot(true); timeout->setInterval(90000);
    connect(timeout, &QTimer::timeout, runner, [this, runner] { log(QStringLiteral("解析超过 90 秒，已停止。请检查网络或代理。")); runner->cancel(); });
    auto parsed = std::make_shared<QJsonObject>();
    connect(runner, &EngineProcess::outputLine, this, [parsed](const QByteArray &line) {
        QJsonParseError error; const auto doc = QJsonDocument::fromJson(line, &error);
        if (error.error == QJsonParseError::NoError && doc.isObject()) *parsed = doc.object();
    });
    connect(runner, &EngineProcess::diagnostic, this, &MainWindow::log);
    connect(runner, &EngineProcess::completed, this, [this, runner, timeout, parsed, url](bool success, const QString &error) {
        timeout->stop(); parser = nullptr; parseButton->setEnabled(true);
        if (success && !parsed->value(QStringLiteral("id")).toString().isEmpty()) {
            metadata = *parsed; parsedUrl = url;
            titleLabel->setText(metadata.value(QStringLiteral("title")).toString(QStringLiteral("未命名视频")));
            const int seconds = metadata.value(QStringLiteral("duration")).toInt();
            detailLabel->setText(QStringLiteral("%1  ·  %2:%3  ·  %4 种格式").arg(metadata.value(QStringLiteral("uploader")).toString())
                .arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0')).arg(metadata.value(QStringLiteral("formats")).toArray().size()));
            refreshHeights(); addButton->setEnabled(urlEdit->text().trimmed() == url);
            log(QStringLiteral("解析成功：") + titleLabel->text());
        } else {
            const QString message = error.isEmpty() ? QStringLiteral("引擎未返回有效视频信息。") : error;
            const bool verification = DownloadCore::needsVerification(message);
            titleLabel->setText(verification ? QStringLiteral("需要平台验证") : QStringLiteral("解析未完成"));
            detailLabel->setText(verification
                ? QStringLiteral("请打开视频完成验证，配置 Cookies 或浏览器会话后重试。若已配置，请刷新会话并核对浏览器与软件的网络出口。")
                : message.section(QLatin1Char('\n'), 0, 0).left(180));
            recoveryActions->show(); log(message);
        }
        runner->deleteLater();
    });
    auto args = DownloadCore::baseArguments(connection.proxy, connection.node, connection.cookiesFile, connection.browser, connection.networkMode);
    args << QStringLiteral("--dump-single-json") << QStringLiteral("--skip-download") << QStringLiteral("--") << url;
    log(QStringLiteral("开始解析视频。")); timeout->start(); runner->start(engine, args);
}
void MainWindow::refreshHeights() {
    const int previous = qualityBox->currentData().toInt();
    qualityBox->clear(); qualityBox->addItem(QStringLiteral("最佳可用"), 0);
    for (const int height : DownloadCore::availableHeights(metadata, containerBox->currentIndex() == 0))
        qualityBox->addItem(QStringLiteral("%1p").arg(height), height);
    const int index = qualityBox->findData(previous); if (index >= 0) qualityBox->setCurrentIndex(index);
}
void MainWindow::enqueue() {
    if (parsedUrl.isEmpty() || parsedUrl != urlEdit->text().trimmed()) return;
    if (tool(QStringLiteral("yt-dlp")).isEmpty() || tool(QStringLiteral("ffmpeg")).isEmpty() || tool(QStringLiteral("ffprobe")).isEmpty() || tool(QStringLiteral("node")).isEmpty()) {
        QMessageBox::information(this, QStringLiteral("依赖未齐全"), QStringLiteral("请先在设置中配置 yt-dlp、FFmpeg、FFprobe 和 Node.js 22 或更新版本。")); showSettings(); return;
    }
    const QString directory = QDir::cleanPath(directoryEdit->text().trimmed());
    if (directoryEdit->text().trimmed().isEmpty() || !QDir::isAbsolutePath(directory) || !QDir().mkpath(directory)) {
        QMessageBox::information(this, QStringLiteral("保存目录不可用"), QStringLiteral("请选择可写的绝对路径。")); return;
    }
    QSaveFile probe(QDir(directory).filePath(QStringLiteral(".mdown-write-test")));
    if (!probe.open(QIODevice::WriteOnly)) { QMessageBox::warning(this, QStringLiteral("无法写入"), probe.errorString()); return; }
    probe.cancelWriting();
    const bool audio = modeBox->currentIndex() == 1;
    const QString container = containerBox->currentIndex() == 0 ? QStringLiteral("mp4") : QStringLiteral("mkv");
    const int height = qualityBox->currentData().toInt();
    for (const auto &task : tasks) {
        if ((task.state == QStringLiteral("等待中") || task.state == QStringLiteral("下载中") || task.state == QStringLiteral("合并 / 处理中"))
            && task.options.url == parsedUrl && task.options.directory == directory && task.options.audioOnly == audio
            && task.options.container == container && task.options.height == height) {
            QMessageBox::information(this, QStringLiteral("任务已存在"), QStringLiteral("相同的下载任务已在队列中。")); return;
        }
    }
    DownloadTask task;
    task.title = metadata.value(QStringLiteral("title")).toString(QStringLiteral("未命名视频"));
    task.state = QStringLiteral("等待中"); task.options.url = parsedUrl; task.options.directory = directory;
    task.options.audioOnly = audio; task.options.container = container; task.options.height = height;
    task.options.subtitles = subtitleBox->currentData().toString();
    tasks.append(task); updateRow(tasks.size() - 1); table->selectRow(tasks.size() - 1);
    localSettings().setValue(QStringLiteral("directory"), directory); saveTasks(); startNext();
}
void MainWindow::startNext() {
    if (closing || activeRow >= 0) return;
    int row = -1;
    for (int i = 0; i < tasks.size(); ++i) if (tasks[i].state == QStringLiteral("等待中")) { row = i; break; }
    if (row < 0) return;
    activeRow = row;
    auto &task = tasks[row]; task.state = QStringLiteral("下载中"); task.error.clear(); task.percent = 0; task.file.clear();
    auto options = task.options;
    const auto connection = connectionOptions();
    options.ffmpeg = tool(QStringLiteral("ffmpeg")); options.node = connection.node;
    options.proxy = connection.proxy; options.networkMode = connection.networkMode;
    options.cookiesFile = connection.cookiesFile; options.browser = connection.browser;
    const auto configError = connectionError(connection);
    if (!configError.isEmpty()) {
        task.state = QStringLiteral("失败"); task.error = configError; activeRow = -1;
        updateRow(row); saveTasks(); log(configError); QTimer::singleShot(0, this, &MainWindow::startNext); return;
    }
    auto *runner = new EngineProcess(this); downloader = runner;
    connect(runner, &EngineProcess::diagnostic, this, &MainWindow::log);
    connect(runner, &EngineProcess::outputLine, this, [this](const QByteArray &line) {
        const int row = activeRow;
        if (row < 0) return;
        auto &task = tasks[row];
        if (line.startsWith("__PROGRESS__")) {
            const auto doc = QJsonDocument::fromJson(line.mid(12));
            if (!doc.isObject()) return;
            const auto p = doc.object();
            const double total = p.value(QStringLiteral("total_bytes")).toDouble(p.value(QStringLiteral("total_bytes_estimate")).toDouble());
            const double downloaded = p.value(QStringLiteral("downloaded_bytes")).toDouble();
            const bool finished = p.value(QStringLiteral("status")).toString() == QStringLiteral("finished");
            task.percent = total > 0 ? std::clamp(int(100 * downloaded / total), 0, 99) : 0;
            if (finished) task.state = QStringLiteral("合并 / 处理中"); else task.state = QStringLiteral("下载中");
            const int eta = p.value(QStringLiteral("eta")).toInt(-1);
            updateRow(row, DownloadCore::readableBytes(p.value(QStringLiteral("speed")).toDouble()) + QStringLiteral("/s"),
                eta < 0 ? QStringLiteral("—") : QStringLiteral("%1:%2").arg(eta / 60).arg(eta % 60, 2, 10, QLatin1Char('0')));
        } else if (line.startsWith("__FILE__")) {
            const auto doc = QJsonDocument::fromJson(QByteArray("[") + line.mid(8) + QByteArray("]"));
            if (doc.isArray() && !doc.array().isEmpty()) task.file = doc.array().first().toString();
        } else log(QString::fromUtf8(line));
    });
    connect(runner, &EngineProcess::completed, this, [this, runner](bool success, const QString &error) {
        const int row = activeRow;
        if (row < 0) return;
        auto &task = tasks[row];
        if (success && !task.file.isEmpty() && QFileInfo(task.file).isFile()) { task.state = QStringLiteral("已完成"); task.percent = 100; }
        else if (task.state != QStringLiteral("已停止")) {
            task.state = QStringLiteral("失败"); task.error = error.isEmpty() ? QStringLiteral("下载结束但未找到最终文件，请查看日志。") : error;
        }
        if (!error.isEmpty()) task.error = error;
        log(task.title + QStringLiteral("：") + task.state + (task.error.isEmpty() ? QString() : QStringLiteral("\n") + task.error));
        updateRow(row); activeRow = -1; downloader = nullptr; saveTasks(); runner->deleteLater();
        QTimer::singleShot(0, this, &MainWindow::startNext);
    });
    updateRow(row); saveTasks(); log(QStringLiteral("开始下载：") + task.title);
    runner->start(tool(QStringLiteral("yt-dlp")), DownloadCore::downloadArguments(options));
}
void MainWindow::updateRow(int row, const QString &speed, const QString &eta) {
    if (table->rowCount() <= row) table->setRowCount(row + 1);
    const auto &task = tasks[row];
    const QStringList values = {task.title, task.state, QString(), speed.isEmpty() ? QStringLiteral("—") : speed,
        eta.isEmpty() ? QStringLiteral("—") : eta, task.options.audioOnly ? QStringLiteral("M4A") : task.options.container.toUpper()};
    for (int i = 0; i < values.size(); ++i) {
        if (i == 2) continue;
        auto *item = table->item(row, i);
        if (!item) { item = new QTableWidgetItem; table->setItem(row, i, item); }
        item->setText(values[i]); item->setToolTip(task.error.isEmpty() ? task.title : task.error);
    }
    auto *bar = qobject_cast<QProgressBar *>(table->cellWidget(row, 2));
    if (!bar) { bar = new QProgressBar; bar->setRange(0, 100); table->setCellWidget(row, 2, bar); }
    bar->setValue(task.percent); table->setRowHeight(row, 42);
}
void MainWindow::stopSelected() {
    const int row = table->currentRow(); if (row < 0) return;
    auto &task = tasks[row];
    if (row == activeRow && downloader) { task.state = QStringLiteral("已停止"); downloader->cancel(); }
    else if (task.state == QStringLiteral("等待中")) task.state = QStringLiteral("已停止");
    updateRow(row); saveTasks();
}
void MainWindow::retrySelected() {
    const int row = table->currentRow(); if (row < 0 || row == activeRow) return;
    auto &task = tasks[row];
    if (task.state != QStringLiteral("失败") && task.state != QStringLiteral("已停止")) return;
    task.state = QStringLiteral("等待中"); task.error.clear(); task.percent = 0;
    updateRow(row); saveTasks(); startNext();
}
void MainWindow::clearHistory() {
    int removed = 0;
    // Remove backwards so retained rows keep their progress widgets and selection.
    // An active task may already say "stopped" while its process is still exiting.
    for (int row = int(tasks.size()) - 1; row >= 0; --row) {
        const auto &state = tasks[row].state;
        if (row == activeRow || (state != QStringLiteral("已完成") && state != QStringLiteral("失败")
                                 && state != QStringLiteral("已停止"))) continue;
        tasks.removeAt(row); table->removeRow(row);
        if (activeRow > row) --activeRow;
        ++removed;
    }
    saveTasks();
    log(removed > 0 ? QStringLiteral("已清空 %1 条历史记录，下载文件保留。").arg(removed)
                    : QStringLiteral("没有可清空的历史记录。"));
}

void MainWindow::showSettings() {
    QDialog dialog(this); dialog.setWindowTitle(QStringLiteral("设置与依赖")); dialog.resize(800, 650);
    auto *layout = new QVBoxLayout(&dialog);
    auto *intro = new QLabel(QStringLiteral("优先使用配置的路径，其次搜索程序 tools 目录及系统 PATH。\nNode.js 需 22 或更新版本；yt-dlp 官方 Windows 包包含 EJS。"));
    intro->setWordWrap(true); layout->addWidget(intro);
    auto *form = new QFormLayout;
    QMap<QString, QLineEdit *> edits;
    for (const auto &name : {QStringLiteral("yt-dlp"), QStringLiteral("ffmpeg"), QStringLiteral("ffprobe"), QStringLiteral("node")}) {
        auto *row = new QHBoxLayout; auto *edit = new QLineEdit(tool(name));
        auto *choose = new QPushButton(QStringLiteral("浏览"));
        edits.insert(name, edit); row->addWidget(edit, 1); row->addWidget(choose); form->addRow(name, row);
        connect(choose, &QPushButton::clicked, &dialog, [this, edit, name] {
            const auto path = QFileDialog::getOpenFileName(this, QStringLiteral("选择 ") + name, edit->text(), QStringLiteral("可执行文件 (*.exe)"));
            if (!path.isEmpty()) edit->setText(QDir::toNativeSeparators(path));
        });
    }
    auto *proxy = new QLineEdit(localSettings().value(QStringLiteral("proxy")).toString());
    proxy->setObjectName(QStringLiteral("proxyInput"));
    proxy->setPlaceholderText(QStringLiteral("例如 http://127.0.0.1:7890"));
    auto *network = new QComboBox;
    network->setObjectName(QStringLiteral("networkMode"));
    network->addItem(QStringLiteral("系统默认（继承引擎 / 环境代理）"), QStringLiteral("system"));
    network->addItem(QStringLiteral("直连（禁用代理）"), QStringLiteral("direct"));
    network->addItem(QStringLiteral("指定代理"), QStringLiteral("custom"));
    network->setCurrentIndex(std::max(0, network->findData(connectionOptions().networkMode)));
    proxy->setEnabled(network->currentData().toString() == QStringLiteral("custom"));
    connect(network, &QComboBox::currentIndexChanged, &dialog, [network, proxy] { proxy->setEnabled(network->currentData().toString() == QStringLiteral("custom")); });
    form->addRow(QStringLiteral("网络模式"), network); form->addRow(QStringLiteral("代理地址"), proxy);
    auto *sessionMode = new QComboBox; sessionMode->setObjectName(QStringLiteral("sessionMode"));
    sessionMode->addItem(QStringLiteral("不使用登录会话"), QStringLiteral("none"));
    sessionMode->addItem(QStringLiteral("Cookies 文件（推荐）"), QStringLiteral("file"));
    sessionMode->addItem(QStringLiteral("读取我选择的浏览器"), QStringLiteral("browser"));
    sessionMode->setCurrentIndex(std::max(0, sessionMode->findData(localSettings().value(QStringLiteral("sessionMode"), QStringLiteral("none")))));
    auto *cookieRow = new QHBoxLayout;
    auto *cookies = new QLineEdit(localSettings().value(QStringLiteral("cookiesFile")).toString());
    cookies->setObjectName(QStringLiteral("cookiesInput")); cookies->setPlaceholderText(QStringLiteral("选择 Netscape 格式的 cookies.txt"));
    auto *chooseCookies = new QPushButton(QStringLiteral("选择文件")); cookieRow->addWidget(cookies, 1); cookieRow->addWidget(chooseCookies);
    connect(chooseCookies, &QPushButton::clicked, &dialog, [&dialog, cookies] {
        const auto path = QFileDialog::getOpenFileName(&dialog, QStringLiteral("选择 Cookies 文件"), cookies->text(), QStringLiteral("Cookies 文件 (*.txt);;所有文件 (*)"));
        if (!path.isEmpty()) cookies->setText(QDir::toNativeSeparators(path));
    });
    auto *browser = new QComboBox; browser->setObjectName(QStringLiteral("sessionBrowser"));
    browser->addItems({QStringLiteral("chrome"), QStringLiteral("edge"), QStringLiteral("firefox")});
    browser->setCurrentText(localSettings().value(QStringLiteral("browser"), QStringLiteral("chrome")).toString());
    auto *profile = new QLineEdit(localSettings().value(QStringLiteral("browserProfile")).toString());
    profile->setPlaceholderText(QStringLiteral("可选：配置文件名或目录；留空由引擎选择"));
    const auto updateSession = [=] {
        const auto mode = sessionMode->currentData().toString();
        cookies->setEnabled(mode == QStringLiteral("file")); chooseCookies->setEnabled(mode == QStringLiteral("file"));
        browser->setEnabled(mode == QStringLiteral("browser")); profile->setEnabled(mode == QStringLiteral("browser"));
    };
    connect(sessionMode, &QComboBox::currentIndexChanged, &dialog, updateSession); updateSession();
    form->addRow(QStringLiteral("登录会话"), sessionMode); form->addRow(QStringLiteral("Cookies 文件"), cookieRow);
    form->addRow(QStringLiteral("浏览器"), browser); form->addRow(QStringLiteral("浏览器配置文件"), profile);
    layout->addLayout(form);
    auto *sessionHelp = new QLabel(QStringLiteral("仅在你选择后使用会话。仅保存配置路径，不将 Cookies 内容写入日志或历史。Cookies 文件可能被引擎更新。\n浏览器会话读取失败时，请改用 Cookies 文件。完成验证的浏览器与软件应使用相同网络出口。"));
    sessionHelp->setWordWrap(true); layout->addWidget(sessionHelp);
    auto *diagnose = new QPushButton(QStringLiteral("检测工具版本"));
    auto *diagnosticResult = new QLabel(QStringLiteral("可检测引擎和媒体工具版本，以及 Node.js 是否达到 22。EJS 可用性需结合真实解析确认。"));
    diagnosticResult->setWordWrap(true); diagnosticResult->setTextFormat(Qt::PlainText);
    layout->addWidget(diagnose); layout->addWidget(diagnosticResult);
    connect(diagnose, &QPushButton::clicked, &dialog, [&, diagnose, diagnosticResult] {
        diagnose->setEnabled(false); diagnosticResult->clear();
        auto remaining = std::make_shared<int>(int(edits.size()));
        for (auto it = edits.cbegin(); it != edits.cend(); ++it) {
            const QString name = it.key();
            auto *runner = new EngineProcess(&dialog);
            auto output = std::make_shared<QString>();
            const auto capture = [output](const QString &line) { if (output->isEmpty()) *output = line.left(180); };
            connect(runner, &EngineProcess::outputLine, &dialog, [capture](const QByteArray &line) { capture(QString::fromUtf8(line)); });
            connect(runner, &EngineProcess::diagnostic, &dialog, capture);
            auto *timeout = new QTimer(runner); timeout->setSingleShot(true);
            connect(timeout, &QTimer::timeout, runner, &EngineProcess::cancel);
            connect(runner, &EngineProcess::completed, &dialog, [=](bool success, const QString &) {
                timeout->stop();
                QString result = success && !output->isEmpty() ? *output : QStringLiteral("无法运行，请检查路径");
                if (name == QStringLiteral("node") && success) {
                    const auto version = QVersionNumber::fromString(output->mid(output->startsWith(QLatin1Char('v')) ? 1 : 0));
                    result += version.majorVersion() >= 22 ? QStringLiteral("（满足要求）") : QStringLiteral("（需要 22 或更新版本）");
                }
                const QString previous = diagnosticResult->text();
                diagnosticResult->setText(previous + (previous.isEmpty() ? QString() : QStringLiteral("\n")) + name + QStringLiteral(": ") + result);
                if (--*remaining == 0) diagnose->setEnabled(true);
                runner->deleteLater();
            });
            timeout->start(5000);
            runner->start(DownloadCore::findTool(name, it.value()->text().trimmed()),
                {name == QStringLiteral("ffmpeg") || name == QStringLiteral("ffprobe") ? QStringLiteral("-version") : QStringLiteral("--version")});
        }
    });
    auto *links = new QLabel(QStringLiteral("<a href='https://github.com/yt-dlp/yt-dlp/releases'>yt-dlp 官方发布</a>　"
        "<a href='https://ffmpeg.org/download.html'>FFmpeg 下载</a>　<a href='https://nodejs.org/'>Node.js</a>"));
    links->setOpenExternalLinks(true); layout->addWidget(links);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消")); layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        for (auto it = edits.cbegin(); it != edits.cend(); ++it) {
            const QString path = it.value()->text().trimmed();
            if (!path.isEmpty() && DownloadCore::findTool(it.key(), path).isEmpty()) {
                QMessageBox::warning(&dialog, QStringLiteral("路径无效"), it.key() + QStringLiteral(" 路径不是可执行文件。")); return;
            }
        }
        const QString proxyText = proxy->text().trimmed(); const QUrl address(proxyText);
        if (network->currentData().toString() == QStringLiteral("custom") && (proxyText.isEmpty() || !address.isValid() || address.host().isEmpty() || !address.userInfo().isEmpty()
            || !QStringList{QStringLiteral("http"), QStringLiteral("https"), QStringLiteral("socks5"), QStringLiteral("socks5h")}.contains(address.scheme()))) {
            QMessageBox::warning(&dialog, QStringLiteral("代理地址无效"), QStringLiteral("请输入 http、https、socks5 或 socks5h 地址；当前版本不保存带账号密码的代理。")); return;
        }
        if (sessionMode->currentData().toString() == QStringLiteral("file")) {
            const auto error = DownloadCore::validateCookiesFile(cookies->text().trimmed());
            if (!error.isEmpty()) { QMessageBox::warning(&dialog, QStringLiteral("Cookies 文件无效"), error); return; }
        }
        auto settings = localSettings();
        for (auto it = edits.cbegin(); it != edits.cend(); ++it) settings.setValue(QStringLiteral("tools/") + it.key(), it.value()->text().trimmed());
        settings.setValue(QStringLiteral("proxy"), proxyText); settings.sync();
        settings.setValue(QStringLiteral("networkMode"), network->currentData());
        settings.setValue(QStringLiteral("sessionMode"), sessionMode->currentData());
        settings.setValue(QStringLiteral("cookiesFile"), cookies->text().trimmed());
        settings.setValue(QStringLiteral("browser"), browser->currentText());
        settings.setValue(QStringLiteral("browserProfile"), profile->text().trimmed()); settings.sync();
        if (settings.status() != QSettings::NoError) {
            QMessageBox::warning(&dialog, QStringLiteral("设置未保存"), QStringLiteral("无法写入配置目录，请检查当前用户的数据目录权限。")); return;
        }
        dialog.accept();
    });
    dialog.exec(); refreshDependencies();
}
DownloadCore::Options MainWindow::connectionOptions() const {
    auto settings = localSettings(); DownloadCore::Options result;
    const auto proxy = settings.value(QStringLiteral("proxy")).toString();
    result.networkMode = settings.value(QStringLiteral("networkMode"), proxy.isEmpty() ? QStringLiteral("system") : QStringLiteral("custom")).toString();
    if (result.networkMode == QStringLiteral("custom")) result.proxy = proxy;
    result.node = tool(QStringLiteral("node"));
    const auto session = settings.value(QStringLiteral("sessionMode"), QStringLiteral("none")).toString();
    if (session == QStringLiteral("file")) result.cookiesFile = settings.value(QStringLiteral("cookiesFile")).toString();
    else if (session == QStringLiteral("browser")) {
        result.browser = settings.value(QStringLiteral("browser"), QStringLiteral("chrome")).toString();
        const auto profile = settings.value(QStringLiteral("browserProfile")).toString();
        if (!profile.isEmpty()) result.browser += QLatin1Char(':') + profile;
    }
    return result;
}
QString MainWindow::connectionError(const DownloadCore::Options &options) const {
    if (localSettings().value(QStringLiteral("sessionMode")).toString() == QStringLiteral("file"))
        return DownloadCore::validateCookiesFile(options.cookiesFile);
    if (options.networkMode == QStringLiteral("custom") && options.proxy.isEmpty()) return QStringLiteral("指定代理模式下需要填写代理地址。");
    return {};
}
QString MainWindow::historyPath() const {
    return QDir(dataDirectory).filePath(QStringLiteral("tasks.json"));
}
QSettings MainWindow::localSettings() const {
    return QSettings(QDir(dataDirectory).filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
}
void MainWindow::saveTasks() {
    QJsonArray array;
    // Bound history while preserving the active queue and its row indices in memory.
    const int from = std::max(0, int(tasks.size()) - 500);
    for (int i = from; i < tasks.size(); ++i) {
        const auto &t = tasks[i];
        array.append(QJsonObject{{QStringLiteral("title"), t.title}, {QStringLiteral("url"), t.options.url},
            {QStringLiteral("directory"), t.options.directory}, {QStringLiteral("state"), t.state},
            {QStringLiteral("file"), t.file}, {QStringLiteral("error"), t.error}, {QStringLiteral("percent"), t.percent},
            {QStringLiteral("container"), t.options.container}, {QStringLiteral("height"), t.options.height},
            {QStringLiteral("audio"), t.options.audioOnly}, {QStringLiteral("subtitles"), t.options.subtitles}});
    }
    const auto path = historyPath(); QDir().mkpath(QFileInfo(path).absolutePath()); QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(array).toJson()) < 0 || !file.commit())
        log(QStringLiteral("任务记录保存失败：") + file.errorString());
}
void MainWindow::loadTasks() {
    QFile file(historyPath()); if (!file.open(QIODevice::ReadOnly)) return;
    if (file.size() > 8 * 1024 * 1024) { log(QStringLiteral("任务记录过大，已跳过加载。")); return; }
    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) { log(QStringLiteral("任务记录损坏，已跳过加载。")); return; }
    for (const auto &value : doc.array()) {
        if (tasks.size() >= 500) break;
        const auto o = value.toObject(); DownloadTask t;
        t.options.url = o.value(QStringLiteral("url")).toString();
        if (!DownloadCore::validUrl(t.options.url)) continue;
        t.title = o.value(QStringLiteral("title")).toString(); t.state = o.value(QStringLiteral("state")).toString();
        t.file = o.value(QStringLiteral("file")).toString(); t.error = o.value(QStringLiteral("error")).toString();
        t.percent = std::clamp(o.value(QStringLiteral("percent")).toInt(), 0, 100);
        t.options.directory = o.value(QStringLiteral("directory")).toString();
        t.options.container = o.value(QStringLiteral("container")).toString() == QStringLiteral("mkv") ? QStringLiteral("mkv") : QStringLiteral("mp4");
        t.options.height = std::max(0, o.value(QStringLiteral("height")).toInt());
        t.options.audioOnly = o.value(QStringLiteral("audio")).toBool();
        t.options.subtitles = o.value(QStringLiteral("subtitles")).toString();
        if (t.state != QStringLiteral("已完成") && t.state != QStringLiteral("失败")) {
            t.state = QStringLiteral("已停止"); t.error = QStringLiteral("程序重启后已停止，可手动重试继续下载。");
        }
        tasks.append(t); updateRow(tasks.size() - 1);
    }
}
void MainWindow::closeEvent(QCloseEvent *event) {
    const bool busy = activeRow >= 0 || (parser && parser->running());
    if (busy && QMessageBox::question(this, QStringLiteral("退出程序"), QStringLiteral("退出将停止当前任务，并保留临时文件。是否退出？"), QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) { event->ignore(); return; }
    closing = true;
    if (activeRow >= 0) tasks[activeRow].state = QStringLiteral("已停止");
    saveTasks();
    // Disconnect UI callbacks before synchronous teardown to avoid re-entering a partially destroyed window.
    if (parser) { parser->disconnect(this); delete parser; parser = nullptr; }
    if (downloader) { downloader->disconnect(this); delete downloader; downloader = nullptr; }
    event->accept();
}
