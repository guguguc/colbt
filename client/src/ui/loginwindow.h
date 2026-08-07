#pragma once

#include <QWidget>

class AppContext;

class QLineEdit;
class QPushButton;
class QStackedWidget;
class QLabel;
class QCheckBox;

// 登录窗口
class LoginWindow : public QWidget {
    Q_OBJECT

public:
    explicit LoginWindow(AppContext* ctx, QWidget* parent = nullptr);

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void switchMode(bool toRegister);
    void onLoginResult(int code, const QString& msg, const struct QtUser& me);
    void onRegisterResult(int code, const QString& msg);
    void onConnectionChanged(bool connected);
    void onError(int code, const QString& msg);
    void autoLogin();

signals:
    void loggedIn();

private:
    void loadSavedCredentials();
    void saveCredentials();

    AppContext* ctx_;
    QLineEdit* hostEdit_;
    QLineEdit* portEdit_;
    QLineEdit* userEdit_;
    QLineEdit* passEdit_;
    QLineEdit* regUserEdit_;
    QLineEdit* regPassEdit_;
    QLineEdit* nickEdit_;
    QPushButton* loginBtn_;
    QPushButton* registerBtn_;
    QLabel* statusLabel_;
    QStackedWidget* modeStack_;
    QCheckBox* rememberCheck_;
    QCheckBox* autoLoginCheck_;
    bool registering_ = false;
};
