#include "ui/chatpanel.h"

#include <QDateTime>
#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "ui/bubblewidget.h"

ChatPanel::ChatPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("chatPanel");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // 标题栏
    auto* header = new QWidget(this);
    header->setObjectName("chatHeader");
    header->setFixedHeight(52);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 0, 12, 0);
    titleLabel_ = new QLabel(QStringLiteral("选择会话开始聊天"), header);
    titleLabel_->setObjectName("chatTitle");
    statusLabel_ = new QLabel(header);
    statusLabel_->setObjectName("chatStatus");
    statusLabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    auto* infoBtn = new QToolButton(header);
    infoBtn->setText(QStringLiteral("···"));
    infoBtn->setObjectName("infoBtn");
    infoBtn->setCursor(Qt::PointingHandCursor);
    connect(infoBtn, &QToolButton::clicked, this, &ChatPanel::onGroupInfoClicked);
    headerLayout->addWidget(titleLabel_);
    headerLayout->addStretch();
    headerLayout->addWidget(statusLabel_);
    headerLayout->addWidget(infoBtn);
    root->addWidget(header);

    // 消息区
    scrollArea_ = new QScrollArea(this);
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setFrameShape(QFrame::NoFrame);
    scrollArea_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    messageContainer_ = new QWidget(scrollArea_);
    messageLayout_ = new QVBoxLayout(messageContainer_);
    messageLayout_->setContentsMargins(12, 12, 12, 12);
    messageLayout_->setSpacing(8);
    messageLayout_->addStretch();
    scrollArea_->setWidget(messageContainer_);
    root->addWidget(scrollArea_, 1);

    // 输入区
    auto* inputBox = new QWidget(this);
    inputBox->setObjectName("inputBox");
    auto* inputLayout = new QVBoxLayout(inputBox);
    inputLayout->setContentsMargins(10, 6, 10, 10);
    inputLayout->setSpacing(6);

    auto* toolRow = new QHBoxLayout;
    auto* emojiBtn = new QToolButton(inputBox);
    emojiBtn->setText(QStringLiteral("☺"));
    emojiBtn->setObjectName("emojiBtn");
    emojiBtn->setToolTip(QStringLiteral("表情"));
    emojiBtn->setCursor(Qt::PointingHandCursor);
    connect(emojiBtn, &QToolButton::clicked, this, &ChatPanel::onEmojiClicked);
    toolRow->addWidget(emojiBtn);

    auto* imageBtn = new QToolButton(inputBox);
    imageBtn->setText(QStringLiteral("🖼"));
    imageBtn->setObjectName("emojiBtn");
    imageBtn->setToolTip(QStringLiteral("发送图片"));
    imageBtn->setCursor(Qt::PointingHandCursor);
    connect(imageBtn, &QToolButton::clicked, this, &ChatPanel::onImageClicked);
    toolRow->addWidget(imageBtn);

    auto* fileBtn = new QToolButton(inputBox);
    fileBtn->setText(QStringLiteral("📎"));
    fileBtn->setObjectName("emojiBtn");
    fileBtn->setToolTip(QStringLiteral("发送文件"));
    fileBtn->setCursor(Qt::PointingHandCursor);
    connect(fileBtn, &QToolButton::clicked, this, &ChatPanel::onFileClicked);
    toolRow->addWidget(fileBtn);

    toolRow->addStretch();
    inputLayout->addLayout(toolRow);

    // 引用回复横幅
    replyBanner_ = new QWidget(inputBox);
    replyBanner_->setObjectName("replyBanner");
    replyBanner_->hide();
    auto* bannerLayout = new QHBoxLayout(replyBanner_);
    bannerLayout->setContentsMargins(8, 4, 8, 4);
    replyBannerLabel_ = new QLabel(replyBanner_);
    replyBannerLabel_->setObjectName("replyBannerLabel");
    auto* cancelBtn = new QToolButton(replyBanner_);
    cancelBtn->setText(QStringLiteral("✕"));
    cancelBtn->setObjectName("emojiBtn");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(cancelBtn, &QToolButton::clicked, this, [this] {
        replyToId_ = 0;
        replyBanner_->hide();
    });
    bannerLayout->addWidget(replyBannerLabel_, 1);
    bannerLayout->addWidget(cancelBtn);
    inputLayout->addWidget(replyBanner_);

    inputEdit_ = new QTextEdit(inputBox);
    inputEdit_->setPlaceholderText(QStringLiteral("输入消息，Enter 发送"));
    inputEdit_->setFixedHeight(80);
    inputEdit_->setEnabled(false); // 未选择会话前不可输入
    inputLayout->addWidget(inputEdit_);

    sendBtn_ = new QPushButton(QStringLiteral("发送"), inputBox);
    sendBtn_->setObjectName("sendBtn");
    sendBtn_->setCursor(Qt::PointingHandCursor);
    sendBtn_->setFixedSize(76, 30);
    sendBtn_->setEnabled(false);
    auto* sendRow = new QHBoxLayout;
    sendRow->addStretch();
    sendRow->addWidget(sendBtn_);
    inputLayout->addLayout(sendRow);

    root->addWidget(inputBox);

    connect(sendBtn_, &QPushButton::clicked, this, &ChatPanel::onSendClicked);
    connect(inputEdit_, &QTextEdit::textChanged, this, [this] {
        int h = 40 + inputEdit_->document()->size().height();
        inputEdit_->setFixedHeight(qBound(40, h, 150));
        sendTypingThrottled();
    });
    typingTimer_ = new QTimer(this);
    typingTimer_->setSingleShot(true);
    connect(typingTimer_, &QTimer::timeout, this, [this] { statusLabel_->clear(); });
}

