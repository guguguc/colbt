#include "ui/mainwindow.h"

#include <QAbstractItemView>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "ui/avatar.h"
#include "ui/chatpanel.h"
#include "ui/contactlistwidget.h"
#include "ui/sessionlistwidget.h"

namespace {
const int kRailWidth = 72;
const int kListWidth = 240;
}

MainWindow::MainWindow(AppContext* ctx, QWidget* parent)
    : QMainWindow(parent), ctx_(ctx) {
    setWindowTitle(QStringLiteral("IM 客户端"));
    resize(1000, 640);
    setMinimumSize(860, 560);

    auto* central = new QWidget(this);
    central->setObjectName("mainCentral");
    auto* rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ---- 左侧导航栏 ----
    auto* rail = new QWidget(central);
    rail->setObjectName("rail");
    rail->setFixedWidth(kRailWidth);
    auto* railLayout = new QVBoxLayout(rail);
    railLayout->setContentsMargins(0, 10, 0, 10);
    railLayout->setSpacing(6);

    myAvatarLabel_ = new QLabel(rail);
    myAvatarLabel_->setObjectName("myAvatar");
    myAvatarLabel_->setFixedSize(40, 40);
    myAvatarLabel_->setPixmap(makeAvatar(ctx_->me().nickname, QString(), 40));
    railLayout->addWidget(myAvatarLabel_, 0, Qt::AlignHCenter);

    msgNav_ = new QToolButton(rail);
    msgNav_->setText(QStringLiteral("消息"));
    msgNav_->setObjectName("navBtn");
    msgNav_->setCheckable(true);
    msgNav_->setChecked(true);
    msgNav_->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    msgNav_->setCursor(Qt::PointingHandCursor);
    railLayout->addWidget(msgNav_);

    contactNav_ = new QToolButton(rail);
    contactNav_->setText(QStringLiteral("好友"));
    contactNav_->setObjectName("navBtn");
    contactNav_->setCheckable(true);
    contactNav_->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    contactNav_->setCursor(Qt::PointingHandCursor);
    railLayout->addWidget(contactNav_);

    railLayout->addStretch();

    profileBtn_ = new QToolButton(rail);
    profileBtn_->setText(QStringLiteral("资料"));
    profileBtn_->setObjectName("navBtn");
    profileBtn_->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    profileBtn_->setCursor(Qt::PointingHandCursor);
    railLayout->addWidget(profileBtn_);

    auto* logoutBtn = new QToolButton(rail);
    logoutBtn->setText(QStringLiteral("退出"));
    logoutBtn->setObjectName("navBtn");
    logoutBtn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    logoutBtn->setCursor(Qt::PointingHandCursor);
    railLayout->addWidget(logoutBtn);

    rootLayout->addWidget(rail);

    // ---- 中间列表区 ----
    auto* listWrap = new QWidget(central);
    listWrap->setObjectName("listWrap");
    listWrap->setFixedWidth(kListWidth);
    auto* listLayout = new QVBoxLayout(listWrap);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(0);

    titleLabel_ = new QLabel(QStringLiteral("消息"), listWrap);
    titleLabel_->setObjectName("listTitle");
    titleLabel_->setFixedHeight(44);
    auto* titleRow = new QWidget(listWrap);
    titleRow->setFixedHeight(48);
    auto* titleLayout = new QHBoxLayout(titleRow);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(0);
    conversationSearch_ = new QLineEdit(titleRow);
    conversationSearch_->setObjectName("conversationSearch");
    conversationSearch_->setPlaceholderText(QStringLiteral("寻找或开始新的对话"));
    conversationSearch_->setClearButtonEnabled(true);
    conversationSearch_->setFixedHeight(30);
    conversationSearch_->setToolTip(QStringLiteral("回车搜索消息"));
    titleLayout->addWidget(conversationSearch_, 1);
    titleLabel_->hide();
    auto* searchBtn = new QToolButton(titleRow);
    searchBtn->setText(QStringLiteral("🔍"));
    searchBtn->setObjectName("emojiBtn");
    searchBtn->setToolTip(QStringLiteral("搜索消息"));
    searchBtn->setCursor(Qt::PointingHandCursor);
    connect(searchBtn, &QToolButton::clicked, this, &MainWindow::onSearchClicked);
    connect(conversationSearch_, &QLineEdit::returnPressed, this, &MainWindow::onSearchClicked);
    titleLayout->addWidget(searchBtn);
    listLayout->addWidget(titleRow);

    // 联系人视图顶部的操作按钮（添加好友 / 创建群聊）
    contactToolbar_ = new QWidget(listWrap);
    contactToolbar_->setObjectName("contactToolbar");
    contactToolbar_->setFixedHeight(40);
    auto* toolbarLayout = new QHBoxLayout(contactToolbar_);
    toolbarLayout->setContentsMargins(12, 6, 12, 6);
    toolbarLayout->setSpacing(8);
    auto* addFriendBtn = new QPushButton(QStringLiteral("＋ 添加好友"), contactToolbar_);
    addFriendBtn->setObjectName("toolBtn");
    addFriendBtn->setCursor(Qt::PointingHandCursor);
    auto* createGroupBtn = new QPushButton(QStringLiteral("＋ 创建群聊"), contactToolbar_);
    createGroupBtn->setObjectName("toolBtn");
    createGroupBtn->setCursor(Qt::PointingHandCursor);
    toolbarLayout->addWidget(addFriendBtn);
    toolbarLayout->addWidget(createGroupBtn);
    toolbarLayout->addStretch();
    contactToolbar_->hide();
    listLayout->addWidget(contactToolbar_);

    sessionList_ = new SessionListWidget(listWrap);
    contactList_ = new ContactListWidget(listWrap);
    listStack_ = new QStackedWidget(listWrap);
    listStack_->addWidget(sessionList_);
    listStack_->addWidget(contactList_);
    listLayout->addWidget(listStack_, 1);
    rootLayout->addWidget(listWrap);

    // ---- 聊天面板 ----
    chatPanel_ = new ChatPanel(ctx_, central);
    rootLayout->addWidget(chatPanel_, 1);

    setCentralWidget(central);

    // ---- 信号连接 ----
    connect(msgNav_, &QToolButton::clicked, this, &MainWindow::showSessionsView);
    connect(contactNav_, &QToolButton::clicked, this, &MainWindow::showContactsView);
    connect(logoutBtn, &QToolButton::clicked, this, &MainWindow::onLogout);
    connect(profileBtn_, &QToolButton::clicked, this, &MainWindow::onProfileClicked);

    connect(sessionList_, &SessionListWidget::sessionClicked, this, &MainWindow::onSessionClicked);
    connect(contactList_, &ContactListWidget::openChatRequested, this, &MainWindow::onOpenChat);
    connect(contactList_, &ContactListWidget::addFriendRequested, this,
            &MainWindow::onAddFriendClicked);
    connect(contactList_, &ContactListWidget::createGroupRequested, this,
            &MainWindow::onCreateGroupClicked);
    connect(contactList_, &ContactListWidget::deleteFriendRequested, this,
            [this](qint64 friendId, const QString& name) {
                if (QMessageBox::question(this, QStringLiteral("删除好友"),
                                          QStringLiteral("确定删除好友 %1 吗？").arg(name)) ==
                    QMessageBox::Yes) {
                    ctx_->deleteFriend(friendId);
                }
            });
    connect(contactList_, &ContactListWidget::groupInfoRequested, this,
            [this](qint64 groupId) {
                if (groupById_.contains(groupId)) {
                    auto g = groupById_.value(groupId);
                    onGroupMembersReady(groupId, g.members);
                }
                ctx_->loadGroupMembers(groupId);
            });
    connect(contactList_, &ContactListWidget::renameGroupRequested, this,
            [this](qint64 groupId) {
                bool ok = false;
                QString name = QInputDialog::getText(this, QStringLiteral("修改群名"),
                                                     QStringLiteral("群名称："), QLineEdit::Normal,
                                                     groupById_.value(groupId).name, &ok);
                if (ok && !name.trimmed().isEmpty()) ctx_->renameGroup(groupId, name.trimmed());
            });
    connect(contactList_, &ContactListWidget::dismissGroupRequested, this,
            [this](qint64 groupId) {
                if (QMessageBox::question(this, QStringLiteral("解散群"),
                                          QStringLiteral("确定解散该群吗？此操作不可恢复。")) ==
                    QMessageBox::Yes) {
                    ctx_->dismissGroup(groupId);
                }
            });
    connect(contactList_, &ContactListWidget::leaveGroupRequested, this,
            [this](qint64 groupId) {
                if (QMessageBox::question(this, QStringLiteral("退出群聊"),
                                          QStringLiteral("确定退出该群吗？")) == QMessageBox::Yes) {
                    ctx_->leaveGroup(groupId);
                }
            });
    connect(addFriendBtn, &QPushButton::clicked, this, &MainWindow::onAddFriendClicked);
    connect(createGroupBtn, &QPushButton::clicked, this, &MainWindow::onCreateGroupClicked);
    connect(chatPanel_, &ChatPanel::sendRequested, this, &MainWindow::onSendRequested);
    connect(chatPanel_, &ChatPanel::typingRequested, this,
            [this](qint64 id, int type) { ctx_->sendTyping(id, type); });
    connect(chatPanel_, &ChatPanel::recallMessageRequested, this,
            [this](qint64 msgId, qint64 targetId, int targetType) {
                if (QMessageBox::question(this, QStringLiteral("撤回消息"),
                                          QStringLiteral("确定要撤回这条消息吗？")) ==
                    QMessageBox::Yes) {
                    ctx_->recallMessage(msgId, targetId, targetType);
                }
            });
    connect(chatPanel_, &ChatPanel::sendImageRequested, this,
            [this](qint64 id, int type, const QString& path) { ctx_->sendFile(id, type, 1, path); });
    connect(chatPanel_, &ChatPanel::sendFileRequested, this,
            [this](qint64 id, int type, const QString& path) { ctx_->sendFile(id, type, 2, path); });
    connect(chatPanel_, &ChatPanel::downloadFileRequested, this,
            [this](const QString& fileId) { ctx_->downloadFile(fileId); });
    connect(chatPanel_, &ChatPanel::saveFileData, this,
            [this](const QString& name, const QByteArray& data) {
                QString path = QFileDialog::getSaveFileName(this, QStringLiteral("保存文件"), name);
                if (path.isEmpty()) return;
                QFile f(path);
                if (f.open(QIODevice::WriteOnly)) {
                    f.write(data);
                    f.close();
                }
            });
    connect(chatPanel_, &ChatPanel::groupMembersRequested, this,
            [this](qint64 groupId) { ctx_->loadGroupMembers(groupId); });
    connect(ctx_, &AppContext::fileDownloaded, this,
            [this](const QString& fileId, const QString& name, qint64 size, const QString& mime,
                   const QByteArray& data) {
                // 头像图片：注册到缓存并刷新界面
                if (mime.startsWith("image/") && !data.isEmpty()) {
                    QPixmap pix;
                    if (pix.loadFromData(data)) {
                        setAvatarImage(fileId, pix);
                        ctx_->loadContacts();
                        ctx_->loadSessions();
                        myAvatarLabel_->setPixmap(
                            makeAvatar(ctx_->me().nickname, ctx_->me().avatar, 40));
                    }
                }
                // 缓存完成后再让聊天气泡重绘，否则气泡会先画成默认头像
                chatPanel_->onFileDownloaded(fileId, name, size, mime, data);
            });

    connect(ctx_, &AppContext::contactsReady, this, &MainWindow::onContactsReady);
    connect(ctx_, &AppContext::sessionsReady, this, &MainWindow::onSessionsReady);
    connect(ctx_, &AppContext::historyReady, this, &MainWindow::onHistoryReady);
    connect(ctx_, &AppContext::messageArrived, this, &MainWindow::onMessageArrived);
    connect(ctx_, &AppContext::messageSent, this, &MainWindow::onMessageSent);
    connect(ctx_, &AppContext::messageRecalled, this, &MainWindow::onMessageRecalled);
    connect(ctx_, &AppContext::readReceipt, this, &MainWindow::onReadReceipt);
    connect(ctx_, &AppContext::friendDeleted, this, &MainWindow::onFriendDeleted);
    connect(ctx_, &AppContext::groupUpdated, this, &MainWindow::onGroupUpdated);
    connect(ctx_, &AppContext::searchResults, this, &MainWindow::onSearchResults);
    connect(ctx_, &AppContext::typing, this, &MainWindow::onTyping);
    connect(ctx_, &AppContext::presenceChanged, this, &MainWindow::onPresenceChanged);
    connect(ctx_, &AppContext::friendAdded, this, &MainWindow::onFriendAdded);
    connect(ctx_, &AppContext::groupCreated, this, &MainWindow::onGroupCreated);
    connect(ctx_, &AppContext::groupMembersReady, this, &MainWindow::onGroupMembersReady);
    connect(ctx_, &AppContext::errorOccurred, this, &MainWindow::onError);
    connect(ctx_, &AppContext::profileUpdated, this, &MainWindow::onProfileUpdated);
    connect(ctx_, &AppContext::profileChanged, this, &MainWindow::onProfileChanged);

    // 加载初始数据
    ctx_->loadContacts();
    ctx_->loadSessions();
}

