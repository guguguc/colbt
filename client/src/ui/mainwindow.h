#pragma once

#include <QHash>
#include <QMainWindow>

#include "app/appcontext.h"

class SessionListWidget;
class ContactListWidget;
class ChatPanel;
class QStackedWidget;
class QLabel;
class QToolButton;

// 主窗口：左侧导航 + 会话/联系人 + 聊天面板
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(AppContext* ctx, QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void showSessionsView();
    void showContactsView();
    void onSessionClicked(const QtSession& session);
    void onOpenChat(qint64 targetId, int targetType, const QString& title);
    void onSendRequested(qint64 targetId, int targetType, qint64 replyToId, const QString& text);
    void onSearchClicked();
    void onAddFriendClicked();
    void onCreateGroupClicked();
    void onProfileClicked();
    void onLogout();

    // AppContext 信号
    void onContactsReady(const QVector<QtBuddy>& buddies, const QVector<QtGroup>& groups);
    void onSessionsReady(const QVector<QtSession>& sessions);
    void onHistoryReady(qint64 targetId, int targetType, const QVector<QtMessage>& msgs);
    void onMessageArrived(const QtMessage& msg);
    void onMessageSent(const QtMessage& msg);
    void onMessageRecalled(qint64 msgId, qint64 targetId, int targetType);
    void onReadReceipt(qint64 peerId, int targetType);
    void onFriendDeleted(qint64 friendId, const QString& name);
    void onGroupUpdated(qint64 groupId);
    void onSearchResults(const QVector<QtMessage>& msgs);
    void onTyping(int64_t fromId, qint64 targetId, int targetType);
    void onPresenceChanged(qint64 userId, bool online);
    void onFriendAdded(const QtBuddy& buddy);
    void onGroupCreated(const QtGroup& group);
    void onGroupMembersReady(qint64 groupId, const QVector<QtMember>& members);
    void onError(int code, const QString& msg);
    void onProfileUpdated(int code, const QString& msg, const QtUser& me);
    void onProfileChanged(qint64 userId, const QString& nickname, const QString& avatar);

signals:
    void loggedOut();
    void profileUpdatedSignal(int code, const QString& msg, const QtUser& me);

private:
    QString titleFor(qint64 targetId, int targetType) const;
    QString avatarFor(qint64 targetId, int targetType) const;
    void ensureSessionItem(const QtMessage& msg);
    void refreshOnlineStatus();
    void showSearchDialog(const QVector<QtMessage>& results);
    void openPeerFromMessage(const QtMessage& msg);

    AppContext* ctx_;
    SessionListWidget* sessionList_;
    ContactListWidget* contactList_;
    ChatPanel* chatPanel_;
    QStackedWidget* listStack_;
    QToolButton* msgNav_;
    QToolButton* contactNav_;
    QToolButton* profileBtn_;
    QLabel* myAvatarLabel_;
    QLabel* titleLabel_;
    QWidget* contactToolbar_;

    QHash<qint64, QtBuddy> buddyById_;
    QHash<qint64, QtGroup> groupById_;
    QVector<QtSession> sessions_;
    qint64 activeTargetId_ = -1;
    int activeTargetType_ = 0;
};