void ChatPanel::sendTypingThrottled() {
    if (targetId_ < 0) return;
    qint64 now = QDateTime::currentSecsSinceEpoch();
    if (now - lastTypingSent_ >= 2) {
        lastTypingSent_ = now;
        emit typingRequested(targetId_, targetType_);
    }
}

void ChatPanel::onTyping(bool typing) {
    if (!typing) {
        statusLabel_->clear();
        return;
    }
    statusLabel_->setText(QStringLiteral("对方正在输入…"));
    statusLabel_->setStyleSheet(QStringLiteral("color:#b5bac1;"));
    typingTimer_->start(3000);
}

void ChatPanel::setPeer(const QString& title, qint64 targetId, int targetType, bool isGroup) {
    titleLabel_->setText(title);
    targetId_ = targetId;
    targetType_ = targetType;
    isGroup_ = isGroup;
    replyToId_ = 0;
    if (replyBanner_) replyBanner_->hide();
    inputEdit_->setEnabled(true);
    sendBtn_->setEnabled(true);
    statusLabel_->clear();
    statusLabel_->setStyleSheet(QString());
    clearAll();
}

void ChatPanel::setOnline(bool online) {
    statusLabel_->setText(online ? QStringLiteral("● 在线") : QStringLiteral("○ 离线"));
    statusLabel_->setStyleSheet(online ? QStringLiteral("color:#23a559;")
                                       : QStringLiteral("color:#72767d;"));
}

bool ChatPanel::belongsToPeer(const QtMessage& msg) const {
    if (targetId_ < 0) return false;
    if (targetType_ == 1) return msg.targetId == targetId_;
    return msg.targetId == targetId_ || msg.fromId == targetId_;
}

void ChatPanel::appendMessage(const QtMessage& msg, bool own) {
    if (!belongsToPeer(msg)) return;
    appendBubble(msg, own);
    scrollToBottom();
}

