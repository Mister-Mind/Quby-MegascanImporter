#pragma once
#include <QString>
#include <QUrl>

namespace Policy {
bool allowNavigation(const QUrl &url);
QString safeFileName(QString name);
bool supportedDownload(const QString &name);
QString allocatePath(const QString &directory, const QString &name, QString *error);
}
