#include "ui/loginwindow.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "app/appcontext.h"

LoginWindow::LoginWindow(AppContext* ctx, QWidget* parent)
    : QWidget(parent), ctx_(ctx) {
    setWindowTitle(QStringLiteral("IM 客户端 - 登录"));
    setFixedSize(360, 460);
    setObjectName("loginWindow");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(36, 30, 36, 24);
    root->setSpacing(12);

    auto* logo = new QLabel(QStringLiteral("C O L B T"), this);
    logo->setObjectName("loginLogo");
    logo->setAlignment(Qt::AlignCenter);
    root->addWidget(logo);

    auto* subtitle = new QLabel(QStringLiteral("简单、高效的即时通讯"), this);
    subtitle->setObjectName("loginSubtitle");
    subtitle->setAlignment(Qt::AlignCenter);
    root->addWidget(subtitle);
    root->addSpacing(8);

    modeStack_ = new QStackedWidget(this);

    // ---- 登录页 ----
    auto* loginPage = new QWidget(this);
    auto* lv = new QVBoxLayout(loginPage);
    lv->setContentsMargins(0, 0, 0, 0);
    lv->setSpacing(10);

    userEdit_ = new QLineEdit(loginPage);
    userEdit_->setPlaceholderText(QStringLiteral("用户名"));
    lv->addWidget(userEdit_);

    passEdit_ = new QLineEdit(loginPage);
    passEdit_->setPlaceholderText(QStringLiteral("密码"));
    passEdit_->setEchoMode(QLineEdit::Password);
    lv->addWidget(passEdit_);

    loginBtn_ = new QPushButton(QStringLiteral("登 录"), loginPage);
    loginBtn_->setObjectName("primaryBtn");
    lv->addWidget(loginBtn_);

    auto* switchToReg = new QPushButton(QStringLiteral("没有账号？立即注册"), loginPage);
    switchToReg->setObjectName("linkBtn");
    lv->addWidget(switchToReg);

    auto* checkRow = new QHBoxLayout;
    checkRow->setSpacing(12);
    rememberCheck_ = new QCheckBox(QStringLiteral("记住密码"), loginPage);
    autoLoginCheck_ = new QCheckBox(QStringLiteral("自动登录"), loginPage);
    checkRow->addWidget(rememberCheck_);
    checkRow->addWidget(autoLoginCheck_);
    checkRow->addStretch();
    lv->addLayout(checkRow);

    modeStack_->addWidget(loginPage);

    // ---- 注册页 ----
    auto* regPage = new QWidget(this);
    auto* rv = new QVBoxLayout(regPage);
    rv->setContentsMargins(0, 0, 0, 0);
    rv->setSpacing(10);

    nickEdit_ = new QLineEdit(regPage);
    nickEdit_->setPlaceholderText(QStringLiteral("昵称"));
    rv->addWidget(nickEdit_);

    regUserEdit_ = new QLineEdit(regPage);
    regUserEdit_->setPlaceholderText(QStringLiteral("用户名（唯一）"));
    rv->addWidget(regUserEdit_);

    regPassEdit_ = new QLineEdit(regPage);
    regPassEdit_->setPlaceholderText(QStringLiteral("密码（至少4位）"));
    regPassEdit_->setEchoMode(QLineEdit::Password);
    rv->addWidget(regPassEdit_);

    registerBtn_ = new QPushButton(QStringLiteral("注 册"), regPage);
    registerBtn_->setObjectName("primaryBtn");
    rv->addWidget(registerBtn_);

    auto* switchToLogin = new QPushButton(QStringLiteral("已有账号？去登录"), regPage);
    switchToLogin->setObjectName("linkBtn");
    rv->addWidget(switchToLogin);

    modeStack_->addWidget(regPage);
    root->addWidget(modeStack_);

    // ---- 服务器配置 ----
    auto* hostRow = new QHBoxLayout;
    hostRow->setSpacing(6);
    hostEdit_ = new QLineEdit(QStringLiteral("127.0.0.1"), this);
    hostEdit_->setObjectName("smallEdit");
    portEdit_ = new QLineEdit(QStringLiteral("9000"), this);
    portEdit_->setObjectName("smallEdit");
    portEdit_->setFixedWidth(70);
    hostRow->addWidget(new QLabel(QStringLiteral("服务器"), this));
    hostRow->addWidget(hostEdit_, 1);
    hostRow->addWidget(portEdit_);
    root->addLayout(hostRow);

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName("statusLabel");
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setWordWrap(true);
    root->addWidget(statusLabel_);

    root->addStretch();

    // ---- 信号 ----
    connect(loginBtn_, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    connect(registerBtn_, &QPushButton::clicked, this, &LoginWindow::onRegisterClicked);
    connect(switchToReg, &QPushButton::clicked, this, [this] { switchMode(true); });
    connect(switchToLogin, &QPushButton::clicked, this, [this] { switchMode(false); });
    connect(passEdit_, &QLineEdit::returnPressed, this, &LoginWindow::onLoginClicked);
    connect(regPassEdit_, &QLineEdit::returnPressed, this, &LoginWindow::onRegisterClicked);

    connect(ctx_, &AppContext::loginResult, this, &LoginWindow::onLoginResult);
    connect(ctx_, &AppContext::registerResult, this, &LoginWindow::onRegisterResult);
    connect(ctx_, &AppContext::connectionChanged, this, &LoginWindow::onConnectionChanged);
    connect(ctx_, &AppContext::errorOccurred, this, &LoginWindow::onError);

    // 预连接
    ctx_->connectToServer(hostEdit_->text(), portEdit_->text().toInt());
    statusLabel_->setText(QStringLiteral("正在连接服务器…"));

    loadSavedCredentials();
}

void LoginWindow::loadSavedCredentials() {
    QSettings settings;
    QString host = settings.value("login/host").toString();
    int port = settings.value("login/port", 9000).toInt();
    if (!host.isEmpty()) hostEdit_->setText(host);
    portEdit_->setText(QString::number(port));
    QString user = settings.value("login/user").toString();
    if (!user.isEmpty()) userEdit_->setText(user);
    bool remembered = settings.value("login/remember", false).toBool();
    rememberCheck_->setChecked(remembered);
    if (remembered) {
        passEdit_->setText(QString::fromUtf8(
            QByteArray::fromBase64(settings.value("login/pass_b64").toString().toUtf8())));
    }
    autoLoginCheck_->setChecked(settings.value("login/auto", false).toBool());
    if (autoLoginCheck_->isChecked() && !userEdit_->text().isEmpty()) {
        QTimer::singleShot(400, this, &LoginWindow::autoLogin);
    }
}

void LoginWindow::saveCredentials() {
    QSettings settings;
    settings.setValue("login/host", hostEdit_->text());
    settings.setValue("login/port", portEdit_->text().toInt());
    settings.setValue("login/user", userEdit_->text().trimmed());
    bool remember = rememberCheck_->isChecked();
    settings.setValue("login/remember", remember);
    settings.setValue("login/auto", autoLoginCheck_->isChecked());
    if (remember) {
        settings.setValue("login/pass_b64",
                          QString::fromLatin1(passEdit_->text().toUtf8().toBase64()));
    } else {
        settings.remove("login/pass_b64");
    }
}

void LoginWindow::autoLogin() {
    onLoginClicked();
}

void LoginWindow::switchMode(bool toRegister) {
    registering_ = toRegister;
    modeStack_->setCurrentIndex(toRegister ? 1 : 0);
    statusLabel_->clear();
}

void LoginWindow::onLoginClicked() {
    statusLabel_->setText(QStringLiteral("登录中…"));
    loginBtn_->setEnabled(false);
    saveCredentials();
    ctx_->connectToServer(hostEdit_->text(), portEdit_->text().toInt());
    ctx_->login(userEdit_->text().trimmed(), passEdit_->text());
}

void LoginWindow::onRegisterClicked() {
    statusLabel_->setText(QStringLiteral("注册中…"));
    registerBtn_->setEnabled(false);
    ctx_->connectToServer(hostEdit_->text(), portEdit_->text().toInt());
    ctx_->registerUser(regUserEdit_->text().trimmed(), regPassEdit_->text(),
                       nickEdit_->text().trimmed());
}

void LoginWindow::onLoginResult(int code, const QString& msg, const QtUser& me) {
    loginBtn_->setEnabled(true);
    if (code == 0) {
        Q_UNUSED(me)
        emit loggedIn();
    } else {
        statusLabel_->setText(QStringLiteral("登录失败：") + msg);
    }
}

void LoginWindow::onRegisterResult(int code, const QString& msg) {
    registerBtn_->setEnabled(true);
    statusLabel_->setText(msg);
    if (code == 0) {
        switchMode(false);
        userEdit_->setText(regUserEdit_->text().trimmed());
        passEdit_->clear();
        statusLabel_->setText(QStringLiteral("注册成功，请登录"));
    }
}

void LoginWindow::onConnectionChanged(bool connected) {
    if (connected)
        statusLabel_->setText(QStringLiteral("已连接服务器"));
    else if (!loginBtn_->isEnabled() && statusLabel_->text() == QStringLiteral("登录中…"))
        statusLabel_->setText(QStringLiteral("无法连接服务器"));
}

void LoginWindow::onError(int code, const QString& msg) {
    Q_UNUSED(code)
    statusLabel_->setText(msg);
    loginBtn_->setEnabled(true);
    registerBtn_->setEnabled(true);
}