void ChatPanel::appendBubble(const QtMessage& msg, bool own) {
    // 时间分隔
    bool insertTime = messageLayout_->count() <= 1; // 只有stretch
    QLabel* lastTimeLabel = nullptr;
    if (messageLayout_->count() > 1) {
        QWidget* last = messageLayout_->itemAt(messageLayout_->count() - 2)->widget();
        lastTimeLabel = qobject_cast<QLabel*>(last);
    }
    QDateTime dt = QDateTime::fromSecsSinceEpoch(msg.timestamp);
    bool sameMinute = lastTimeLabel && lastTimeLabel->property("ts").toLongLong() == dt.toSecsSinceEpoch();
    if (!insertTime && !sameMinute) insertTime = true;

    if (insertTime) {
        auto* timeLabel = new QLabel(dt.toString("yyyy-MM-dd HH:mm"), messageContainer_);
        timeLabel->setProperty("ts", dt.toSecsSinceEpoch());
        timeLabel->setObjectName("timeLabel");
        timeLabel->setAlignment(Qt::AlignCenter);
        messageLayout_->insertWidget(messageLayout_->count() - 1, timeLabel);
    }

    bool showName = (targetType_ == 1 && !own);
    auto* bubble = new BubbleWidget(messageContainer_);
    bubble->setMessage(msg, own, showName);
    // 文件卡片点击下载
    connect(bubble, &BubbleWidget::requestDownload, this, [this](const QString& fileId) {
        pendingDownloads_.insert(fileId, QString());
        emit downloadFileRequested(fileId);
    });
    // 图片点击放大
    connect(bubble, &BubbleWidget::imageClicked, this, &ChatPanel::showImageDialog);
    // 右键：撤回
    connect(bubble, &BubbleWidget::recallRequested, this,
            [this](qint64 msgId) { emit recallMessageRequested(msgId, targetId_, targetType_); });
    // 右键：引用回复
    connect(bubble, &BubbleWidget::replyRequested, this, &ChatPanel::setReplyQuote);
    // 图片气泡登记，等待下载数据
    if (msg.msgType == 1 && !bubble->fileId().isEmpty()) {
        imageBubbles_.insert(bubble->fileId(), bubble);
    }
    if (msg.id > 0) bubblesById_.insert(msg.id, bubble);
    messageLayout_->insertWidget(messageLayout_->count() - 1, bubble);
}

