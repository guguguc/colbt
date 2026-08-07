#include "ui/avatar.h"

#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QFileInfo>

static const QColor kAvatarColors[] = {
    QColor("#5B8FF9"), QColor("#61DAA7"), QColor("#F6BD16"), QColor("#7262FD"),
    QColor("#78D3F8"), QColor("#9661BC"), QColor("#F6903D"), QColor("#008685"),
};

QPixmap makeAvatar(const QString& name, const QString& avatarPath, int size) {
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);

    QPainter painter(&pix);
    painter.setRenderHint(QPainter::Antialiasing);

    // 尝试加载本地图片
    if (!avatarPath.isEmpty() && QFileInfo::exists(avatarPath)) {
        QPixmap src(avatarPath);
        if (!src.isNull()) {
            QPixmap scaled = src.scaled(size, size, Qt::KeepAspectRatioByExpanding,
                                        Qt::SmoothTransformation);
            QPainterPath path;
            path.addEllipse(0, 0, size, size);
            painter.setClipPath(path);
            painter.drawPixmap(0, 0, size, size, scaled);
            return pix;
        }
    }

    // 首字符彩底
    QString letter = name.isEmpty() ? QStringLiteral("?") : name.left(1).toUpper();
    uint hash = qHash(name);
    QColor bg = kAvatarColors[hash % (sizeof(kAvatarColors) / sizeof(QColor))];

    QPainterPath path;
    path.addEllipse(0, 0, size, size);
    painter.fillPath(path, bg);

    QFont font;
    font.setPixelSize(size * 9 / 20);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, letter);

    return pix;
}
