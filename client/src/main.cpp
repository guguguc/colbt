#include <QApplication>
#include <QFile>
#include <QFont>
#include <QFontDatabase>

#include "app/appcontext.h"
#include "ui/loginwindow.h"
#include "ui/mainwindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("IMClient"));

    QFont font = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    font.setPointSize(10);
    app.setFont(font);

    QFile qss(QStringLiteral(":/style.qss"));
    if (qss.open(QIODevice::ReadOnly)) {
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));
    }

    AppContext ctx;
    LoginWindow login(&ctx);
    login.show();

    QObject::connect(&login, &LoginWindow::loggedIn, &login, [&ctx, &login] {
        auto* mw = new MainWindow(&ctx);
        mw->setAttribute(Qt::WA_DeleteOnClose);
        QObject::connect(mw, &MainWindow::loggedOut, mw, &MainWindow::close);
        QObject::connect(mw, &QObject::destroyed, &login, [&login] { login.show(); });
        mw->show();
        login.hide();
    });

    return app.exec();
}
