#include "ui/bubblewidget.h"

#include <QContextMenuEvent>
#include <QFileInfo>
#include <QFontMetrics>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include <ctime>

#include "ui/avatar.h"

namespace {
const int kMaxBubbleWidth = 440;
const int kMaxImageWidth = 360;
const int kHPad = 12;
const int kVPad = 8;
const int kAvatarSize = 36;
const int kGap = 8;
const int kReplyBlockH = 40;
const qint64 kRecallWindowSec = 120; // 撤回时限 2 分钟

QString humanSize(qint64 bytes) {
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1024LL * 1024 * 1024) return QString::number(bytes / (1024.0 * 1024), 'f', 1) + " MB";
    return QString::number(bytes / (1024.0 * 1024 * 1024), 'f', 1) + " GB";
}
} // namespace

BubbleWidget::BubbleWidget(QWidget* parent) : QWidget(parent) {
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void BubbleWidget::setMessage(const QtMessage& msg, bool own, bool showName) {
    msg_ = msg;
    own_ = own;
    showName_ = showName;
    hasImage_ = false;
    image_ = QPixmap();
    parseContent();
    computeSize();
    update();
}

void BubbleWidget::setAvatar(const QString& fileId) {
    if (avatarPath_ == fileId) return;
    avatarPath_ = fileId;
    update();
}

void BubbleWidget::parseContent() {
    fileId_.clear();
    fileName_.clear();
    fileSize_ = 0;
    if (msg_.msgType == 1 || msg_.msgType == 2) {
        QStringList parts = msg_.content.split(QLatin1Char('|'));
        if (parts.size() >= 2) {
            fileId_ = parts.at(0);
            fileName_ = parts.at(1);
            fileSize_ = parts.size() >= 3 ? parts.at(2).toLongLong() : 0;
        }
    }
}

void BubbleWidget::setImageData(const QByteArray& data) {
    if (msg_.msgType != 1 || data.isEmpty()) return;
    image_.loadFromData(data);
    if (image_.isNull()) {
        hasImage_ = false;
        computeSize();
        update();
        return;
    }
    hasImage_ = true;
    computeSize();
    update();
}

void BubbleWidget::computeSize() {
    QFontMetrics fm(font());
    if (msg_.msgType == 3) { // 系统消息
        int w = fm.horizontalAdvance(msg_.content) + 16;
        size_ = QSize(w, fm.height() + 8);
        setFixedSize(size_);
        return;
    }

    int nameH = showName_ ? fm.height() + 4 : 0;
    int readH = own_ ? fm.height() : 0;

    if (msg_.msgType == 1) { // 图片
        int w = kMaxImageWidth;
        int h = 120;
        if (hasImage_) {
            int iw = image_.width();
            int ih = image_.height();
            if (iw > kMaxImageWidth) {
                h = qMax(60, ih * kMaxImageWidth / iw);
                w = kMaxImageWidth;
            } else {
                w = iw;
                h = ih;
            }
        }
        size_ = QSize(w + kAvatarSize + 3 * kGap, qMax(h, kAvatarSize) + nameH + readH);
    } else if (msg_.msgType == 2) { // 文件卡片
        int w = 280;
        int h = 60;
        size_ = QSize(w + kAvatarSize + 3 * kGap, qMax(h, kAvatarSize) + nameH + readH);
    } else { // 文本
        QString text = msg_.content;
        if (text.isEmpty()) text = QStringLiteral("…");
        QRect bounds = fm.boundingRect(QRect(0, 0, kMaxBubbleWidth - 2 * kHPad, 200000),
                                       Qt::TextWordWrap, text);
        int bubbleW = qMin(kMaxBubbleWidth, bounds.width() + 2 * kHPad);
        int replyH = hasReply() ? kReplyBlockH : 0;
        int bubbleH = bounds.height() + 2 * kVPad + replyH;
        size_ = QSize(bubbleW + kAvatarSize + 3 * kGap, qMax(bubbleH, kAvatarSize) + nameH + readH);
    }
    setFixedSize(size_);
}

bool BubbleWidget::hasReply() const {
    return msg_.replyToId > 0 && !msg_.replyContent.isEmpty();
}

void BubbleWidget::setRead(bool read) {
    msg_.read = read ? 1 : 0;
    update();
}

void BubbleWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QFontMetrics fm(font());

    int nameH = showName_ ? fm.height() + 4 : 0;
    int readH = own_ ? fm.height() : 0;
    int yTop = nameH;

    // 系统消息
    if (msg_.msgType == 3) {
        p.setPen(QColor("#949ba4"));
        QFont f = font();
        f.setPointSizeF(f.pointSizeF() - 1.0);
        p.setFont(f);
        p.drawText(rect(), Qt::AlignCenter, msg_.content);
        return;
    }

    bool own = own_;
    int avatarX = own ? width() - kAvatarSize - kGap : kGap;
    int avatarY = yTop + qMax(0, (size_.height() - nameH - readH - kAvatarSize) / 2);

    // 头像
    p.drawPixmap(avatarX, avatarY, makeAvatar(msg_.senderName, avatarPath_, kAvatarSize));

    int contentW = size_.width() - kAvatarSize - 3 * kGap;
    int bubbleX = own ? avatarX - kGap - contentW : avatarX + kAvatarSize + kGap;

    // 发送者昵称（群聊）
    if (showName_) {
        p.setPen(QColor("#b5bac1"));
        QFont small = font();
        small.setPointSizeF(small.pointSizeF() - 1.0);
        p.setFont(small);
        p.drawText(QRect(bubbleX, 0, contentW, fm.height() + 4), Qt::AlignLeft | Qt::AlignVCenter,
                   msg_.senderName);
    }

    if (msg_.msgType == 1) { // 图片
        int imgH = size_.height() - nameH - readH;
        QRect imgRect(bubbleX, yTop, contentW, imgH);
        if (hasImage_) {
            QPainterPath clip;
            clip.addRoundedRect(QRectF(imgRect), 8, 8);
            p.save();
            p.setClipPath(clip);
            p.drawPixmap(imgRect, image_);
            p.restore();
        } else {
            QPainterPath bubblePath;
            bubblePath.addRoundedRect(QRectF(imgRect), 8, 8);
            p.fillPath(bubblePath, own ? QColor("#5865f2") : QColor("#383a40"));
            p.setPen(QColor("#d4d7dc"));
            p.drawText(imgRect, Qt::AlignCenter, QStringLiteral("🖼️ 图片加载中…"));
        }
    } else if (msg_.msgType == 2) { // 文件卡片
        paintFileCard(p, bubbleX, yTop, contentW, size_.height() - nameH - readH);
    } else { // 文本
        QString text = msg_.content;
        if (text.isEmpty()) text = QStringLiteral("…");
        QRect textBounds = fm.boundingRect(QRect(0, 0, kMaxBubbleWidth - 2 * kHPad, 200000),
                                           Qt::TextWordWrap, text);
        int bubbleW = qMin(kMaxBubbleWidth, textBounds.width() + 2 * kHPad);
        int replyH = hasReply() ? kReplyBlockH : 0;
        int bubbleH = textBounds.height() + 2 * kVPad + replyH;
        int bx = own ? avatarX - kGap - bubbleW : avatarX + kAvatarSize + kGap;
        QPainterPath bubblePath;
        bubblePath.addRoundedRect(QRectF(bx, yTop, bubbleW, bubbleH), 8, 8);
        p.fillPath(bubblePath, own ? QColor("#5865f2") : QColor("#383a40"));
        if (!own) {
            p.setPen(QPen(QColor("#45474f")));
            p.drawPath(bubblePath);
        }
        int innerTop = yTop;
        if (hasReply()) {
            // 引用块
            QRect qrect(bx + kHPad, yTop + kVPad, bubbleW - 2 * kHPad, kReplyBlockH - 2 * kVPad);
            QPainterPath qpath;
            qpath.addRoundedRect(QRectF(qrect), 4, 4);
            p.fillPath(qpath, own ? QColor("#ffffff22") : QColor("#00000022"));
            // 左侧色条
            p.fillRect(QRect(qrect.left(), qrect.top(), 3, qrect.height()),
                       own ? QColor("#ffffff") : QColor("#5865f2"));
            QFont qf = font();
            qf.setPointSizeF(qf.pointSizeF() - 1.0);
            QFontMetrics qfm(qf);
            p.setFont(qf);
            p.setPen(QColor("#b5bac1"));
            p.drawText(QRect(qrect.left() + 8, qrect.top() + 2, qrect.width() - 12,
                             qfm.height()),
                       Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("引用"));
            p.drawText(QRect(qrect.left() + 8, qrect.top() + 2 + qfm.height(),
                             qrect.width() - 12, qfm.height()),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       qfm.elidedText(msg_.replyContent, Qt::ElideRight, qrect.width() - 12));
            innerTop = qrect.bottom() + kVPad;
        }
        QFont tf = font();
        tf.setPointSizeF(tf.pointSizeF() - 0.5);
        p.setFont(tf);
        p.setPen(QColor("#f2f3f5"));
        p.drawText(QRect(bx + kHPad, innerTop, kMaxBubbleWidth - 2 * kHPad, 200000),
                   Qt::TextWordWrap, text);
    }

    // 己方消息：已读/未读
    if (own_) {
        QFont rf = font();
        rf.setPointSizeF(rf.pointSizeF() - 2.0);
        QFontMetrics rfm(rf);
        p.setFont(rf);
        p.setPen(msg_.read != 0 ? QColor("#23a559") : QColor("#949ba4"));
        int contentH = size_.height() - nameH - readH;
        int y = yTop + contentH + qMax(0, readH - rfm.height());
        p.drawText(QRect(bubbleX, y, contentW, rfm.height()), Qt::AlignRight,
                   msg_.read != 0 ? QStringLiteral("已读") : QStringLiteral("未读"));
    }
}

