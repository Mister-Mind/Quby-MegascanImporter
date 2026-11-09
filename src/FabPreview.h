#pragma once
#include <QObject>
#include <QUrl>
#include <functional>

class QNetworkAccessManager;

namespace FabPreview {
bool isListing(const QUrl &url);
bool isImage(const QUrl &url);
QString pathFor(const QString &assetPath);
void save(QNetworkAccessManager &network, const QUrl &url, const QString &assetPath,
          std::function<void()> done);
}