MainWindow::~MainWindow() = default;

void MainWindow::showSessionsView() {
    msgNav_->setChecked(true);
    contactNav_->setChecked(false);
    titleLabel_->setText(QStringLiteral("消息"));
    titleLabel_->hide();
    conversationSearch_->show();
    contactToolbar_->hide();
    listStack_->setCurrentWidget(sessionList_);
}

void MainWindow::showContactsView() {
    contactNav_->setChecked(true);
    msgNav_->setChecked(false);
    titleLabel_->setText(QStringLiteral("联系人"));
    conversationSearch_->hide();
    titleLabel_->show();
    contactToolbar_->show();
    listStack_->setCurrentWidget(contactList_);
}

void MainWindow::onAddFriendClicked() {
    bool ok = false;
    QString name = QInputDialog::getText(this, QStringLiteral("添加好友"),
                                         QStringLiteral("输入对方用户名："), QLineEdit::Normal,
                                         QString(), &ok);
    if (ok && !name.trimmed().isEmpty()) ctx_->addFriend(name.trimmed());
}

void MainWindow::onCreateGroupClicked() {
    bool ok = false;
    QString name = QInputDialog::getText(this, QStringLiteral("创建群聊"),
                                         QStringLiteral("群名称："), QLineEdit::Normal, QString(),
                                         &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    QStringList items;
    for (auto it = buddyById_.cbegin(); it != buddyById_.cend(); ++it)
        items << it->user.nickname + "(" + it->user.username + ")";
    QString sel = QInputDialog::getItem(this, QStringLiteral("选择群成员"),
                                        QStringLiteral("选择好友（单选，可多建后补充）："), items, 0,
                                        false, &ok);
    if (!ok) return;
    QVector<qint64> members;
    for (const auto& b : buddyById_) {
        if (b.user.nickname + "(" + b.user.username + ")" == sel) members << b.user.id;
    }
    ctx_->createGroup(name.trimmed(), members);
}

QString MainWindow::titleFor(qint64 targetId, int targetType) const {
    if (targetType == 1) {
        auto it = groupById_.find(targetId);
        return it != groupById_.end() ? it->name : QStringLiteral("群聊");
    }
    auto it = buddyById_.find(targetId);
    if (it != buddyById_.end())
        return it->user.nickname.isEmpty() ? it->user.username : it->user.nickname;
    return QStringLiteral("好友");
}

QString MainWindow::avatarFor(qint64 targetId, int targetType) const {
    if (targetType == 1) return QString();
    auto it = buddyById_.find(targetId);
    return it != buddyById_.end() ? it->user.avatar : QString();
}

void MainWindow::onSessionClicked(const QtSession& session) {
    activeTargetId_ = session.targetId;
    activeTargetType_ = session.targetType;
    chatPanel_->setPeer(titleFor(session.targetId, session.targetType), session.targetId,
                        session.targetType, session.targetType == 1);
    sessionList_->clearUnread(session.targetId, session.targetType);
    ctx_->openSession(session.targetId, session.targetType);
    ctx_->markRead(session.targetId, session.targetType); // 打开会话即上报已读
    refreshOnlineStatus();
}

void MainWindow::onOpenChat(qint64 targetId, int targetType, const QString& title) {
    Q_UNUSED(title)
    onSessionClicked(QtSession{targetId, targetType, titleFor(targetId, targetType),
                               avatarFor(targetId, targetType), QString(), 0, 0});
    showSessionsView();
}

void MainWindow::onSendRequested(qint64 targetId, int targetType, qint64 replyToId,
                                 const QString& text) {
    if (replyToId > 0)
        ctx_->sendReply(targetId, targetType, replyToId, text);
    else
        ctx_->sendText(targetId, targetType, text);
}

void MainWindow::onSearchClicked() {
    bool ok = false;
    QString kw = QInputDialog::getText(this, QStringLiteral("搜索消息"),
                                       QStringLiteral("关键词："), QLineEdit::Normal, QString(),
                                       &ok);
    if (ok && !kw.trimmed().isEmpty()) ctx_->searchMessages(kw.trimmed());
}

void MainWindow::onSearchResults(const QVector<QtMessage>& results) {
    showSearchDialog(results);
}

void MainWindow::showSearchDialog(const QVector<QtMessage>& results) {
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("搜索结果 (%1 条)").arg(results.size()));
    dlg.resize(520, 480);
    auto* v = new QVBoxLayout(&dlg);
    auto* list = new QListWidget(&dlg);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    for (const auto& msg : results) {
        QString peer = titleFor(msg.targetType == 1 ? msg.targetId
                                                    : (msg.fromId == ctx_->myId()
                                                           ? msg.targetId
                                                           : msg.fromId),
                                msg.targetType);
        QString text = msg.content;
        if (msg.msgType == 1) text = QStringLiteral("[图片]");
        else if (msg.msgType == 2) text = QStringLiteral("[文件]");
        QString who = (msg.fromId == ctx_->myId()) ? QStringLiteral("我") : msg.senderName;
        QString display = QStringLiteral("%1  %2: %3").arg(peer, who, text);
        auto* item = new QListWidgetItem(display, list);
        item->setData(Qt::UserRole, msg.targetType == 1 ? msg.targetId
                                                        : (msg.fromId == ctx_->myId()
                                                               ? msg.targetId
                                                               : msg.fromId));
        item->setData(Qt::UserRole + 1, msg.targetType);
        item->setData(Qt::UserRole + 2, msg.timestamp);
    }
    if (results.isEmpty()) {
        new QListWidgetItem(QStringLiteral("没有找到相关消息"), list);
    }
    v->addWidget(list, 1);
    auto* btnRow = new QHBoxLayout;
    auto* openBtn = new QPushButton(QStringLiteral("打开会话"), &dlg);
    auto* closeBtn = new QPushButton(QStringLiteral("关闭"), &dlg);
    QObject::connect(openBtn, &QPushButton::clicked, &dlg, [&] {
        auto* cur = list->currentItem();
        if (cur) {
            qint64 tid = cur->data(Qt::UserRole).toLongLong();
            int ttype = cur->data(Qt::UserRole + 1).toInt();
            onOpenChat(tid, ttype, titleFor(tid, ttype));
            dlg.accept();
        }
    });
    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnRow->addStretch();
    btnRow->addWidget(openBtn);
    btnRow->addWidget(closeBtn);
    v->addLayout(btnRow);
    dlg.exec();
}

