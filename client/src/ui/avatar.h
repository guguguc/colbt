#pragma once

#include <QPixmap>
#include <QString>

// 生成圆形头像：优先加载本地图片，否则以昵称首字符绘制彩色底
QPixmap makeAvatar(const QString& name, const QString& avatarPath, int size = 40);
