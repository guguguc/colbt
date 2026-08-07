#include "ui/contactlistwidget.h"

#include <QContextMenuEvent>
#include <QHeaderView>
#include <QMenu>
#include <QPainter>
#include <QPixmap>

#include "ui/avatar.h"

namespace {
const int kAvatarSize = 34;
}

ContactListWidget::ContactListWidget(QWidget* parent) : QTreeWidget(parent) {
    setHeaderHidden(true);
    setRootIsDecorated(false);
    setIndentation(8);
    setIconSize(QSize(kAvatarSize, kAvatarSize));
    setContextMenuPolicy(Qt::DefaultContextMenu);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setSelectionMode(QAbstractItemView::SingleSelection);

    friendsRoot_ = new QTreeWidgetItem(this);
    friendsRoot_->setText(0, QStringLiteral("我的好友"));
    friendsRoot_->setIcon(0, makeAvatar(QStringLiteral("友"), QString(), kAvatarSize - 6));

    groupsRoot_ = new QTreeWidgetItem(this);
    groupsRoot_->setText(0, QStringLiteral("我的群组"));
    groupsRoot_->setIcon(0, makeAvatar(QStringLiteral("群"), QString(), kAvatarSize - 6));

    connect(this, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem* item, int) { openForItem(item); });
    // 单击同样打开聊天（双击/单击都能用，避免双击时序问题）
    connect(this, &QTreeWidget::itemClicked, this,
            [this](QTreeWidgetItem* item, int) { openForItem(item); });
}

void ContactListWidget::openForItem(QTreeWidgetItem* item) {
    if (!item || item->parent() == nullptr) return;
    int64_t id = item->data(0, Qt::UserRole).toLongLong();
    int type = item->data(0, Qt::UserRole + 1).toInt();
    emit openChatRequested(id, type, item->text(0));
}

void ContactListWidget::rebuildFriendCount() {
    int n = friendsRoot_->childCount();
    friendsRoot_->setText(0, QStringLiteral("我的好友 (%1)").arg(n));
    int m = groupsRoot_->childCount();
    groupsRoot_->setText(0, QStringLiteral("我的群组 (%1)").arg(m));
}

void ContactListWidget::setBuddies(const QVector<QtBuddy>& buddies) {
    friendsRoot_->takeChildren();
    for (const auto& b : buddies) upsertBuddy(b);
    expandAll();
}

void ContactListWidget::setGroups(const QVector<QtGroup>& groups) {
    groupsRoot_->takeChildren();
    for (const auto& g : groups) upsertGroup(g);
    expandAll();
}

void ContactListWidget::upsertBuddy(const QtBuddy& buddy) {
    for (int i = 0; i < friendsRoot_->childCount(); ++i) {
        auto* it = friendsRoot_->child(i);
        if (it->data(0, Qt::UserRole).toLongLong() == buddy.user.id) {
            it->setText(0, buddy.user.nickname.isEmpty() ? buddy.user.username
                                                         : buddy.user.nickname);
            it->setData(0, Qt::UserRole, buddy.user.id);
            it->setData(0, Qt::UserRole + 1, 0);
            it->setIcon(0, makeAvatar(it->text(0), buddy.user.avatar, kAvatarSize));
            QFont f = it->font(0);
            f.setBold(false);
            it->setFont(0, f);
            if (!buddy.user.online) {
                QColor gray("#72767d");
                it->setForeground(0, gray);
            } else {
                it->setForeground(0, QColor("#dbdee1"));
            }
            rebuildFriendCount();
            return;
        }
    }
    auto* item = new QTreeWidgetItem(friendsRoot_);
    QString name = buddy.user.nickname.isEmpty() ? buddy.user.username : buddy.user.nickname;
    item->setText(0, name);
    item->setData(0, Qt::UserRole, buddy.user.id);
    item->setData(0, Qt::UserRole + 1, 0);
    item->setToolTip(0, buddy.user.username);
    item->setIcon(0, makeAvatar(name, buddy.user.avatar, kAvatarSize));
    if (!buddy.user.online) {
        item->setForeground(0, QColor("#72767d"));
    }
    rebuildFriendCount();
}

void ContactListWidget::upsertGroup(const QtGroup& group) {
    for (int i = 0; i < groupsRoot_->childCount(); ++i) {
        auto* it = groupsRoot_->child(i);
        if (it->data(0, Qt::UserRole).toLongLong() == group.id) {
            it->setText(0, group.name);
            it->setIcon(0, makeAvatar(group.name, QString(), kAvatarSize));
            rebuildFriendCount();
            return;
        }
    }
    auto* item = new QTreeWidgetItem(groupsRoot_);
    item->setText(0, group.name);
    item->setData(0, Qt::UserRole, group.id);
    item->setData(0, Qt::UserRole + 1, 1);
    item->setToolTip(0, QStringLiteral("成员 %1 人").arg(group.members.size()));
    item->setIcon(0, makeAvatar(group.name, QString(), kAvatarSize));
    rebuildFriendCount();
}

void ContactListWidget::setBuddyOnline(qint64 userId, bool online) {
    for (int i = 0; i < friendsRoot_->childCount(); ++i) {
        auto* it = friendsRoot_->child(i);
        if (it->data(0, Qt::UserRole).toLongLong() == userId) {
            if (online)
                it->setForeground(0, QColor("#dbdee1"));
            else
                it->setForeground(0, QColor("#72767d"));
            break;
        }
    }
}

void ContactListWidget::contextMenuEvent(QContextMenuEvent* event) {
    QTreeWidgetItem* item = itemAt(event->pos());
    if (item && item->parent()) {
        int type = item->data(0, Qt::UserRole + 1).toInt();
        int64_t id = item->data(0, Qt::UserRole).toLongLong();
        QMenu menu(this);
        if (type == 0) {
            // 好友菜单
            auto* chat = menu.addAction(QStringLiteral("发起聊天"));
            auto* del = menu.addAction(QStringLiteral("删除好友"));
            QAction* chosen = menu.exec(event->globalPos());
            if (chosen == chat) {
                emit openChatRequested(id, 0, item->text(0));
            } else if (chosen == del) {
                emit deleteFriendRequested(id, item->text(0));
            }
        } else {
            // 群组菜单
            auto* info = menu.addAction(QStringLiteral("群信息"));
            auto* rename = menu.addAction(QStringLiteral("修改群名"));
            auto* leave = menu.addAction(QStringLiteral("退出群聊"));
            auto* dismiss = menu.addAction(QStringLiteral("解散群"));
            QAction* chosen = menu.exec(event->globalPos());
            if (chosen == info) emit groupInfoRequested(id);
            else if (chosen == rename) emit renameGroupRequested(id);
            else if (chosen == leave) emit leaveGroupRequested(id);
            else if (chosen == dismiss) emit dismissGroupRequested(id);
        }
        return;
    }

    QMenu menu(this);
    auto* addAction = menu.addAction(QStringLiteral("添加好友…"));
    auto* createAction = menu.addAction(QStringLiteral("创建群聊…"));
    QAction* chosen = menu.exec(event->globalPos());
    if (chosen == addAction) emit addFriendRequested();
    else if (chosen == createAction) emit createGroupRequested();
}