void MainWindow::onMessageRecalled(qint64 msgId, qint64 targetId, int targetType) {
    if (activeTargetId_ == targetId && activeTargetType_ == targetType) {
        chatPanel_->onMessageRecalled(msgId);
    }
}

void MainWindow::onFriendDeleted(qint64 friendId, const QString& name) {
    Q_UNUSED(name)
    buddyById_.remove(friendId);
    ctx_->loadContacts();
    if (activeTargetType_ == 0 && activeTargetId_ == friendId) {
        chatPanel_->setPeer(QStringLiteral("选择会话开始聊天"), -1, 0, false);
    }
}

void MainWindow::onGroupUpdated(qint64 groupId) {
    Q_UNUSED(groupId)
    ctx_->loadContacts();
    ctx_->loadSessions();
}

void MainWindow::onTyping(int64_t fromId, qint64 targetId, int targetType) {
    bool relevant;
    if (targetType == 1) {
        relevant = (activeTargetType_ == 1 && activeTargetId_ == targetId && fromId != ctx_->myId());
    } else {
        relevant = (activeTargetType_ == 0 && activeTargetId_ == fromId);
    }
    if (relevant) chatPanel_->onTyping(true);
}

void MainWindow::openPeerFromMessage(const QtMessage& msg) {
    qint64 peer = msg.targetType == 1 ? msg.targetId
                                      : (msg.fromId == ctx_->myId() ? msg.targetId : msg.fromId);
    onOpenChat(peer, msg.targetType, titleFor(peer, msg.targetType));
}

