#include "mainwindow.h"
#include <QApplication>
#include <QDir>
#include <QTimer>
#include <QStyleFactory>
#include <QFontDatabase>
#include <QFont>
#include <QPushButton>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("mdownYTB"));
    app.setApplicationName(QStringLiteral("mdownYTB"));
    app.setApplicationVersion(QStringLiteral("0.2.2"));
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    if (QApplication::platformName() == QStringLiteral("offscreen")) {
        QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyh.ttc"));
        QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/segoeui.ttf"));
    }
    const bool smokeTest = app.arguments().contains(QStringLiteral("--smoke-test"));
    MainWindow window(smokeTest ? QDir::currentPath() + QStringLiteral("/build/smoke-profile") : QString());
    window.show();
    if (smokeTest) {
        if (app.arguments().contains(QStringLiteral("--smoke-settings"))) {
            QTimer::singleShot(0, &window, [&window] {
                for (auto *button : window.findChildren<QPushButton *>()) {
                    if (button->text() == QStringLiteral("设置与依赖")) { button->click(); break; }
                }
            });
        }
        QTimer::singleShot(300, &app, [&] {
            const QString path = QDir::current().filePath(QStringLiteral("smoke-window.png"));
            auto *modal = QApplication::activeModalWidget();
            const bool saved = (modal ? modal->grab() : window.grab()).save(path);
            app.exit(saved ? 0 : 1);
        });
    }
    return app.exec();
}
