#pragma once

#include <QHash>
#include <QWidget>

#include "app/appcontext.h"

class QLabel;
class QScrollArea;
class QVBoxLayout;
class QTextEdit;
class QPushButton;
class QToolButton;
class BubbleWidget;

// 聊天面板：标题栏 + 消息区 + 输入区
class ChatPanel : public QWidget {
    Q_OBJECT

public:
    explicit ChatPanel(AppContext* ctx, QWidget* parent = nullptr);

    void setPeer(const QString& title, qint64 targetId, int targetType, bool isGroup);
    void setOnline(bool online);
    void appendMessage(const QtMessage& msg, bool own);
    void loadHistory(const QVector<QtMessage>& msgs);
    void markPeerRead();
    void onFileDownloaded(const QString& fileId, const QString& name, qint64 size,
                          const QString& mime, const QByteArray& data);
    void onMessageRecalled(qint64 msgId);
    void onTyping(bool typing);
    void clearAll();

signals:
    void sendRequested(qint64 targetId, int targetType, qint64 replyToId, const QString& text);
    void sendImageRequested(qint64 targetId, int targetType, const QString& path);
    void sendFileRequested(qint64 targetId, int targetType, const QString& path);
    void downloadFileRequested(const QString& fileId);
    void recallMessageRequested(qint64 msgId, qint64 targetId, int targetType);
    void typingRequested(qint64 targetId, int targetType);
    void saveFileData(const QString& name, const QByteArray& data);
    void groupMembersRequested(qint64 groupId);

private slots:
    void onSendClicked();
    void onEmojiClicked();
    void onImageClicked();
    void onFileClicked();
    void onGroupInfoClicked();

private:
    void scrollToBottom();
    bool belongsToPeer(const QtMessage& msg) const;
    void appendBubble(const QtMessage& msg, bool own);
    void showImageDialog(const QPixmap& pixmap, const QString& name);
    void setReplyQuote(qint64 msgId, const QString& content, const QString& senderName);
    void sendTypingThrottled();

    QLabel* titleLabel_;
    QLabel* statusLabel_;
    QWidget* messageContainer_;
    QVBoxLayout* messageLayout_;
    QScrollArea* scrollArea_;
    QTextEdit* inputEdit_;
    QPushButton* sendBtn_;
    QWidget* replyBanner_;
    QLabel* replyBannerLabel_;

    qint64 targetId_ = -1;
    int targetType_ = 0;
    bool isGroup_ = false;
    AppContext* ctx_ = nullptr;

    qint64 replyToId_ = 0;

    // fileId -> 图片气泡
    QHash<QString, BubbleWidget*> imageBubbles_;
    // msgId -> 气泡（撤回用）
    QHash<qint64, BubbleWidget*> bubblesById_;
    // 用户点击下载的文件（fileId -> name）
    QHash<QString, QString> pendingDownloads_;

    QTimer* typingTimer_ = nullptr;
    qint64 lastTypingSent_ = 0;
};
