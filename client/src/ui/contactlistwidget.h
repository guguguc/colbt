#pragma once

#include <QTreeWidget>

#include "app/appcontext.h"

// 联系人列表：好友 + 群组
class ContactListWidget : public QTreeWidget {
    Q_OBJECT

public:
    explicit ContactListWidget(QWidget* parent = nullptr);

    void setBuddies(const QVector<QtBuddy>& buddies);
    void setGroups(const QVector<QtGroup>& groups);
    void upsertBuddy(const QtBuddy& buddy);
    void upsertGroup(const QtGroup& group);
    void setBuddyOnline(qint64 userId, bool online);

signals:
    void openChatRequested(qint64 targetId, int targetType, const QString& title);
    void addFriendRequested();
    void createGroupRequested();
    void deleteFriendRequested(qint64 friendId, const QString& name);
    void groupInfoRequested(qint64 groupId);
    void renameGroupRequested(qint64 groupId);
    void dismissGroupRequested(qint64 groupId);
    void leaveGroupRequested(qint64 groupId);

protected:
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void openForItem(QTreeWidgetItem* item);
    void rebuildFriendCount();
    QTreeWidgetItem* friendsRoot_ = nullptr;
    QTreeWidgetItem* groupsRoot_ = nullptr;
};
