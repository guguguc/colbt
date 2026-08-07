#include "ui/sessionlistwidget.h"

#include <QContextMenuEvent>
#include <QDateTime>
#include <QEnterEvent>
#include <QEvent>
#include <QMenu>
#include <QResizeEvent>
#include <QSettings>
#include <algorithm>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

#include "ui/avatar.h"

namespace {
const int kAvatarSize = 42;
} // namespace

// ---------- SessionItemWidget ----------

SessionItemWidget::SessionItemWidget(const QtSession& session, QWidget* parent)
    : QWidget(parent), session_(session) {
    setFixedHeight(kRowHeight);
    setCursor(Qt::PointingHandCursor);
}

void SessionItemWidget::setSession(const QtSession& session) {
    session_ = session;
    update();
}

void SessionItemWidget::setFlags(bool pinned, bool muted) {
    pinned_ = pinned;
    muted_ = muted;
    update();
}

void SessionItemWidget::enterEvent(QEnterEvent* event) {
    hovered_ = true;
    update();
    QWidget::enterEvent(event);
}

void SessionItemWidget::leaveEvent(QEvent* event) {
    hovered_ = false;
    update();
    QWidget::leaveEvent(event);
}

void SessionItemWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (hovered_) {
        p.fillRect(rect(), QColor("#35373c"));
    }

    // 头像
    QString name = session_.title;
    p.drawPixmap(10, (height() - kAvatarSize) / 2,
                 makeAvatar(name, session_.avatar, kAvatarSize));

    // 标题
    QFont titleFont = font();
    titleFont.setBold(true);
    QFontMetrics tfm(titleFont);
    p.setFont(titleFont);
    p.setPen(QColor("#dbdee1"));
    int textX = 10 + kAvatarSize + 10;
    p.drawText(QRect(textX, 8, width() - textX - 70, tfm.height()), Qt::AlignLeft | Qt::AlignVCenter,
               tfm.elidedText(session_.title, Qt::ElideRight, width() - textX - 70));

    // 时间
    QFont timeFont = font();
    timeFont.setPointSizeF(timeFont.pointSizeF() - 1.0);
    QFontMetrics tfm2(timeFont);
    p.setFont(timeFont);
    p.setPen(QColor("#949ba4"));
    QString time;
    QDateTime dt = QDateTime::fromSecsSinceEpoch(session_.lastTime);
    QDateTime now = QDateTime::currentDateTime();
    if (dt.date() == now.date())
        time = dt.toString("HH:mm");
    else if (dt.date().addDays(1) == now.date())
        time = QStringLiteral("昨天");
    else
        time = dt.toString("M月d日");
    p.drawText(QRect(width() - 66, 8, 56, tfm2.height()), Qt::AlignRight | Qt::AlignVCenter, time);

    // 最后一条消息预览
    QFont previewFont = font();
    previewFont.setPointSizeF(previewFont.pointSizeF() - 0.5);
    QFontMetrics pfm(previewFont);
    p.setFont(previewFont);
    p.setPen(QColor("#949ba4"));
    p.drawText(QRect(textX, 8 + tfm.height(), width() - textX - 12, pfm.height()),
               Qt::AlignLeft | Qt::AlignVCenter,
               pfm.elidedText(session_.lastContent.isEmpty() ? QStringLiteral(" ")
                                                             : session_.lastContent,
                              Qt::ElideRight, width() - textX - 12));

    // 未读角标
    if (session_.unread > 0) {
        QString badge = session_.unread > 99 ? QStringLiteral("99+")
                                             : QString::number(session_.unread);
        QFontMetrics bfm(font());
        int bw = bfm.horizontalAdvance(badge) + 12;
        int bh = 18;
        QRect badgeRect(width() - bw - 10, height() - bh - 8, bw, bh);
        QPainterPath path;
        path.addRoundedRect(QRectF(badgeRect), bh / 2.0, bh / 2.0);
        p.fillPath(path, QColor("#ed4245"));
        QFont bfont = font();
        bfont.setPointSizeF(bfont.pointSizeF() - 1.5);
        p.setFont(bfont);
        p.setPen(Qt::white);
        p.drawText(badgeRect, Qt::AlignCenter, badge);
    }

    // 置顶 / 免打扰标记
    int flagX = width() - 26;
    QFont f2 = font();
    f2.setPointSizeF(f2.pointSizeF() - 2.0);
    p.setFont(f2);
    p.setPen(QColor("#949ba4"));
    if (muted_) {
        p.drawText(QRect(flagX, height() - 22, 18, 16), Qt::AlignCenter, QStringLiteral("🔕"));
    }
    if (pinned_) {
        p.drawText(QRect(12, 6, 16, 14), Qt::AlignCenter, QStringLiteral("📌"));
    }
}

// ---------- SessionListWidget ----------

SessionListWidget::SessionListWidget(QWidget* parent) : QListWidget(parent) {
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setSelectionMode(QAbstractItemView::SingleSelection);
    connect(this, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        auto* w = static_cast<SessionItemWidget*>(itemWidget(item));
        if (w) emit sessionClicked(w->currentSession());
    });

    // 读取置顶/免打扰配置
    QSettings settings;
    const auto pinnedList = settings.value("sessions/pinned").toStringList();
    for (const QString& k : pinnedList) pinned_.insert(k);
    const auto mutedList = settings.value("sessions/muted").toStringList();
    for (const QString& k : mutedList) muted_.insert(k);
}

