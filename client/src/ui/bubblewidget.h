#pragma once

#include <QPixmap>
#include <QWidget>

#include "app/appcontext.h"

// 聊天气泡：文本 / 图片 / 文件卡片，左侧或右侧 + 头像，系统消息居中
class BubbleWidget : public QWidget {
    Q_OBJECT

public:
    explicit BubbleWidget(QWidget* parent = nullptr);

    void setMessage(const QtMessage& msg, bool own, bool showName);

    // 图片数据到达后设置，重算尺寸
    void setImageData(const QByteArray& data);
    // 更新已读状态（收到已读回执时调用）
    void setRead(bool read);
    bool isOwn() const { return own_; }

    const QString& fileId() const { return fileId_; }
    qint64 msgId() const { return msg_.id; }

    QSize sizeHint() const override { return size_; }

signals:
    void requestDownload(const QString& fileId);
    void imageClicked(const QPixmap& pixmap, const QString& name);
    void recallRequested(qint64 msgId);
    void replyRequested(qint64 msgId, const QString& content, const QString& senderName);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void parseContent();
    void computeSize();
    bool hasReply() const;
    void paintFileCard(QPainter& p, int x, int y, int w, int h) const;

    QtMessage msg_;
    bool own_ = false;
    bool showName_ = false;
    QSize size_;
    QString avatarPath_;

    QString fileId_;
    QString fileName_;
    qint64 fileSize_ = 0;
    QPixmap image_;
    bool hasImage_ = false;
};
