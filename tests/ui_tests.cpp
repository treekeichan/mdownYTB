#include "mainwindow.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include "engineprocess.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QSettings>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QTimer>
#include <QLabel>

class UiTests final : public QObject {
    Q_OBJECT
    QTemporaryDir directory{QDir::currentPath() + QStringLiteral("/ui-tests-XXXXXX")};
    QPushButton *button(MainWindow &window, const QString &text) {
        for (auto *b : window.findChildren<QPushButton *>()) if (b->text() == text) return b;
        return nullptr;
    }
private slots:
    void settingsAndVerificationRecovery() {
        const QString cookiePath = directory.filePath(QStringLiteral("session cookies.txt"));
        QFile cookieFile(cookiePath); QVERIFY(cookieFile.open(QIODevice::WriteOnly));
        cookieFile.write("# Netscape HTTP Cookie File\n.youtube.com\tTRUE\t/\tTRUE\t0\tSESSION\ttest-secret\n"); cookieFile.close();
        MainWindow window(directory.path()); window.show();
        auto *url = window.findChild<QLineEdit *>(QStringLiteral("urlInput"));
        auto *path = window.findChild<QLineEdit *>(QStringLiteral("directoryInput"));
        auto *add = button(window, QStringLiteral("加入下载队列"));
        auto *table = window.findChild<QTableWidget *>(QStringLiteral("taskTable"));
        url->setText(QStringLiteral("https://youtu.be/verify-session")); path->setText(directory.path());
        button(window, QStringLiteral("解析链接"))->click();
        QTRY_COMPARE_WITH_TIMEOUT(window.findChild<QLabel *>(QStringLiteral("videoTitle"))->text(), QStringLiteral("需要平台验证"), 5000);
        QVERIFY(window.findChild<QWidget *>(QStringLiteral("recoveryActions"))->isVisible()); QVERIFY(!add->isEnabled());
        QTimer::singleShot(0, &window, [&] {
            auto *dialog = window.findChild<QDialog *>(); QVERIFY(dialog);
            auto *mode = dialog->findChild<QComboBox *>(QStringLiteral("sessionMode"));
            auto *network = dialog->findChild<QComboBox *>(QStringLiteral("networkMode"));
            mode->setCurrentIndex(mode->findData(QStringLiteral("file")));
            network->setCurrentIndex(network->findData(QStringLiteral("direct")));
            dialog->findChild<QLineEdit *>(QStringLiteral("cookiesInput"))->setText(cookiePath);
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Save)->click();
        });
        button(window, QStringLiteral("配置 Cookies / 网络"))->click();
        button(window, QStringLiteral("重新解析"))->click(); QTRY_VERIFY_WITH_TIMEOUT(add->isEnabled(), 5000);
        QVERIFY(!window.findChild<QWidget *>(QStringLiteral("recoveryActions"))->isVisible());
        add->click(); QTRY_COMPARE_WITH_TIMEOUT(table->item(0, 1)->text(), QStringLiteral("已完成"), 5000);
        QFile history(directory.filePath(QStringLiteral("tasks.json"))); QVERIFY(history.open(QIODevice::ReadOnly));
        const auto savedHistory = history.readAll(); history.close();
        QVERIFY(!savedHistory.contains("test-secret")); QVERIFY(!savedHistory.contains("session cookies.txt"));
        QVERIFY(!window.findChild<QPlainTextEdit *>()->toPlainText().contains(QStringLiteral("test-secret")));
        window.close();
        QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        QCOMPARE(settings.value(QStringLiteral("networkMode")).toString(), QStringLiteral("direct"));
        QCOMPARE(settings.value(QStringLiteral("cookiesFile")).toString(), cookiePath);
        settings.setValue(QStringLiteral("sessionMode"), QStringLiteral("none")); settings.sync();
        QFile::remove(directory.filePath(QStringLiteral("tasks.json")));
    }
    void initTestCase() {
        QVERIFY(directory.isValid());
        QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyh.ttc"));
        QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/segoeui.ttf"));
        QCoreApplication::setOrganizationName(QStringLiteral("mdownYTB-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("ui-tests"));
        QStandardPaths::setTestModeEnabled(true);
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
        const QString fake = QCoreApplication::applicationDirPath() + QStringLiteral("/fake_engine.exe");
        QSettings settings(directory.filePath(QStringLiteral("settings.ini")), QSettings::IniFormat);
        for (const auto &name : {QStringLiteral("yt-dlp"), QStringLiteral("ffmpeg"), QStringLiteral("ffprobe"), QStringLiteral("node")})
            settings.setValue(QStringLiteral("tools/") + name, fake);
        settings.sync(); QCOMPARE(settings.status(), QSettings::NoError);
    }
    void parseDownloadAndRestoreHistory() {
        const QString history = directory.filePath(QStringLiteral("tasks.json"));
        // Tests use their own application identity and never touch the user's normal history.
        QFile::remove(history);
        {
            MainWindow window(directory.path()); window.show();
            auto *url = window.findChild<QLineEdit *>(QStringLiteral("urlInput"));
            auto *path = window.findChild<QLineEdit *>(QStringLiteral("directoryInput"));
            auto *table = window.findChild<QTableWidget *>(QStringLiteral("taskTable"));
            auto *parse = button(window, QStringLiteral("解析链接"));
            auto *add = button(window, QStringLiteral("加入下载队列"));
            QVERIFY(url && path && table && parse && add); QVERIFY(!add->isEnabled());
            url->setText(QStringLiteral("https://youtu.be/test")); path->setText(directory.path()); parse->click();
            QTRY_VERIFY2_WITH_TIMEOUT(add->isEnabled(), qPrintable(window.findChild<QPlainTextEdit *>()->toPlainText()), 5000);
            add->click(); QCOMPARE(table->rowCount(), 1);
            QTRY_COMPARE_WITH_TIMEOUT(table->item(0, 1)->text(), QStringLiteral("已完成"), 10000);
            QVERIFY(QFile::exists(directory.filePath(QStringLiteral("中文测试视频 [test].mp4"))));
            QFile file(history); QVERIFY(file.open(QIODevice::ReadOnly));
            const auto tasks = QJsonDocument::fromJson(file.readAll()).array();
            file.close();
            QCOMPARE(tasks.size(), 1); QCOMPARE(tasks[0].toObject().value(QStringLiteral("percent")).toInt(), 100);
            url->setText(QStringLiteral("https://youtu.be/other")); QVERIFY(!add->isEnabled());
            parse->click(); QTRY_VERIFY_WITH_TIMEOUT(add->isEnabled(), 5000);
            add->click(); QCOMPARE(table->rowCount(), 2);
            button(window, QStringLiteral("清空历史记录"))->click();
            QCOMPARE(table->rowCount(), 1);
            QVERIFY(QFile::exists(directory.filePath(QStringLiteral("中文测试视频 [test].mp4"))));
            QTest::qWait(80);
            button(window, QStringLiteral("停止选中"))->click();
            QTRY_VERIFY_WITH_TIMEOUT(window.findChildren<EngineProcess *>().isEmpty(), 5000);
            QCOMPARE(table->item(0, 1)->text(), QStringLiteral("已停止"));
            button(window, QStringLiteral("重试选中"))->click();
            QTRY_COMPARE_WITH_TIMEOUT(table->item(0, 1)->text(), QStringLiteral("已完成"), 10000);
            window.close();
        }
        {
            MainWindow restored(directory.path());
            auto *table = restored.findChild<QTableWidget *>(QStringLiteral("taskTable"));
            QCOMPARE(table->rowCount(), 1); QCOMPARE(table->item(0, 1)->text(), QStringLiteral("已完成"));
            button(restored, QStringLiteral("清空历史记录"))->click();
            QCOMPARE(table->rowCount(), 0);
            QVERIFY(QFile::exists(directory.filePath(QStringLiteral("中文测试视频 [test].mp4"))));
        }
        MainWindow empty(directory.path());
        QCOMPARE(empty.findChild<QTableWidget *>(QStringLiteral("taskTable"))->rowCount(), 0);
        QFile::remove(history);
    }
};
QTEST_MAIN(UiTests)
#include "ui_tests.moc"