QString SessionListWidget::keyOf(const QtSession& sess) const {
    return QStringLiteral("%1:%2").arg(sess.targetType).arg(sess.targetId);
}

bool SessionListWidget::isPinned(const QtSession& sess) const { return pinned_.contains(keyOf(sess)); }
bool SessionListWidget::isMuted(const QtSession& sess) const { return muted_.contains(keyOf(sess)); }

void SessionListWidget::setPinned(const QtSession& sess, bool on) {
    if (on) pinned_.insert(keyOf(sess));
    else pinned_.remove(keyOf(sess));
    QSettings settings;
    QStringList pv; for (const auto&k : pinned_) pv << k; settings.setValue("sessions/pinned", pv);
    reorder();
}

void SessionListWidget::setMuted(const QtSession& sess, bool on) {
    if (on) muted_.insert(keyOf(sess));
    else muted_.remove(keyOf(sess));
    QSettings settings;
    QStringList mv; for (const auto&k : muted_) mv << k; settings.setValue("sessions/muted", mv);
    reorder();
}

void SessionListWidget::reorder() {
    // 置顶的排前面，未置顶按原顺序
    QList<QtSession> items;
    for (int i = 0; i < count(); ++i) {
        if (auto* w = static_cast<SessionItemWidget*>(itemWidget(item(i))))
            items.append(w->currentSession());
    }
    std::stable_sort(items.begin(), items.end(),
                     [this](const QtSession& a, const QtSession& b) {
                         return isPinned(a) && !isPinned(b);
                     });
    setSessions(items);
}

void SessionListWidget::resort() { reorder(); }

void SessionListWidget::contextMenuEvent(QContextMenuEvent* event) {
    QListWidgetItem* it = itemAt(event->pos());
    if (!it) return;
    auto* w = static_cast<SessionItemWidget*>(itemWidget(it));
    if (!w) return;
    QtSession sess = w->currentSession();
    QMenu menu(this);
    bool pinned = isPinned(sess);
    QAction* pinAct = menu.addAction(pinned ? QStringLiteral("取消置顶")
                                            : QStringLiteral("置顶会话"));
    bool muted = isMuted(sess);
    QAction* muteAct = menu.addAction(muted ? QStringLiteral("取消免打扰")
                                            : QStringLiteral("消息免打扰"));
    QAction* chosen = menu.exec(event->globalPos());
    if (chosen == pinAct) {
        setPinned(sess, !pinned);
        emit togglePinRequested(sess, !pinned);
    } else if (chosen == muteAct) {
        setMuted(sess, !muted);
        emit toggleMuteRequested(sess, !muted);
    }
}

void SessionListWidget::setSessions(const QVector<QtSession>& sessions) {
    clear();
    for (const auto& s : sessions) upsertSession(s);
}

void SessionListWidget::upsertSession(const QtSession& session) {
    if (QListWidgetItem* it = findItem(session.targetId, session.targetType)) {
        auto* w = static_cast<SessionItemWidget*>(itemWidget(it));
        QtSession merged = w->currentSession();
        merged.lastContent = session.lastContent;
        merged.lastTime = session.lastTime;
        merged.unread += session.unread;
        merged.title = session.title;
        w->setSession(merged);
        w->setFlags(isPinned(merged), isMuted(merged));
    } else {
        auto* item = new QListWidgetItem(this);
        auto* w = new SessionItemWidget(session, this);
        w->setFlags(isPinned(session), isMuted(session));
        item->setSizeHint(QSize(viewport()->width(), w->height()));
        setItemWidget(item, w);
    }
    if (isPinned(session)) reorder();
}

void SessionListWidget::resizeEvent(QResizeEvent* event) {
    QListWidget::resizeEvent(event);
    // 窗口宽度变化时同步会话项宽度
    for (int i = 0; i < count(); ++i) {
        if (QListWidgetItem* it = item(i)) {
            auto* w = static_cast<SessionItemWidget*>(itemWidget(it));
            it->setSizeHint(QSize(viewport()->width(), w ? w->height() : 0));
        }
    }
}

void SessionListWidget::clearUnread(qint64 targetId, int targetType) {
    if (QListWidgetItem* it = findItem(targetId, targetType)) {
        auto* w = static_cast<SessionItemWidget*>(itemWidget(it));
        QtSession s = w->currentSession();
        s.unread = 0;
        w->setSession(s);
    }
}

QtSession SessionListWidget::sessionAt(int row) const {
    QListWidgetItem* it = item(row);
    auto* w = it ? static_cast<SessionItemWidget*>(itemWidget(it)) : nullptr;
    return w ? w->currentSession() : QtSession();
}

QListWidgetItem* SessionListWidget::findItem(qint64 targetId, int targetType) {
    for (int i = 0; i < count(); ++i) {
        auto* w = static_cast<SessionItemWidget*>(itemWidget(item(i)));
        if (w && w->targetId() == targetId && w->targetType() == targetType) return item(i);
    }
    return nullptr;
}

int SessionListWidget::rowOfSession(qint64 targetId, int targetType) const {
    for (int i = 0; i < count(); ++i) {
        auto* w = static_cast<SessionItemWidget*>(itemWidget(item(i)));
        if (w && w->targetId() == targetId && w->targetType() == targetType) return i;
    }
    return -1;
}
