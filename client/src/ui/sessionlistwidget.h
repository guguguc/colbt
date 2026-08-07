#pragma once

#include <QListWidget>

#include "app/appcontext.h"

// 会话列表项（自绘控件）
class SessionItemWidget : public QWidget {
    Q_OBJECT

public:
    explicit SessionItemWidget(const QtSession& session, QWidget* parent = nullptr);

    void setSession(const QtSession& session);
    void setFlags(bool pinned, bool muted);
    qint64 targetId() const { return session_.targetId; }
    int targetType() const { return session_.targetType; }
    const QtSession& currentSession() const { return session_; }

    QSize sizeHint() const override { return QSize(200, kRowHeight); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    static const int kRowHeight = 56;
    QtSession session_;
    bool hovered_ = false;
    bool pinned_ = false;
    bool muted_ = false;
};

// 会话列表
class SessionListWidget : public QListWidget {
    Q_OBJECT

public:
    explicit SessionListWidget(QWidget* parent = nullptr);

    void setSessions(const QVector<QtSession>& sessions);
    void upsertSession(const QtSession& session);
    void clearUnread(qint64 targetId, int targetType);
    QtSession sessionAt(int row) const;
    int rowOfSession(qint64 targetId, int targetType) const;
    // 置顶/免打扰状态
    bool isPinned(const QtSession& s) const;
    bool isMuted(const QtSession& s) const;
    void setPinned(const QtSession& s, bool on);
    void setMuted(const QtSession& s, bool on);
    void resort();

signals:
    void sessionClicked(const QtSession& session);
    void togglePinRequested(const QtSession& session, bool pinned);
    void toggleMuteRequested(const QtSession& session, bool muted);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    QString keyOf(const QtSession& s) const;
    QListWidgetItem* findItem(qint64 targetId, int targetType);
    void reorder();
    QSet<QString> pinned_;
    QSet<QString> muted_;
};