void ChatPanel::loadHistory(const QVector<QtMessage>& msgs) {
    QLayoutItem* child;
    while ((child = messageLayout_->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
    messageLayout_->addStretch();
    imageBubbles_.clear();
    pendingDownloads_.clear();
    bubblesById_.clear();
    for (const auto& m : msgs) {
        bool own = m.direction == 0;
        appendBubble(m, own);
    }
    QTimer::singleShot(0, this, &ChatPanel::scrollToBottom);
}

void ChatPanel::onFileDownloaded(const QString& fileId, const QString& name, qint64 size,
                                 const QString& mime, const QByteArray& data) {
    Q_UNUSED(size)
    Q_UNUSED(mime)
    if (auto it = imageBubbles_.find(fileId); it != imageBubbles_.end()) {
        it.value()->setImageData(data);
        return;
    }
    if (pendingDownloads_.contains(fileId)) {
        pendingDownloads_.remove(fileId);
        QString saveName = name.isEmpty() ? fileId : name;
        emit saveFileData(saveName, data);
    }
}

void ChatPanel::markPeerRead() {
    // 收到已读回执：把所有自己发出的消息标记为已读
    for (int i = 0; i < messageLayout_->count(); ++i) {
        auto* w = messageLayout_->itemAt(i)->widget();
        auto* bubble = qobject_cast<BubbleWidget*>(w);
        if (bubble && bubble->isOwn()) bubble->setRead(true);
    }
    update();
}

void ChatPanel::clearAll() {
    QLayoutItem* child;
    while ((child = messageLayout_->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }
    messageLayout_->addStretch();
    inputEdit_->clear();
}

void ChatPanel::scrollToBottom() {
    auto* bar = scrollArea_->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void ChatPanel::onSendClicked() {
    QString text = inputEdit_->toPlainText().trimmed();
    if (text.isEmpty()) return;
    if (targetId_ < 0) {
        statusLabel_->setText(QStringLiteral("请先双击左侧的好友或会话"));
        statusLabel_->setStyleSheet(QStringLiteral("color:#fa5151;"));
        return;
    }
    emit sendRequested(targetId_, targetType_, replyToId_, text);
    replyToId_ = 0;
    if (replyBanner_) replyBanner_->hide();
    inputEdit_->clear();
}

void ChatPanel::setReplyQuote(qint64 msgId, const QString& content, const QString& senderName) {
    replyToId_ = msgId;
    if (!replyBanner_) return;
    replyBannerLabel_->setText(QStringLiteral("回复 %1: %2").arg(senderName, content));
    replyBanner_->show();
    inputEdit_->setFocus();
}

void ChatPanel::showImageDialog(const QPixmap& pixmap, const QString& name) {
    QDialog dlg(this);
    dlg.setWindowTitle(name.isEmpty() ? QStringLiteral("图片预览") : name);
    dlg.resize(700, 560);
    auto* v = new QVBoxLayout(&dlg);
    auto* scroll = new QScrollArea(&dlg);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* label = new QLabel(scroll);
    label->setAlignment(Qt::AlignCenter);
    label->setPixmap(pixmap);
    scroll->setWidget(label);
    v->addWidget(scroll, 1);
    auto* btnRow = new QHBoxLayout;
    auto* saveBtn = new QPushButton(QStringLiteral("保存到本地"), &dlg);
    auto* closeBtn = new QPushButton(QStringLiteral("关闭"), &dlg);
    QObject::connect(saveBtn, &QPushButton::clicked, &dlg, [&dlg, &pixmap, &name] {
        QString path = QFileDialog::getSaveFileName(&dlg, QStringLiteral("保存图片"), name);
        if (path.isEmpty()) return;
        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) {
            pixmap.save(&f, "PNG");
            f.close();
        }
    });
    QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    btnRow->addStretch();
    btnRow->addWidget(saveBtn);
    btnRow->addWidget(closeBtn);
    v->addLayout(btnRow);
    dlg.exec();
}

void ChatPanel::onMessageRecalled(qint64 msgId) {
    if (auto it = bubblesById_.find(msgId); it != bubblesById_.end()) {
        auto* bubble = it.value();
        messageLayout_->removeWidget(bubble);
        bubblesById_.erase(it);
        if (!bubble->fileId().isEmpty()) imageBubbles_.remove(bubble->fileId());
        bubble->deleteLater();
    }
}

void ChatPanel::onEmojiClicked() {
    // 极简表情面板
    QMenu menu(this);
    const QStringList emojis = {QStringLiteral("😀"), QStringLiteral("😁"), QStringLiteral("😂"),
                                QStringLiteral("😅"), QStringLiteral("😊"), QStringLiteral("😍"),
                                QStringLiteral("😘"), QStringLiteral("😜"), QStringLiteral("🤔"),
                                QStringLiteral("😴"), QStringLiteral("👍"), QStringLiteral("👎"),
                                QStringLiteral("❤️"),  QStringLiteral("🎉"), QStringLiteral("👋"),
                                QStringLiteral("💪")};
    for (const QString& e : emojis) {
        auto* a = menu.addAction(e);
        connect(a, &QAction::triggered, this, [this, e] { inputEdit_->insertPlainText(e); });
    }
    menu.exec(QCursor::pos());
}

void ChatPanel::onImageClicked() {
    if (targetId_ < 0) {
        statusLabel_->setText(QStringLiteral("请先选择会话"));
        return;
    }
    QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择图片"), QString(),
        QStringLiteral("图片 (*.png *.jpg *.jpeg *.gif *.bmp *.webp);;所有文件 (*)"));
    if (!path.isEmpty()) emit sendImageRequested(targetId_, targetType_, path);
}

void ChatPanel::onFileClicked() {
    if (targetId_ < 0) {
        statusLabel_->setText(QStringLiteral("请先选择会话"));
        return;
    }
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择文件"), QString(),
                                                QStringLiteral("所有文件 (*)"));
    if (!path.isEmpty()) emit sendFileRequested(targetId_, targetType_, path);
}

void ChatPanel::onGroupInfoClicked() {
    if (isGroup_ && targetId_ >= 0) emit groupMembersRequested(targetId_);
}