void MainWindow::onLogout() {
    ctx_->logout();
    ctx_->disconnectAll();
    emit loggedOut();
    close();
}

void MainWindow::onContactsReady(const QVector<QtBuddy>& buddies, const QVector<QtGroup>& groups) {
    buddyById_.clear();
    for (const auto& b : buddies) buddyById_.insert(b.user.id, b);
    groupById_.clear();
    for (const auto& g : groups) groupById_.insert(g.id, g);
    contactList_->setBuddies(buddies);
    contactList_->setGroups(groups);
}

void MainWindow::onSessionsReady(const QVector<QtSession>& sessions) {
    sessions_ = sessions;
    sessionList_->setSessions(sessions);
}

void MainWindow::onHistoryReady(qint64 targetId, int targetType, const QVector<QtMessage>& msgs) {
    if (targetId == activeTargetId_ && targetType == activeTargetType_) {
        chatPanel_->loadHistory(msgs);
    }
}

void MainWindow::onMessageArrived(const QtMessage& msg) {
    // 判断当前打开的会话是否就是本消息所属的会话
    bool isActive;
    if (msg.targetType == 1) {
        isActive = (activeTargetType_ == 1 && activeTargetId_ == msg.targetId);
    } else {
        qint64 peer = (msg.fromId == ctx_->myId()) ? msg.targetId : msg.fromId;
        isActive = (activeTargetType_ == 0 && activeTargetId_ == peer);
    }
    if (isActive) {
        chatPanel_->appendMessage(msg, false);
        ctx_->markRead(msg.targetType == 1 ? msg.targetId
                                           : (msg.fromId == ctx_->myId() ? msg.targetId : msg.fromId),
                       msg.targetType); // 正在查看时即时上报已读
    }

    qint64 key = msg.targetType == 1 ? msg.targetId
                                     : (msg.fromId == ctx_->myId() ? msg.targetId : msg.fromId);
    ensureSessionItem(msg);
    int row = sessionList_->rowOfSession(key, msg.targetType);
    bool muted = false;
    if (row >= 0) muted = sessionList_->isMuted(sessionList_->sessionAt(row));
    if (!isActive && !muted && row >= 0) {
        QtSession s = sessionList_->sessionAt(row);
        s.unread += 1;
        sessionList_->upsertSession(s);
    }
}