void BubbleWidget::paintFileCard(QPainter& p, int x, int y, int w, int h) const {
    bool own = own_;
    QPainterPath card;
    card.addRoundedRect(QRectF(x, y, w, h), 8, 8);
    p.fillPath(card, own ? QColor("#4752c4") : QColor("#383a40"));
    if (!own) {
        p.setPen(QPen(QColor("#45474f")));
        p.drawPath(card);
    }

    // 文件图标
    QFont iconFont = font();
    iconFont.setPointSizeF(iconFont.pointSizeF() + 4);
    p.setFont(iconFont);
    p.setPen(QColor("#b5bac1"));
    p.drawText(QRect(x + 12, y, 36, h), Qt::AlignCenter, QStringLiteral("📄"));

    // 文件名 + 大小
    QFont nf = font();
    nf.setBold(true);
    QFontMetrics nfm(nf);
    p.setFont(nf);
    p.setPen(QColor("#f2f3f5"));
    QString name = fileName_.isEmpty() ? QStringLiteral("文件") : fileName_;
    p.drawText(QRect(x + 52, y + 8, w - 64, nfm.height()), Qt::AlignLeft | Qt::AlignVCenter,
               nfm.elidedText(name, Qt::ElideRight, w - 64));
    QFont sf = font();
    sf.setPointSizeF(sf.pointSizeF() - 1.0);
    QFontMetrics sfm(sf);
    p.setFont(sf);
    p.setPen(QColor("#b5bac1"));
    p.drawText(QRect(x + 52, y + 8 + nfm.height(), w - 64, sfm.height()),
               Qt::AlignLeft | Qt::AlignVCenter,
               QStringLiteral("%1 · 点击下载").arg(humanSize(fileSize_)));
}