void MainWindow::onReadReceipt(qint64 peerId, int targetType) {
    // 对方已读我发的消息：若正在与该对方聊天，则把己方消息标记为已读
    if (targetType == 0 && activeTargetType_ == 0 && activeTargetId_ == peerId) {
        chatPanel_->markPeerRead();
    }
}

void MainWindow::onMessageSent(const QtMessage& msg) {
    if (activeTargetId_ == msg.targetId && activeTargetType_ == msg.targetType) {
        chatPanel_->appendMessage(msg, true);
    }
    qint64 key = msg.targetType == 1 ? msg.targetId : msg.targetId;
    ensureSessionItem(msg);
    int row = sessionList_->rowOfSession(key, msg.targetType);
    if (row >= 0) sessionList_->upsertSession(sessionList_->sessionAt(row));
}

void MainWindow::ensureSessionItem(const QtMessage& msg) {
    QtSession s;
    s.targetId = msg.targetType == 1 ? msg.targetId
                                     : (msg.fromId == ctx_->myId() ? msg.targetId : msg.fromId);
    s.targetType = msg.targetType;
    s.title = titleFor(s.targetId, s.targetType);
    s.avatar = avatarFor(s.targetId, s.targetType);
    s.lastContent = msg.content;
    s.lastTime = msg.timestamp;
    s.unread = 0;
    sessionList_->upsertSession(s);
}