void BubbleWidget::mousePressEvent(QMouseEvent* event) {
    // 文件卡片点击 -> 触发下载（由 ChatPanel 监听该事件信号）
    if (msg_.msgType == 2 && !fileId_.isEmpty() && event->button() == Qt::LeftButton) {
        emit requestDownload(fileId_);
    }
    // 图片点击 -> 放大预览
    if (msg_.msgType == 1 && hasImage_ && event->button() == Qt::LeftButton) {
        emit imageClicked(image_, fileName_);
    }
    QWidget::mousePressEvent(event);
}

void BubbleWidget::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu(this);
    QAction* recallAction = nullptr;
    // 自己的消息、2 分钟内可撤回（系统消息除外）
    if (own_ && msg_.msgType != 3 &&
        (time(nullptr) - msg_.timestamp) <= kRecallWindowSec) {
        recallAction = menu.addAction(QStringLiteral("撤回"));
    }
    QAction* replyAction = menu.addAction(QStringLiteral("引用回复"));
    QAction* chosen = menu.exec(event->globalPos());
    if (chosen == recallAction) {
        emit recallRequested(msg_.id);
    } else if (chosen == replyAction) {
        QString content = msg_.content;
        if (content.size() > 80) content = content.left(80) + QStringLiteral("…");
        emit replyRequested(msg_.id, content, msg_.senderName);
    }
}