void MainWindow::onPresenceChanged(qint64 userId, bool online) {
    auto it = buddyById_.find(userId);
    if (it != buddyById_.end()) {
        it->user.online = online ? 1 : 0;
        contactList_->setBuddyOnline(userId, online);
    }
    if (activeTargetType_ == 0 && activeTargetId_ == userId) chatPanel_->setOnline(online);
}

void MainWindow::onFriendAdded(const QtBuddy& buddy) {
    buddyById_.insert(buddy.user.id, buddy);
    contactList_->upsertBuddy(buddy);
}

void MainWindow::onGroupCreated(const QtGroup& group) {
    groupById_.insert(group.id, group);
    contactList_->upsertGroup(group);
    ctx_->loadSessions();
}

void MainWindow::onGroupMembersReady(qint64 groupId, const QVector<QtMember>& members) {
    auto g = groupById_.value(groupId);
    bool isOwner = (g.ownerId == ctx_->myId());
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("群成员 - %1").arg(g.name.isEmpty()
                                                             ? QStringLiteral("群聊")
                                                             : g.name));
    dlg.resize(360, 420);
    auto* v = new QVBoxLayout(&dlg);
    auto* list = new QListWidget(&dlg);
    for (const auto& m : members) {
        QString n = m.user.nickname.isEmpty() ? m.user.username : m.user.nickname;
        QString label = (m.user.online ? QStringLiteral("● ") : QStringLiteral("○ ")) + n;
        if (m.user.id == g.ownerId) label += QStringLiteral(" (群主)");
        if (m.user.id == ctx_->myId()) label += QStringLiteral(" (我)");
        auto* item = new QListWidgetItem(label, list);
        item->setData(Qt::UserRole, m.user.id);
        item->setData(Qt::UserRole + 1, n);
        item->setData(Qt::UserRole + 2, QStringLiteral("踢出群聊"));
    }
    v->addWidget(list, 1);
    auto* btnRow = new QHBoxLayout;
    auto* kickBtn = new QPushButton(QStringLiteral("踢出选中成员"), &dlg);
    kickBtn->setEnabled(isOwner);
    auto* closeBtn = new QPushButton(QStringLiteral("关闭"), &dlg);
    QObject::connect(kickBtn, &QPushButton::clicked, &dlg, [&] {
        auto* cur = list->currentItem();
        if (!cur) return;
        qint64 memberId = cur->data(Qt::UserRole).toLongLong();
        if (memberId == ctx_->myId() || memberId == g.ownerId) return;
        if (QMessageBox::question(&dlg, QStringLiteral("踢出成员"),
                                  QStringLiteral("确定将 %1 踢出群聊吗？")
                                      .arg(cur->data(Qt::UserRole + 1).toString())) ==
            QMessageBox::Yes) {
            ctx_->kickMember(groupId, memberId);
            dlg.accept();
        }
    });
    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnRow->addStretch();
    btnRow->addWidget(kickBtn);
    btnRow->addWidget(closeBtn);
    v->addLayout(btnRow);
    dlg.exec();
}

void MainWindow::onError(int code, const QString& msg) {
    Q_UNUSED(code)
    QMessageBox::warning(this, QStringLiteral("提示"), msg);
}

void MainWindow::onProfileClicked() {
    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("个人资料"));
    dlg.setFixedWidth(360);

    auto* form = new QFormLayout(&dlg);
    form->setContentsMargins(20, 20, 20, 20);
    form->setSpacing(12);

    auto* avatarLabel = new QLabel(&dlg);
    avatarLabel->setFixedSize(72, 72);
    avatarLabel->setPixmap(makeAvatar(ctx_->me().nickname, ctx_->me().avatar, 72));
    avatarLabel->setAlignment(Qt::AlignCenter);
    form->addRow(QStringLiteral("头像"), avatarLabel);

    auto* avatarPath = new QLineEdit(&dlg);
    avatarPath->setPlaceholderText(QStringLiteral("（留空则不修改）选择图片文件"));
    auto* pickAvatar = new QPushButton(QStringLiteral("选择…"), &dlg);
    auto* avatarRow = new QHBoxLayout;
    avatarRow->addWidget(avatarPath, 1);
    avatarRow->addWidget(pickAvatar);
    form->addRow(QStringLiteral("新头像"), avatarRow);

    auto* nickEdit = new QLineEdit(ctx_->me().nickname, &dlg);
    form->addRow(QStringLiteral("昵称"), nickEdit);

    auto* oldPass = new QLineEdit(&dlg);
    oldPass->setEchoMode(QLineEdit::Password);
    oldPass->setPlaceholderText(QStringLiteral("改密码时需要"));
    form->addRow(QStringLiteral("旧密码"), oldPass);

    auto* newPass = new QLineEdit(&dlg);
    newPass->setEchoMode(QLineEdit::Password);
    newPass->setPlaceholderText(QStringLiteral("留空则不修改"));
    form->addRow(QStringLiteral("新密码"), newPass);

    auto* statusLabel = new QLabel(&dlg);
    statusLabel->setWordWrap(true);
    form->addRow(QString(), statusLabel);

    auto* btnRow = new QHBoxLayout;
    auto* cancelBtn = new QPushButton(QStringLiteral("取消"), &dlg);
    auto* saveBtn = new QPushButton(QStringLiteral("保存"), &dlg);
    saveBtn->setObjectName("primaryBtn");
    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(saveBtn);
    form->addRow(btnRow);

    connect(pickAvatar, &QPushButton::clicked, &dlg, [avatarPath] {
        QString f = QFileDialog::getOpenFileName(avatarPath, QStringLiteral("选择头像"),
                                                 QString(), QStringLiteral("图片 (*.png *.jpg *.jpeg *.gif *.webp *.bmp)"));
        if (!f.isEmpty()) avatarPath->setText(f);
    });

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, &dlg, [&] {
        const QString nick = nickEdit->text().trimmed();
        const QString avatar = avatarPath->text().trimmed();
        const QString oldP = oldPass->text();
        const QString newP = newPass->text();
        if (!newP.isEmpty() && newP.size() < 4) {
            statusLabel->setText(QStringLiteral("新密码至少4位"));
            return;
        }
        if (newP.isEmpty() && (nick.isEmpty() || nick == ctx_->me().nickname) && avatar.isEmpty()) {
            statusLabel->setText(QStringLiteral("没有要修改的内容"));
            return;
        }
        // 昵称留空 = 不修改；头像/密码同理
        ctx_->updateProfile(nick, avatar, oldP, newP);
        statusLabel->setText(QStringLiteral("保存中…"));
        saveBtn->setEnabled(false);
    });

    // 保存成功后自动关闭（通过 profileUpdatedSignal 连接）
    connect(this, &MainWindow::profileUpdatedSignal, &dlg,
            [&](int code, const QString& msg, const QtUser&) {
                if (code == 0) {
                    dlg.accept();
                } else {
                    statusLabel->setText(msg);
                    saveBtn->setEnabled(true);
                }
            });

    dlg.exec();

    // 更新左侧头像
    myAvatarLabel_->setPixmap(makeAvatar(ctx_->me().nickname, ctx_->me().avatar, 40));
}

void MainWindow::onProfileUpdated(int code, const QString& msg, const QtUser& me) {
    Q_UNUSED(me)
    if (code == 0) {
        myAvatarLabel_->setPixmap(makeAvatar(ctx_->me().nickname, ctx_->me().avatar, 40));
        ctx_->loadContacts();
    }
    emit profileUpdatedSignal(code, msg, me);
}

void MainWindow::onProfileChanged(qint64 userId, const QString& nickname,
                                  const QString& avatar) {
    Q_UNUSED(userId)
    Q_UNUSED(nickname)
    Q_UNUSED(avatar)
    ctx_->loadContacts(); // 刷新好友列表显示新昵称
}

void MainWindow::refreshOnlineStatus() {
    if (activeTargetType_ != 0) return;
    auto it = buddyById_.find(activeTargetId_);
    if (it != buddyById_.end()) chatPanel_->setOnline(it->user.online == 1);
}
